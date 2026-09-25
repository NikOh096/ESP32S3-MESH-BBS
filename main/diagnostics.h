#pragma once
#include <stddef.h>
#include <stdint.h>

void diagnostics_init(void);
void diagnostics_record(const char *format, ...) __attribute__((format(printf, 1, 2)));
void diagnostics_flush(void);
void diagnostics_tick(void);
size_t diagnostics_report(char *out, size_t capacity);
void diagnostics_print(void);
void diagnostics_set_node(uint32_t node);
