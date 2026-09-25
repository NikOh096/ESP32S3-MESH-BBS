#include "settings.h"
#include "nvs.h"
#include <string.h>

bool settings_load(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->pin = -3; s->hops = 3;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("meshbbs", NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) return true;
    if (err != ESP_OK) return false;
    size_t size = sizeof(s->target);
    nvs_get_str(nvs, "target", s->target, &size);
    nvs_get_i32(nvs, "pin", &s->pin);
    nvs_get_u8(nvs, "hops", &s->hops);
    nvs_close(nvs);
    s->target[sizeof(s->target) - 1] = 0;
    return (s->pin == -3 || s->pin >= -1) && s->pin <= 999999 && s->hops <= 7;
}

bool settings_save(const settings_t *s)
{
    nvs_handle_t nvs;
    if (nvs_open("meshbbs", NVS_READWRITE, &nvs) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(nvs, "target", s->target);
    if (err == ESP_OK) err = nvs_set_i32(nvs, "pin", s->pin);
    if (err == ESP_OK) err = nvs_set_u8(nvs, "hops", s->hops);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err == ESP_OK;
}
