#include "diagnostics.h"
#include "activity.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static activity_t activity;
static bool dirty;
static uint32_t last_node;
static int64_t last_saved;
static const char* TAG = "activity";

void diagnostics_init(void)
{
    activity_init(&activity);
    nvs_handle_t handle;
    if (nvs_open_from_partition("bbs", "diagnostics", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(activity);
        if (nvs_get_blob(handle, "recent", &activity, &size) != ESP_OK || size != sizeof(activity) || !activity_valid(&activity))
            activity_init(&activity);
        nvs_get_u32(handle, "node", &last_node);
        nvs_close(handle);
    }
}
void diagnostics_record(const char* format, ...)
{
    char line[ACTIVITY_LINE_SIZE];
    unsigned seconds = (unsigned)(esp_timer_get_time() / 1000000);
    int offset = snprintf(line, sizeof(line), "+%us ", seconds);
    va_list args;
    va_start(args, format);
    vsnprintf(line + offset, sizeof(line) - offset, format, args);
    va_end(args);
    /* Keep message previews valid UTF-8 when bounded log lines cut a codepoint. */
    size_t length = strlen(line), start = length;
    if (length) {
        start = length - 1;
        while (start && ((unsigned char)line[start] & 0xc0) == 0x80)
            --start;
        unsigned char lead = (unsigned char)line[start];
        unsigned width = lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3
            : lead >= 0xc0                               ? 2
                                                         : 1;
        if (length - start < width)
            line[start] = 0;
    }
    for (char* p = line; *p; p++)
        if (*p == '\n' || *p == '\r' || *p == '\t')
            *p = ' ';
    activity_add(&activity, line);
    dirty = true;
    ESP_LOGI(TAG, "%s", line);
}
void diagnostics_flush(void)
{
    if (!dirty)
        return;
    nvs_handle_t handle;
    esp_err_t rc = nvs_open_from_partition("bbs", "diagnostics", NVS_READWRITE, &handle);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(handle, "recent", &activity, sizeof(activity));
        if (rc == ESP_OK)
            rc = nvs_set_u32(handle, "node", last_node);
        if (rc == ESP_OK)
            rc = nvs_commit(handle);
        nvs_close(handle);
    }
    last_saved = esp_timer_get_time();
    if (rc == ESP_OK)
        dirty = false;
    else
        ESP_LOGW(TAG, "Activity checkpoint failed: %s", esp_err_to_name(rc));
}
void diagnostics_tick(void)
{
    if (dirty && esp_timer_get_time() - last_saved >= 30000000)
        diagnostics_flush();
}
size_t diagnostics_report(char* out, size_t capacity)
{
    if (capacity < 80)
        return 0;
    int n = last_node ? snprintf(out, capacity, "Last BBS node !%08" PRIx32 "; DM commands to this ID.\n", last_node) : snprintf(out, capacity, "Node ID not captured yet; save settings and start BBS.\n");
    if (!activity.count)
        return n + snprintf(out + n, capacity - n, "No BBS events saved yet.\n");
    return (size_t)n + activity_text(&activity, out + n, capacity - n);
}
void diagnostics_print(void)
{
    static char text[ACTIVITY_CAPACITY * ACTIVITY_LINE_SIZE + 81];
    diagnostics_report(text, sizeof(text));
    printf("BBS_ACTIVITY_BEGIN\n%sBBS_ACTIVITY_END\n", text);
}
void diagnostics_set_node(uint32_t node)
{
    if (node != last_node) {
        last_node = node;
        dirty = true;
    }
}
