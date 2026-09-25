#pragma once
#include <stdbool.h>
#include <stdint.h>
#define NODE_SCAN_LIMIT 32
enum { NODE_PROTOCOL_UNKNOWN, NODE_PROTOCOL_MESHTASTIC, NODE_PROTOCOL_MESHCORE };
typedef struct {
    char address[18], name[64];
    uint8_t address_type;
    int rssi;
    bool connectable;
    uint8_t protocol;
    bool service_seen;
    int64_t seen_ms;
} node_scan_entry_t;
typedef struct {
    unsigned count;
    node_scan_entry_t entries[NODE_SCAN_LIMIT];
} node_scan_t;
void node_scan_clear(node_scan_t*);
bool node_scan_record(node_scan_t*, const char* address, uint8_t type, const char* name,
    unsigned protocol, int connectable, int rssi, int64_t now_ms);
bool node_scan_address(const char*);
const node_scan_entry_t* node_scan_find(const node_scan_t*, const char*, int64_t now_ms);
