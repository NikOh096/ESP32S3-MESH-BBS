#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BAN_LIMIT 64
#define BAN_MESSAGE_MAX 120
typedef struct {
    uint32_t version, count;
    uint32_t nodes[BAN_LIMIT];
    char message[BAN_MESSAGE_MAX + 1];
} bans_t;
void bans_init(bans_t*);
bool bans_valid(const bans_t*);
bool bans_contains(const bans_t*, uint32_t node);
bool bans_set(bans_t*, uint32_t node, bool add);
bool bans_message(bans_t*, const char*);
bool bans_parse_node(const char*, uint32_t*);
