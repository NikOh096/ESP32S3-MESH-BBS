#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bbs.h"
#include "mesh_protocol.h"
#include "setup_protocol.h"
#include "activity.h"
#include "pb_encode.h"
#include "pb_decode.h"

static bbs_post_t disk[BBS_CAPACITY];
static bool disk_fail;
static unsigned writes;
static bool save(unsigned slot, const bbs_post_t *p, void *ctx)
{
    (void)ctx;
    if (disk_fail) return false;
    disk[slot] = *p; ++writes; return true;
}
static bbs_t b;
static bbs_request_t author = {.sender = 0x12345678, .packet_id = 1, .authenticated = true, .key = {1,2,3}};
static char reply[BBS_REPLY_MAX + 1];

static void command(const char *s)
{
    memset(reply, 0xcc, sizeof(reply));
    assert(bbs_handle(&b, &author, s, reply));
    assert(memchr(reply, 0, sizeof(reply)));
    assert(strlen(reply) <= BBS_REPLY_MAX);
}

static void test_board(void)
{
    bbs_init(&b, save, NULL);
    command("LIST"); assert(strstr(reply, "No posts"));
    command("HELP"); assert(strstr(reply, "PUBLIC"));
    command("POST hello mesh"); assert(strstr(reply, "#1 saved")); assert(bbs_count(&b) == 1);
    unsigned before = writes;
    command("POST hello mesh"); assert(strstr(reply, "already saved")); assert(writes == before);
    command("READ 1"); assert(strstr(reply, "hello mesh"));
    bbs_init(&b, save, NULL);
    assert(bbs_restore(&b, 0, &disk[0]));
    command("READ 1"); assert(strstr(reply, "hello mesh"));
    command("POST hello mesh"); assert(bbs_count(&b) == 1);
    author.sender++; command("DEL 1"); assert(strstr(reply, "requires")); assert(bbs_count(&b) == 1);
    author.sender--; author.key[0]++; command("DEL 1"); assert(strstr(reply, "requires"));
    author.key[0]--; author.authenticated = false; command("DEL 1"); assert(strstr(reply, "requires"));
    author.authenticated = true;
    disk_fail = true; command("DEL 1"); assert(strstr(reply, "NOT saved")); assert(bbs_count(&b) == 1);
    author.packet_id++; command("POST cannot save"); assert(strstr(reply, "NOT saved")); assert(bbs_count(&b) == 1);
    disk_fail = false; command("DEL 1"); assert(strstr(reply, "Deleted")); assert(!bbs_count(&b));
    bbs_init(&b, save, NULL); assert(bbs_restore(&b, 0, &disk[0]));
    assert(b.next_id == 2); command("READ 1"); assert(strstr(reply, "not found"));
    for (unsigned i = 0; i < 70; ++i) {
        author.packet_id++;
        command("POST retained post");
    }
    assert(bbs_count(&b) == BBS_CAPACITY);
    command("READ 2"); assert(strstr(reply, "not found"));
    command("LIST"); assert(strstr(reply, "#71"));
    command("LIST 22"); assert(strstr(reply, "22/22"));
    command("LIST 23"); assert(strstr(reply, "Choose LIST"));
    command("LIST 4294967296"); assert(strstr(reply, "Use LIST"));
    command("READ -1"); assert(strstr(reply, "Use READ"));
    command("!bbs"); assert(strstr(reply, "HELP"));
    command("!bbs whatever"); assert(strstr(reply, "Unknown"));
    assert(!bbs_handle(&b, &author, "BBS: reply from another bot", reply));
    assert(!bbs_handle(&b, &author, "normal chat", reply));
    char maxpost[6 + BBS_BODY_MAX + 2] = "POST ";
    memset(maxpost + 5, 'x', BBS_BODY_MAX); maxpost[5 + BBS_BODY_MAX] = 0;
    author.packet_id++; command(maxpost); assert(strstr(reply, "saved"));
    char read_cmd[32]; snprintf(read_cmd, sizeof(read_cmd), "READ %u", (unsigned)b.next_id - 1);
    command(read_cmd); assert(strlen(reply) <= BBS_REPLY_MAX);
    maxpost[5 + BBS_BODY_MAX] = 'x'; maxpost[6 + BBS_BODY_MAX] = 0;
    author.packet_id++; command(maxpost); assert(strstr(reply, "1-160"));
    bbs_post_t corrupt = disk[0]; corrupt.version = 999; assert(!bbs_restore(&b, 0, &corrupt));
    corrupt = disk[0]; memset(corrupt.body, 'x', sizeof(corrupt.body)); assert(!bbs_restore(&b, 0, &corrupt));
    assert(!bbs_restore(&b, BBS_CAPACITY, &disk[0]));
    assert(bbs_delete_local(&b, b.next_id - 1));

    command("Ping"); assert(strstr(reply, "PONG"));
    command("HELP READ"); assert(strstr(reply, "READ 12"));
    command("HELP POST"); assert(strstr(reply, "PUBLIC") && strstr(reply, "160"));
    command("HELP DEL"); assert(strstr(reply, "same Meshtastic node"));
    command("Hello"); assert(strstr(reply, "LIST"));
    command("!BBS help"); assert(strstr(reply, "LIST"));
    command("L"); assert(strstr(reply, "retained post"));
    assert(!bbs_is_command("unrelated chat"));
    assert(!bbs_is_command("BBS: reply from another bot"));
    assert(bbs_is_command(" !BbS help") && bbs_is_command("h") && bbs_is_command("PING"));

    /* LIST worst-case IDs and UTF-8 snippets must fit a single radio response. */
    bbs_init(&b, save, NULL);
    bbs_post_t longpost = {.version = BBS_STORE_VERSION, .author = UINT32_MAX - 1};
    for (unsigned j = 0; j < 39; ++j) memcpy(longpost.body + j * 4, "\xf0\x9f\x93\xbb", 4);
    for (unsigned i = 0; i < 4; ++i) {
        longpost.id = UINT32_MAX - 1 - i;
        assert(bbs_restore(&b, i, &longpost));
    }
    command("LIST"); assert(strlen(reply) <= BBS_REPLY_MAX && strstr(reply, "LIST 2"));
    meshtastic_MeshPacket preview_packet = meshtastic_MeshPacket_init_zero;
    preview_packet.from = 123; preview_packet.to = 456; preview_packet.id = 1;
    preview_packet.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    preview_packet.decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    preview_packet.decoded.payload.size = (pb_size_t)strlen(reply);
    memcpy(preview_packet.decoded.payload.bytes, reply, strlen(reply));
    mesh_request_t preview_req;
    assert(mesh_extract(&preview_packet, 456, &preview_req)); /* validates UTF-8 boundaries */
}

static void test_activity(void)
{
    activity_t log; activity_init(&log);
    char text[513], line[50];
    assert(activity_valid(&log) && !activity_text(&log, text, sizeof(text)));
    for (unsigned i = 0; i < 40; ++i) {
        snprintf(line, sizeof(line), "event %u command/queue/ACK", i); activity_add(&log, line);
    }
    assert(log.count == 32 && activity_valid(&log));
    size_t n = activity_text(&log, text, sizeof(text));
    assert(n < sizeof(text) && strlen(text) == n && strstr(text, "event 39 command"));
    assert(!strstr(text, "event 7 command"));
    assert(text[n - 1] == '\n' && !strncmp(text, "event ", 6));
    activity_t restored = log;
    char saved[513]; assert(activity_text(&restored, saved, sizeof(saved)) == n && !strcmp(text, saved));
    assert(!activity_text(&log, text, 1) && text[0] == 0);
    restored.next = 32; assert(!activity_valid(&restored));
    restored = log; memset(restored.lines[0], 'x', ACTIVITY_LINE_SIZE); assert(!activity_valid(&restored));
}

static void test_protocol(void)
{
    uint8_t bytes[MESH_WIRE_MAX];
    /* Independent known protobuf vectors: ToRadio.want_config_id=150. */
    size_t n = mesh_config(150, bytes, sizeof(bytes));
    const uint8_t cfg[] = {0x18, 0x96, 0x01};
    assert(n == sizeof(cfg) && !memcmp(bytes, cfg, n));
    n = mesh_heartbeat(bytes, sizeof(bytes));
    const uint8_t heartbeat[] = {0x3a, 0x00};
    assert(n == sizeof(heartbeat) && !memcmp(bytes, heartbeat, n));

    meshtastic_FromRadio incoming = meshtastic_FromRadio_init_zero, decoded;
    incoming.which_payload_variant = meshtastic_FromRadio_packet_tag;
    meshtastic_MeshPacket *p = &incoming.packet;
    p->from = 123; p->to = 456; p->id = 777; p->channel = 2;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.payload.size = 4; memcpy(p->decoded.payload.bytes, "HELP", 4);
    p->pki_encrypted = true; p->public_key.size = 32; memset(p->public_key.bytes, 42, 32);
    pb_ostream_t os = pb_ostream_from_buffer(bytes, sizeof(bytes));
    assert(pb_encode(&os, meshtastic_FromRadio_fields, &incoming));
    assert(mesh_decode(bytes, os.bytes_written, &decoded));
    mesh_request_t req;
    assert(mesh_extract(&decoded.packet, 456, &req));
    assert(!strcmp(req.text, "HELP")); assert(req.request.authenticated);
    assert(!mesh_extract(p, 0, &req));
    p->to = UINT32_MAX; assert(!mesh_extract(p, 456, &req)); p->to = 456;
    p->from = 456; assert(!mesh_extract(p, 456, &req)); p->from = 123;
    p->decoded.portnum = meshtastic_PortNum_TELEMETRY_APP;
    assert(!mesh_extract(p, 456, &req)); p->decoded.portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->decoded.payload.bytes[1] = 0; assert(!mesh_extract(p, 456, &req));
    memcpy(p->decoded.payload.bytes, "\xf0\x80\x80\x80", 4); assert(!mesh_extract(p, 456, &req));
    memcpy(p->decoded.payload.bytes, "HELP", 4);
    assert(mesh_extract(p, 456, &req));
    n = mesh_reply(&req, "BBS: test", 12345, 3, bytes, sizeof(bytes));
    assert(n > 0 && n <= 512);
    meshtastic_ToRadio tx = meshtastic_ToRadio_init_zero;
    pb_istream_t is = pb_istream_from_buffer(bytes, n);
    assert(pb_decode(&is, meshtastic_ToRadio_fields, &tx));
    assert(tx.which_payload_variant == meshtastic_ToRadio_packet_tag);
    assert(tx.packet.to == 123 && tx.packet.from == 0 && tx.packet.channel == 2);
    assert(tx.packet.id == 12345 && tx.packet.hop_limit == 3 && tx.packet.want_ack);
    assert(tx.packet.decoded.reply_id == 777 && !tx.packet.decoded.want_response);
    assert(tx.packet.pki_encrypted && tx.packet.public_key.size == 32);
    assert(tx.packet.public_key.bytes[0] == 42);

    /* Node routing acknowledgments refer to our reply ID through request_id. */
    meshtastic_MeshPacket ack = meshtastic_MeshPacket_init_zero;
    ack.from = 123; ack.to = 456;
    ack.which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    ack.decoded.portnum = meshtastic_PortNum_ROUTING_APP;
    ack.decoded.request_id = 12345;
    ack.decoded.payload.size = 2;
    ack.decoded.payload.bytes[0] = 0x18; ack.decoded.payload.bytes[1] = 0;
    uint32_t ack_id; meshtastic_Routing_Error ack_error;
    assert(mesh_routing(&ack, 456, &ack_id, &ack_error));
    assert(ack_id == 12345 && ack_error == meshtastic_Routing_Error_NONE);
    ack.decoded.payload.bytes[1] = 39;
    assert(mesh_routing(&ack, 456, &ack_id, &ack_error));
    assert(!strcmp(mesh_routing_error(ack_error), "PKI_SEND_NO_KEY"));
    assert(!mesh_routing(&ack, 789, &ack_id, &ack_error));
    ack.decoded.request_id = 0; assert(!mesh_routing(&ack, 456, &ack_id, &ack_error));
    ack.decoded.request_id = 12345; ack.decoded.payload.size = 1;
    assert(!mesh_routing(&ack, 456, &ack_id, &ack_error));
    ack.decoded.payload.size = 0; assert(!mesh_routing(&ack, 456, &ack_id, &ack_error));
    ack.decoded.payload.size = 238; assert(!mesh_routing(&ack, 456, &ack_id, &ack_error));
    assert(!mesh_reply(&req, "", 12345, 3, bytes, sizeof(bytes)));
    assert(!mesh_reply(&req, "BBS", 0, 3, bytes, sizeof(bytes)));
    char long_reply[BBS_REPLY_MAX + 2]; memset(long_reply, 'a', sizeof(long_reply) - 1); long_reply[sizeof(long_reply) - 1] = 0;
    assert(!mesh_reply(&req, long_reply, 1, 3, bytes, sizeof(bytes)));
    const uint8_t malformed[] = {0x12, 0xff, 0xff, 0xff};
    assert(!mesh_decode(malformed, sizeof(malformed), &decoded));
    uint32_t seed = 17;
    for (unsigned i = 0; i < 2000; ++i) {
        n = i % 300 + 1;
        for (unsigned j = 0; j < n; ++j) { seed = seed * 1664525u + 1013904223u; bytes[j] = seed >> 24; }
        mesh_decode(bytes, n, &decoded); /* Bounded malformed-input stress test. */
    }
}

int main(void)
{
    void test_threads(void);
    test_threads();
    settings_t current = {.pin = 123456, .hops = 3}, decoded = {0};
    /* Golden vector shared with the Android wire codec test. */
    uint8_t config_wire[] = {1,3,4,0x40,0xe2,1,0,'T','e','s','t'};
    assert(setup_decode(config_wire, sizeof(config_wire), &current, &decoded));
    assert(decoded.pin == 123456 && decoded.hops == 3 && !strcmp(decoded.target, "Test"));
    config_wire[3] = 0xfe; config_wire[4] = config_wire[5] = config_wire[6] = 0xff;
    assert(setup_decode(config_wire, sizeof(config_wire), &current, &decoded) && decoded.pin == 123456);
    config_wire[3] = 0xff;
    assert(setup_decode(config_wire, sizeof(config_wire), &current, &decoded) && decoded.pin == -1);
    config_wire[3] = 0xfd;
    assert(!setup_decode(config_wire, sizeof(config_wire), &current, &decoded));
    config_wire[3] = 0xff; config_wire[1] = 8;
    assert(!setup_decode(config_wire, sizeof(config_wire), &current, &decoded));
    config_wire[1] = 3; config_wire[7] = 0;
    assert(!setup_decode(config_wire, sizeof(config_wire), &current, &decoded));
    config_wire[7] = 0xc0;
    assert(!setup_decode(config_wire, sizeof(config_wire), &current, &decoded));
    config_wire[7] = 'T';
    for (size_t i = 0; i < sizeof(config_wire); ++i) assert(!setup_decode(config_wire, i, &current, &decoded));
    strcpy(current.target, "Test");
    uint8_t status_wire[70] = {0};
    assert(setup_status(&current, true, status_wire, sizeof(status_wire)) == 8);
    assert(!memcmp(status_wire, "\x01\x03\x03\x04Test", 8));
    assert(setup_status(&current, true, status_wire, 7) == 0);
    extern void test_node_scan(void);
    test_node_scan(); test_board(); test_protocol(); test_activity();
    puts("PASS: persistence/restart, retention, duplicate POST, authorization, storage failures, bounds, protobuf vectors, routing, UTF-8 and 2000 malformed inputs");
    return 0;
}
