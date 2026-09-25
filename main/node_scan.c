#include "node_scan.h"
#include "bulletins.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
void node_scan_clear(node_scan_t* s) { memset(s, 0, sizeof(*s)); }
bool node_scan_address(const char* address)
{
    if (!address || strlen(address) != 17)
        return false;
    for (unsigned i = 0; i < 17; i++)
        if (i % 3 == 2 ? address[i] != ':' : !isxdigit((unsigned char)address[i]))
            return false;
    return true;
}
static bool prefix_is(const char* name, const char* prefix)
{
    for (unsigned i = 0; prefix[i]; i++)
        if (tolower((unsigned char)name[i]) != prefix[i])
            return false;
    return true;
}
bool node_scan_record(node_scan_t* s, const char* address, uint8_t type, const char* name,
    unsigned protocol, int connectable, int rssi, int64_t now_ms)
{
    if (!node_scan_address(address) || type > 1 || protocol > NODE_PROTOCOL_MESHCORE || !name || (*name && !bulletin_text_valid(name, 63)))
        return false;
    unsigned candidate = protocol ? protocol : prefix_is(name, "meshtastic") ? NODE_PROTOCOL_MESHTASTIC
                                            : prefix_is(name, "meshcore") ? NODE_PROTOCOL_MESHCORE : NODE_PROTOCOL_UNKNOWN;
    unsigned index = s->count;
    for (unsigned i = 0; i < s->count; i++)
        if (!strcmp(s->entries[i].address, address) && s->entries[i].address_type == type) {
            index = i;
            break;
        }
    if (index == s->count) {
        if (!candidate || s->count == NODE_SCAN_LIMIT)
            return false;
        ++s->count;
    }
    node_scan_entry_t* e = &s->entries[index];
    strcpy(e->address, address);
    e->address_type = type;
    if (protocol || !e->service_seen)
        if (candidate)
            e->protocol = (uint8_t)candidate;
    e->service_seen |= protocol != 0;
    if (*name)
        strcpy(e->name, name); /* A later scan response supplies the full name. */
    if (connectable >= 0)
        e->connectable = connectable != 0;
    e->rssi = rssi;
    e->seen_ms = now_ms;
    return true;
}
const node_scan_entry_t* node_scan_find(const node_scan_t* s, const char* address, int64_t now_ms)
{
    if (!node_scan_address(address))
        return NULL;
    for (unsigned i = 0; i < s->count; i++)
        if (!strcmp(s->entries[i].address, address) && s->entries[i].connectable && now_ms >= s->entries[i].seen_ms && now_ms - s->entries[i].seen_ms <= 120000)
            return &s->entries[i];
    return NULL;
}
