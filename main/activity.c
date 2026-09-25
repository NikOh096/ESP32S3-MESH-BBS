#include "activity.h"
#include <stdio.h>
#include <string.h>

void activity_init(activity_t *log) { memset(log, 0, sizeof(*log)); log->version = 1; }
int activity_valid(const activity_t *log)
{
    if (log->version != 1 || log->count > ACTIVITY_CAPACITY || log->next >= ACTIVITY_CAPACITY) return 0;
    for (unsigned i = 0; i < ACTIVITY_CAPACITY; ++i)
        if (!memchr(log->lines[i], 0, ACTIVITY_LINE_SIZE)) return 0;
    return 1;
}
void activity_add(activity_t *log, const char *line)
{
    snprintf(log->lines[log->next], ACTIVITY_LINE_SIZE, "%s", line);
    log->next = (log->next + 1) % ACTIVITY_CAPACITY;
    if (log->count < ACTIVITY_CAPACITY) ++log->count;
}
size_t activity_text(const activity_t *log, char *out, size_t capacity)
{
    if (!capacity) return 0;
    out[0] = 0;
    if (!activity_valid(log)) return 0;
    unsigned count = 0;
    size_t bytes = 0;
    while (count < log->count) {
        unsigned index = (log->next + ACTIVITY_CAPACITY - 1 - count) % ACTIVITY_CAPACITY;
        size_t n = strlen(log->lines[index]) + 1;
        if (bytes + n >= capacity) break;
        bytes += n; ++count;
    }
    size_t used = 0;
    while (count) {
        unsigned index = (log->next + ACTIVITY_CAPACITY - count--) % ACTIVITY_CAPACITY;
        size_t n = strlen(log->lines[index]);
        memcpy(out + used, log->lines[index], n); used += n; out[used++] = '\n';
    }
    out[used] = 0;
    return used;
}
