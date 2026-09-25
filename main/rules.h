#pragma once
#include <stdbool.h>
#include <stdint.h>
#define RULE_COUNT 6
#define RULE_BYTES 160
typedef struct {
    uint32_t version;
    char lines[RULE_COUNT][RULE_BYTES + 1];
} rules_t;
void rules_init(rules_t*);
bool rules_valid(const rules_t*);
bool rules_set(rules_t*, unsigned index, const char* text);
