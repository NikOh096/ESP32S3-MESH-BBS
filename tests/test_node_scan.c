#include "node_scan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void test_node_scan(void)
{
    node_scan_t s;
    node_scan_clear(&s);
    const char* a = "9C:13:9E:A1:4B:59";
    assert(!node_scan_record(&s, a, 0, "Unrelated headset", false, 1, -40, 0));
    assert(!node_scan_record(&s, "bad", 0, "Meshtastic", true, 1, -40, 0));
    assert(node_scan_record(&s, a, 0, "", true, 1, -40, 10));
    assert(node_scan_record(&s, a, 0, "Meshtastic_4b58", false, -1, -42, 20));
    assert(s.count == 1 && node_scan_find(&s, a, 30));
    assert(!strcmp(s.entries[0].name, "Meshtastic_4b58"));
    assert(node_scan_record(&s, a, 0, "", true, 1, -43, 40));
    assert(!strcmp(s.entries[0].name, "Meshtastic_4b58"));
    assert(!node_scan_find(&s, a, 120041));
    assert(!node_scan_find(&s, a, 39));
    assert(!node_scan_record(&s, a, 0, "\xc0\xaf", true, 1, -50, 50));
    assert(!node_scan_record(&s, a, 2, "Meshtastic", true, 1, -50, 50));
    node_scan_clear(&s);
    assert(node_scan_record(&s, a, 1, "Meshtastic_4b58", false, -1, -50, 50));
    assert(!node_scan_find(&s, a, 50));
    assert(node_scan_record(&s, a, 1, "", false, 1, -50, 60));
    assert(node_scan_find(&s, a, 60));
    assert(s.entries[0].address_type == 1);
    assert(node_scan_record(&s, a, 1, "", false, 0, -50, 70));
    assert(!node_scan_find(&s, a, 70));
    node_scan_clear(&s);
    for (unsigned i = 0; i < NODE_SCAN_LIMIT; i++) {
        char address[18];
        snprintf(address, sizeof(address), "00:00:00:00:00:%02X", i);
        assert(node_scan_record(&s, address, 0, "Meshtastic", false, 1, -50, 10));
    }
    assert(!node_scan_record(&s, a, 0, "Meshtastic", true, 1, -50, 10));
    assert(s.count == NODE_SCAN_LIMIT);
    node_scan_clear(&s);
    assert(node_scan_record(&s, a, 0, "MeshCore_RAK", 0, 1, -40, 1));
    assert(s.entries[0].protocol == NODE_PROTOCOL_MESHCORE && !s.entries[0].service_seen);
    assert(node_scan_record(&s, a, 0, "MeshCore renamed", NODE_PROTOCOL_MESHTASTIC, 1, -40, 2));
    assert(s.entries[0].protocol == NODE_PROTOCOL_MESHTASTIC && s.entries[0].service_seen);
    assert(node_scan_record(&s, a, 0, "MeshCore renamed", 0, -1, -40, 3));
    assert(s.entries[0].protocol == NODE_PROTOCOL_MESHTASTIC);
    assert(!node_scan_record(&s, a, 0, "MeshCore", 99, 1, -40, 4));
    puts("PASS: radio scan filtering, merged scan responses, connectability, address types, stale results, UTF-8 and bounded capacity");
}
