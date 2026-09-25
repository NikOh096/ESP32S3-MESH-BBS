#pragma once
#include <stddef.h>
#include <stdint.h>

#define ACTIVITY_CAPACITY 32
#define ACTIVITY_LINE_SIZE 96
typedef struct {
    uint32_t version, count, next;
    char lines[ACTIVITY_CAPACITY][ACTIVITY_LINE_SIZE];
} activity_t;
void activity_init(activity_t *log);
void activity_add(activity_t *log, const char *line);
int activity_valid(const activity_t *log);
/* Most recent complete lines that fit, in chronological order. */
size_t activity_text(const activity_t *log, char *out, size_t capacity);
