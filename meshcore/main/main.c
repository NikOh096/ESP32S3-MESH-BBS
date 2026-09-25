#include "bans.h"
#include "ble_link.h"
#include "bulletins.h"
#include "cJSON.h"
#include "diagnostics.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "meshcore_ids.h"
#include "meshcore_protocol.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "provision.h"
#include "sdkconfig.h"
#include "settings.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
static const char* TAG = "MESHBBS";
static bulletins_t board;
static bans_t bans;
static bool bans_ok = true;
static settings_t settings;
static nvs_handle_t store;
static bool storage_ok, ble_up, api_ready;
static int node_hw, node_pin_mode = -1;
static char node_hardware[41];
static uint32_t own_node, revision = 1;
typedef struct {
    bbs_request_t request;
    char text[MC_TEXT_MAX + 1];
} mesh_request_t;
static mc_ids_t identities;
static nvs_handle_t identity_store;
static bool identities_ok;
static unsigned command_pending, sync_step;
static int64_t command_deadline, next_pull, next_contacts;
static bool refresh_contacts, deferred_valid;
static mc_packet_t deferred_message;
static uint32_t outbound_timestamp;
static uint8_t self_key[32];
static char node_version[32] = "unknown";
static int64_t sync_started, text_sent;
static struct {
    uint32_t sender, id;
    int64_t at;
} seen[128];
static unsigned seen_next;
static struct {
    uint32_t sender;
    int64_t at;
} users[32];
static unsigned user_next;
static struct {
    uint32_t id;
    char name[THREAD_NAME + 1];
} names[MC_ID_LIMIT];
static unsigned name_next;
static struct {
    mesh_request_t request;
    bulletin_reply_t reply;
    unsigned next;
    int64_t created;
}* jobs;
static unsigned job_head, job_count;
static struct {
    uint32_t id, to;
    int64_t sent;
    bool reported;
} pending[32];
static unsigned pending_next;
static unsigned received, texts, commands, submitted, acks, rejected;
static uint8_t wire[MC_FRAME_MAX];
static mc_packet_t radio;
static link_event_t event;
static uint32_t utc(void)
{
    time_t t = time(NULL);
    return t >= 1700000000 && t <= UINT32_MAX ? (uint32_t)t : 0;
}
static bool set_clock(uint32_t t)
{
    if (t < 1700000000)
        return false;
    struct timeval tv = { .tv_sec = t };
    return settimeofday(&tv, NULL) == 0;
}
static bool persist(unsigned slot, const bulletin_t* t, void* ctx)
{
    (void)ctx;
    if (!storage_ok)
        return false;
    char key[12];
    snprintf(key, sizeof(key), "thread%u", slot);
    esp_err_t rc = nvs_set_blob(store, key, t, sizeof(*t));
    if (rc == ESP_OK)
        rc = nvs_commit(store);
    if (rc != ESP_OK)
        ESP_LOGE(TAG, "Bulletin save failed: %s", esp_err_to_name(rc));
    else
        ++revision;
    return rc == ESP_OK;
}
static bool persist_queue(const bulletin_queue_t* q, void* ctx)
{
    (void)ctx;
    if (!storage_ok)
        return false;
    esp_err_t rc = nvs_set_blob(store, "waiting", q, sizeof(*q));
    if (rc == ESP_OK)
        rc = nvs_commit(store);
    if (rc == ESP_OK)
        ++revision;
    return rc == ESP_OK;
}
static bool maintain_threads(void)
{
    if (!storage_ok || !bans_ok)
        return false;
    for (unsigned i = 0; i < bans.count; i++)
        if (!bulletins_ban_pending(&board, bans.nodes[i]))
            return false;
    return bulletins_tick(&board, utc());
}
static bool save_bans(const bans_t* next)
{
    if (!storage_ok || !bans_ok || !bans_valid(next))
        return false;
    esp_err_t rc = nvs_set_blob(store, "bans", next, sizeof(*next));
    if (rc == ESP_OK)
        rc = nvs_commit(store);
    if (rc != ESP_OK)
        return false;
    bans = *next;
    ++revision;
    for (unsigned n = 0; n < job_count; n++) {
        unsigned i = (job_head + n) % 2;
        if (bans_contains(&bans, jobs[i].request.request.sender)) {
            jobs[i].reply.count = 1;
            jobs[i].next = 0;
            strcpy(jobs[i].reply.text[0], bans.message);
        }
    }
    return true;
}
static void status(void)
{
    ESP_LOGI(TAG, "STATUS target='%s' pairing=%s BLE=%s API=%s node=!%08" PRIx32 " version=%s storage=%s owner=%s time=%" PRIu32,
        settings.target, ble_link_pair_state(), ble_up ? "up" : "down", api_ready ? "ready" : "waiting", own_node, node_version,
        storage_ok ? "OK" : "ERROR", provision_authorized() ? "connected" : "offline", utc());
    ESP_LOGI(TAG, "COUNTERS received=%u text=%u commands=%u replies=%u acks=%u rejected=%u", received, texts, commands, submitted, acks, rejected);
}
static void console_poll(void)
{
    static char lines[2][96];
    static unsigned sizes[2];
    for (unsigned port = 0; port < 2; port++) {
        uint8_t in[64];
        int n = port ? usb_serial_jtag_read_bytes(in, sizeof(in), 0) : uart_read_bytes(UART_NUM_0, in, sizeof(in), 0);
        for (int i = 0; i < n; i++)
            if (in[i] == '\r' || in[i] == '\n') {
                lines[port][sizes[port]] = 0;
                if (!strcmp(lines[port], "STATUS"))
                    status();
                else if (!strcmp(lines[port], "ACTIVITY")) {
                    diagnostics_flush();
                    diagnostics_print();
                } else if (sizes[port])
                    ESP_LOGI(TAG, "USB is read-only: STATUS / ACTIVITY. Administration requires the owner app.");
                sizes[port] = 0;
            } else if (in[i] >= 32 && sizes[port] < sizeof(lines[port]) - 1)
                lines[port][sizes[port]++] = in[i];
    }
}
static bool limited(const mesh_request_t* r, int64_t now)
{
    for (unsigned i = 0; i < 128; i++)
        if (seen[i].sender == r->request.sender && seen[i].id == r->request.packet_id && now - seen[i].at < 600000000)
            return true;
    for (unsigned i = 0; i < 32; i++)
        if (users[i].sender == r->request.sender) {
            if (now - users[i].at < 10000000)
                return true;
            users[i].at = now;
            goto accepted;
        }
    users[user_next].sender = r->request.sender;
    users[user_next].at = now;
    user_next = (user_next + 1) % 32;
accepted:
    seen[seen_next].sender = r->request.sender;
    seen[seen_next].id = r->request.packet_id;
    seen[seen_next].at = now;
    seen_next = (seen_next + 1) % 128;
    return false;
}
static const char* node_name(uint32_t id)
{
    for (unsigned i = 0; i < MC_ID_LIMIT; i++)
        if (names[i].id == id)
            return names[i].name;
    return NULL;
}
static int pending_find(uint32_t id)
{
    if (id)
        for (unsigned i = 0; i < 32; i++)
            if (pending[i].id == id)
                return i;
    return -1;
}
static bool save_identity(unsigned index, const mc_identity_t* id, void* ctx)
{
    (void)ctx;
    if (!identities_ok)
        return false;
    char key[12];
    snprintf(key, sizeof(key), "id%03u", index);
    return nvs_set_blob(identity_store, key, id, sizeof(*id)) == ESP_OK && nvs_commit(identity_store) == ESP_OK;
}
static void load_identities(void)
{
    mc_ids_init(&identities, save_identity, NULL);
    identities_ok = nvs_open_from_partition("bbs", "mc_ids", NVS_READWRITE, &identity_store) == ESP_OK;
    if (!identities_ok)
        return;
    for (unsigned i = 0; i < MC_ID_LIMIT; i++) {
        char key[12];
        snprintf(key, sizeof(key), "id%03u", i);
        mc_identity_t value;
        size_t size = sizeof(value);
        esp_err_t rc = nvs_get_blob(identity_store, key, &value, &size);
        if (rc == ESP_ERR_NVS_NOT_FOUND)
            break;
        if (rc != ESP_OK || size != sizeof(value) || !mc_ids_restore(&identities, i, &value)) {
            identities_ok = false;
            diagnostics_record("MeshCore identities invalid; visitor access disabled");
            break;
        }
    }
}
static void remember_contact(const mc_packet_t* p)
{
    uint32_t alias = identities_ok ? mc_ids_assign(&identities, p->key) : 0;
    if (!alias) {
        diagnostics_record("MeshCore contact ID storage unavailable/full");
        return;
    }
    unsigned slot = name_next;
    for (unsigned i = 0; i < MC_ID_LIMIT; i++)
        if (names[i].id == alias) {
            slot = i;
            break;
        }
    names[slot].id = alias;
    snprintf(names[slot].name, sizeof(names[slot].name), "%s", p->name);
    if (slot == name_next)
        name_next = (name_next + 1) % MC_ID_LIMIT;
}
static void process_request(const mesh_request_t* req)
{
    ++texts;
    diagnostics_record("DM received from !%08" PRIx32 " id=%" PRIu32, req->request.sender, req->request.packet_id);
    diagnostics_record("RX !%08" PRIx32 ": %.60s", req->request.sender, req->text);
    int64_t now = esp_timer_get_time();
    if (!api_ready || limited(req, now)) {
        diagnostics_record("DM deferred: API not ready, duplicate or 10s rate limit");
        return;
    }
    if (job_count == 2) {
        diagnostics_record("DM dropped: two complete thread responses already queued");
        return;
    }
    unsigned slot = (job_head + job_count) % 2;
    jobs[slot].reply.thread_id = 0;
    jobs[slot].reply.expires = 0;
    jobs[slot].request = *req;
    jobs[slot].next = 0;
    jobs[slot].created = now;
    if (!bans_ok) {
        jobs[slot].reply.count = 1;
        strcpy(jobs[slot].reply.text[0], "BBS: Moderation storage unavailable. Contact the owner.");
    } else if (bans_contains(&bans, req->request.sender)) {
        jobs[slot].reply.count = 1;
        strcpy(jobs[slot].reply.text[0], bans.message);
    } else if (!maintain_threads()) {
        jobs[slot].reply.count = 1;
        strcpy(jobs[slot].reply.text[0], "BBS: Storage maintenance unavailable. Please retry later.");
    } else
        bulletins_command(&board, &req->request, node_name(req->request.sender), req->text, utc(), &jobs[slot].reply);
    ++job_count;
    ++commands;
    diagnostics_record("DM processed; %u reply packets queued", jobs[slot].reply.count);
}
static void process_message(const mc_packet_t* p, bool may_defer)
{
    ++received;
    if (p->text_type != 0 || !p->text[0] || !identities_ok)
        return;
    const mc_identity_t* sender = mc_ids_prefix(&identities, p->prefix);
    if (!sender) {
        if (may_defer) {
            deferred_message = *p;
            deferred_valid = true;
            refresh_contacts = true;
            diagnostics_record("Refreshing contacts to resolve incoming MeshCore DM");
        } else
            diagnostics_record("DM rejected: missing or ambiguous full contact key");
        return;
    }
    if (sender->alias == own_node)
        return;
    mesh_request_t req = { 0 };
    req.request.sender = sender->alias;
    req.request.packet_id = mc_request_id(sender->key, p->timestamp, p->text);
    req.request.received_at = p->timestamp;
    req.request.authenticated = true;
    memcpy(req.request.key, sender->key, 32);
    strcpy(req.text, p->text);
    size_t n = strlen(req.text);
    while (n && req.text[n - 1] == ' ')
        req.text[--n] = 0;
    if (n)
        process_request(&req);
}
static bool send_command(unsigned command, size_t length)
{
    if (command_pending || !length || !ble_link_send(wire, length))
        return false;
    command_pending = command;
    command_deadline = esp_timer_get_time() + (command == 4 ? 45000000 : 10000000);
    return true;
}
static void process_radio(const mc_packet_t* p)
{
    if (p->type == MC_WAITING) {
        next_pull = 0;
        return;
    }
    if (p->type == MC_NEW_CONTACT) {
        remember_contact(p);
        refresh_contacts = true;
        return;
    }
    if (p->type == MC_ADVERT || p->type == MC_PATH || p->type == MC_CONTACT_DELETED) {
        refresh_contacts = true;
        return;
    }
    if (p->type == MC_CONTACTS_FULL) {
        diagnostics_record("MeshCore radio contact list full; manage it in MeshCore app");
        return;
    }
    if (p->type == MC_ACK) {
        int i = pending_find(p->ack);
        if (i >= 0 && !pending[i].reported) {
            pending[i].reported = true;
            ++acks;
            diagnostics_record("TX ACK=%08" PRIx32 " recipient confirmed", p->ack);
        }
        return;
    }
    if (p->type == MC_ERROR) {
        diagnostics_record("MeshCore command %u rejected, code=%u", command_pending, p->error);
        if (command_pending == 2 && job_count) {
            ++rejected;
            job_head = (job_head + 1) % 2;
            --job_count;
        }
        if (sync_step < 5)
            ble_link_reconnect();
        command_pending = 0;
        next_pull = esp_timer_get_time() + 2000000;
        return;
    }
    if (command_pending == 1 && p->type == MC_SELF) {
        own_node = identities_ok ? mc_ids_assign(&identities, p->key) : 0;
        memcpy(self_key, p->key, 32);
        if (!own_node) {
            diagnostics_record("Own MeshCore identity could not be saved");
            ble_link_reconnect();
        }
        diagnostics_set_node(own_node);
        command_pending = 0;
        sync_step = 2;
    } else if (command_pending == 22 && p->type == MC_DEVICE) {
        snprintf(node_version, sizeof(node_version), "MeshCore %.20s", p->version);
        snprintf(node_hardware, sizeof(node_hardware), "%s", p->hardware);
        node_pin_mode = p->fixed_pin ? 1 : -1;
        command_pending = 0;
        sync_step = 3;
    } else if (command_pending == 5 && p->type == MC_TIME) {
        if (!utc())
            set_clock(p->timestamp);
        command_pending = 0;
        sync_step = 4;
    } else if (command_pending == 4 && p->type == MC_CONTACTS_START) {
        command_deadline = esp_timer_get_time() + 45000000;
    } else if (command_pending == 4 && p->type == MC_CONTACT) {
        remember_contact(p);
        command_deadline = esp_timer_get_time() + 15000000;
    } else if (command_pending == 4 && p->type == MC_CONTACTS_END) {
        command_pending = 0;
        next_contacts = esp_timer_get_time() + 60000000;
        if (sync_step < 5) {
            sync_step = 5;
            api_ready = own_node && identities_ok;
            ++revision;
            diagnostics_record("MeshCore ready; app IDs are local aliases, routing uses radio paths");
            status();
        }
        if (deferred_valid) {
            deferred_valid = false;
            process_message(&deferred_message, false);
        }
    } else if (command_pending == 10 && (p->type == MC_MESSAGE || p->type == MC_MESSAGE_V3)) {
        command_pending = 0;
        next_pull = 0;
        process_message(p, true);
    } else if (command_pending == 10 && (p->type == MC_EMPTY || p->type == MC_CHANNEL || p->type == MC_CHANNEL_V3)) {
        command_pending = 0;
        next_pull = p->type == MC_EMPTY ? esp_timer_get_time() + 2000000 : 0;
    } else if (command_pending == 2 && p->type == MC_SENT) {
        command_pending = 0;
        pending[pending_next].id = p->ack;
        pending[pending_next].to = jobs[job_head].request.request.sender;
        pending[pending_next].sent = esp_timer_get_time();
        pending[pending_next].reported = false;
        pending_next = (pending_next + 1) % 32;
        ++submitted;
        diagnostics_record("TX MeshCore accepted ACK=%08" PRIx32 " part=%u/%u", p->ack, jobs[job_head].next + 1, jobs[job_head].reply.count);
        diagnostics_record("TX text: %.70s", jobs[job_head].reply.text[jobs[job_head].next]);
        if (++jobs[job_head].next >= jobs[job_head].reply.count) {
            job_head = (job_head + 1) % 2;
            --job_count;
        }
    }
}
static void radio_tick(int64_t now)
{
    if (!ble_up)
        return;
    if (command_pending) {
        if (now >= command_deadline) {
            diagnostics_record("MeshCore command %u timed out; delivery uncertain", command_pending);
            command_pending = 0;
            ble_link_reconnect();
            ble_up = api_ready = false;
            job_count = 0;
        }
        return;
    }
    if (sync_step == 1)
        send_command(1, mc_start(wire, sizeof(wire)));
    else if (sync_step == 2)
        send_command(22, mc_query(wire, sizeof(wire)));
    else if (sync_step == 3)
        send_command(5, mc_get_time(wire, sizeof(wire)));
    else if (sync_step == 4)
        send_command(4, mc_get_contacts(wire, sizeof(wire)));
    else if (api_ready) {
        if ((refresh_contacts && now >= next_contacts - 55000000) || now >= next_contacts || deferred_valid) {
            if (send_command(4, mc_get_contacts(wire, sizeof(wire))))
                refresh_contacts = false;
        } else if (job_count && now - text_sent >= 5000000 && utc()) {
            unsigned i = job_head;
            if (now - jobs[i].created > 900000000 || (jobs[i].reply.thread_id && (bulletin_by_id(&board, jobs[i].reply.thread_id) < 0 || (jobs[i].reply.expires && utc() >= jobs[i].reply.expires)))) {
                job_head = (job_head + 1) % 2;
                --job_count;
                return;
            }
            const uint8_t* key = jobs[i].request.request.key;
            const mc_identity_t* peer = mc_ids_prefix(&identities, key);
            if (!peer || memcmp(peer->key, key, 32)) {
                diagnostics_record("TX rejected: ambiguous MeshCore contact key");
                job_head = (job_head + 1) % 2;
                --job_count;
                return;
            }
            uint32_t stamp = utc();
            if (stamp <= outbound_timestamp)
                stamp = outbound_timestamp + 1;
            size_t n = mc_send(key, jobs[i].reply.text[jobs[i].next], stamp, wire, sizeof(wire));
            if (!n) {
                diagnostics_record("TX rejected: invalid MeshCore reply");
                job_head = (job_head + 1) % 2;
                --job_count;
                return;
            }
            if (send_command(2, n)) {
                text_sent = now;
                outbound_timestamp = stamp;
            }
        } else if (job_count < 2 && !deferred_valid && now >= next_pull)
            send_command(10, mc_next(wire, sizeof(wire)));
    }
}
static const char* str(const cJSON* j, const char* key)
{
    const cJSON* x = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsString(x) ? x->valuestring : NULL;
}
static int integer(const cJSON* j, const char* key, int fallback)
{
    const cJSON* x = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsNumber(x) && x->valuedouble == x->valueint ? x->valueint : fallback;
}
static uint32_t positive(const cJSON* j, const char* key, uint32_t fallback)
{
    const cJSON* x = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsNumber(x) && x->valuedouble >= 0 && x->valuedouble <= UINT32_MAX && (double)(uint32_t)x->valuedouble == x->valuedouble ? (uint32_t)x->valuedouble : fallback;
}
static void json_num(cJSON* r, const char* name, uint32_t x) { cJSON_AddNumberToObject(r, name, x); }
static void admin_poll(void)
{
    char input[513];
    if (!provision_take(input, sizeof(input)))
        return;
    cJSON *j = cJSON_Parse(input), *r = cJSON_CreateObject();
    const char* op = j ? str(j, "op") : NULL;
    int id = j ? integer(j, "id", 0) : 0;
    json_num(r, "id", id);
    bool ok = false;
    const char* error = "Invalid request";
    unsigned slot = (unsigned)(j ? integer(j, "slot", 0) - 1 : THREAD_SLOTS);
    uint32_t generation = j ? positive(j, "generation", 0) : 0;
    if (op && !strcmp(op, "create"))
        slot = (unsigned)bulletin_free(&board);
    else if (generation)
        slot = (unsigned)bulletin_by_id(&board, generation);
    if (op && !strcmp(op, "status")) {
        cJSON_AddStringToObject(r, "board", provision_id());
        cJSON_AddStringToObject(r, "target", settings.target);
        cJSON_AddStringToObject(r, "pair", ble_link_pair_state());
        cJSON_AddBoolToObject(r, "api", api_ready);
        cJSON_AddBoolToObject(r, "ble", ble_up);
        cJSON_AddStringToObject(r, "firmware", node_version);
        char name[12];
        snprintf(name, sizeof(name), "!%08" PRIx32, own_node);
        cJSON_AddStringToObject(r, "node", name);
        json_num(r, "time", utc());
        json_num(r, "revision", revision);
        json_num(r, "queued", board.queue.count);
        cJSON_AddNumberToObject(r, "total_threads", (double)bulletins_total_threads(&board));
        cJSON_AddNumberToObject(r, "total_replies", (double)bulletins_total_replies(&board));
        json_num(r, "hops", settings.hops);
        cJSON_AddBoolToObject(r, "no_pin", settings.pin == -1);
        cJSON_AddBoolToObject(r, "unread", bulletins_unread(&board, utc()));
        ok = true;
    } else if (op && !strcmp(op, "clock")) {
        ok = set_clock(positive(j, "time", 0));
        error = "Invalid phone time";
    } else if (op && !strcmp(op, "node_status")) {
        cJSON_AddStringToObject(r, "protocol", "MeshCore");
        json_num(r, "hw", ble_up ? node_hw : 0);
        cJSON_AddStringToObject(r, "hardware", ble_up ? node_hardware : "");
        cJSON_AddStringToObject(r, "pin_mode", ble_up ? (node_pin_mode == 0 ? "display" : node_pin_mode == 1 ? "fixed" : node_pin_mode == 2 ? "none" : "unknown") : "unknown");
        cJSON_AddStringToObject(r, "stage", ble_link_pair_stage());
        cJSON_AddStringToObject(r, "target", settings.target);
        json_num(r, "attempt", ble_link_pair_attempt());
        cJSON_AddNumberToObject(r, "code", ble_link_last_error());
        cJSON_AddBoolToObject(r, "api", api_ready);
        ok = true;
    } else if (op && !strcmp(op, "node_scan")) {
        ok = ble_link_scan_nodes();
        error = "Finish owner enrollment or cancel the current node pairing before scanning";
    } else if (op && !strcmp(op, "node_scan_results")) {
        unsigned index = (unsigned)integer(j, "index", 0), count;
        bool scanning;
        node_scan_entry_t entry;
        if (ble_link_scan_result(index, &entry, &count, &scanning)) {
            cJSON_AddStringToObject(r, "address", entry.address);
            cJSON_AddStringToObject(r, "name", entry.name);
            cJSON_AddNumberToObject(r, "rssi", entry.rssi);
            cJSON_AddBoolToObject(r, "connectable", entry.connectable && entry.protocol == 2);
            cJSON_AddStringToObject(r, "protocol", entry.protocol == 2 ? "MeshCore" : "Meshtastic");
            cJSON_AddBoolToObject(r, "service", entry.service_seen);
        }
        json_num(r, "count", count);
        cJSON_AddBoolToObject(r, "scanning", scanning);
        cJSON_AddNumberToObject(r, "code", ble_link_last_error());
        ok = index < NODE_SCAN_LIMIT;
    } else if (op && (!strcmp(op, "configure") || !strcmp(op, "node_select"))) {
        const char* target = str(j, "target");
        int hops = integer(j, "hops", 3);
        const cJSON* none = cJSON_GetObjectItemCaseSensitive(j, "no_pin");
        bool selecting = !strcmp(op, "node_select");
        if (target && bulletin_text_valid(target, 63) && hops == 3 && (!selecting || ble_link_selectable(target))) {
            settings_t next = { .pin = cJSON_IsTrue(none) ? -1 : -3, .hops = hops };
            strcpy(next.target, target);
            if (settings_save(&next)) {
                settings = next;
                ok = ble_link_select_node(&settings);
                error = "Node saved; pairing request busy. Tap Retry node pairing";
                ++revision;
            } else
                error = "Settings storage failed";
        } else
            error = "Select a connectable MeshCore node. Leave hop limit at 3; MeshCore uses radio routing";
    } else if (op && !strcmp(op, "pair")) {
        ok = settings.target[0] && ble_link_pair();
        error = "Select a node first";
    } else if (op && !strcmp(op, "pair_cancel")) {
        uint32_t attempt = positive(j, "attempt", 0);
        ok = (!attempt || attempt == ble_link_pair_attempt()) && ble_link_cancel_pair();
        error = "Pairing attempt changed; refresh node status";
    } else if (op && !strcmp(op, "pin")) {
        const char* pin = str(j, "pin");
        bool valid = pin && strlen(pin) == 6;
        if (valid)
            for (unsigned i = 0; i < 6; i++)
                if (!isdigit((unsigned char)pin[i]))
                    valid = false;
        ok = valid && ble_link_pin_for_attempt((uint32_t)strtoul(pin, NULL, 10), positive(j, "attempt", 0));
        error = "No PIN request is pending, or the PIN is not six digits";
    } else if (op && !strcmp(op, "list") && slot < THREAD_SLOTS) {
        int physical = bulletin_index(&board, slot, utc());
        static const bulletin_t empty;
        const bulletin_t* t = physical < 0 ? &empty : &board.slots[physical];
        json_num(r, "slot", slot + 1);
        json_num(r, "generation", t->generation);
        json_num(r, "creator", t->author_node);
        char id_text[8];
        bulletin_id(t->generation, id_text);
        cJSON_AddStringToObject(r, "thread_id", id_text);
        cJSON_AddBoolToObject(r, "active", t->active);
        cJSON_AddBoolToObject(r, "visible", bulletin_visible(t, utc()));
        cJSON_AddStringToObject(r, "title", t->title);
        json_num(r, "count", t->count);
        json_num(r, "unread", t->unread);
        json_num(r, "expires", t->expires);
        ok = true;
    } else if (op && !strcmp(op, "read") && slot < THREAD_SLOTS) {
        const bulletin_t* t = &board.slots[slot];
        int index = integer(j, "index", -1);
        if (t->active && t->generation == generation) {
            if (index == -1) {
                cJSON_AddStringToObject(r, "text", t->body);
                cJSON_AddStringToObject(r, "name", t->author_name);
                json_num(r, "node", t->author_node);
                json_num(r, "at", t->created);
                json_num(r, "count", t->count);
                json_num(r, "expires", t->expires);
                ok = true;
            } else if (index >= 0 && index < t->count) {
                const bulletin_comment_t* c = &t->comments[index];
                cJSON_AddStringToObject(r, "name", c->name);
                cJSON_AddStringToObject(r, "text", c->text);
                json_num(r, "at", c->at);
                json_num(r, "node", c->node);
                ok = true;
            }
        }
        error = "Thread changed or no longer exists; refresh Home";
    } else if (op && !strcmp(op, "create") && slot < THREAD_SLOTS) {
        const char *title = str(j, "title"), *body = str(j, "text");
        uint32_t now = utc(), expires = cJSON_HasObjectItem(j, "expires") ? positive(j, "expires", UINT32_MAX) : now + 86400;
        ok = expires != UINT32_MAX && !board.queue.count && title && body && bulletin_create(&board, slot, title, body, now, expires);
        error = "Slot occupied, invalid text/expiry, unsynced clock, or storage failure";
    } else if (op && !strcmp(op, "delete") && slot < THREAD_SLOTS) {
        ok = bulletin_delete(&board, slot, generation);
        error = "Thread changed or delete could not be saved";
    } else if (op && !strcmp(op, "seen") && slot < THREAD_SLOTS) {
        ok = bulletin_seen(&board, slot, generation, (unsigned)integer(j, "count", -1));
        error = "Thread changed or read state could not be saved";
    } else if (op && !strcmp(op, "expiry") && slot < THREAD_SLOTS) {
        uint32_t expires = positive(j, "expires", UINT32_MAX);
        ok = expires != UINT32_MAX && bulletin_expiry(&board, slot, generation, expires, utc());
        error = "Invalid expiry or thread changed";
    } else if (op && !strcmp(op, "activity")) {
        char report[320];
        diagnostics_report(report, sizeof(report));
        cJSON_AddStringToObject(r, "text", report);
        ok = true;
    } else if (op && !strcmp(op, "queue_list")) {
        unsigned index = (unsigned)integer(j, "index", 0);
        json_num(r, "count", board.queue.count);
        if (index < board.queue.count) {
            const bulletin_pending_t* p = &board.queue.entries[index];
            json_num(r, "position", index + 1);
            json_num(r, "node", p->node);
            json_num(r, "packet", p->packet_id);
            json_num(r, "at", p->submitted);
            cJSON_AddStringToObject(r, "title", p->title);
            cJSON_AddStringToObject(r, "name", p->name);
        }
        ok = index < THREAD_QUEUE;
    } else if (op && !strcmp(op, "queue_delete")) {
        ok = bulletins_remove_pending(&board, positive(j, "node", 0), positive(j, "packet", 0));
        error = "Queued bulletin changed or removal failed";
    } else if (op && !strcmp(op, "ban_list")) {
        unsigned index = (unsigned)integer(j, "index", 0);
        if (bans_ok && index < BAN_LIMIT) {
            json_num(r, "count", bans.count);
            cJSON_AddStringToObject(r, "message", bans.message);
            char node[12] = "";
            if (index < bans.count)
                snprintf(node, sizeof(node), "!%08" PRIx32, bans.nodes[index]);
            cJSON_AddStringToObject(r, "node", node);
            ok = true;
        } else
            error = "Ban storage unavailable or invalid index";
    } else if (op && (!strcmp(op, "ban_add") || !strcmp(op, "ban_remove"))) {
        uint32_t node = 0;
        bans_t next = bans;
        ok = bans_parse_node(str(j, "node"), &node) && node != own_node && bans_set(&next, node, !strcmp(op, "ban_add")) && save_bans(&next);
        error = "Invalid node ID, own node, ban list full (64), or storage failure";
    } else if (op && !strcmp(op, "rules_get")) {
        unsigned index = (unsigned)integer(j, "index", 0);
        if (index < RULE_COUNT) {
            cJSON_AddStringToObject(r, "text", board.rules.lines[index]);
            json_num(r, "count", RULE_COUNT);
            ok = true;
        }
    } else if (op && !strcmp(op, "rules_set")) {
        rules_t next = board.rules;
        if (storage_ok && rules_set(&next, (unsigned)integer(j, "index", -1), str(j, "text"))) {
            ok = nvs_set_blob(store, "rules", &next, sizeof(next)) == ESP_OK && nvs_commit(store) == ESP_OK;
            if (ok) {
                board.rules = next;
                ++revision;
            }
        }
        error = "Use up to 160 UTF-8 bytes per rule and keep at least one rule. Storage must be available";
    } else if (op && !strcmp(op, "ban_message")) {
        bans_t next = bans;
        ok = bans_message(&next, str(j, "text")) && save_bans(&next);
        error = "Use 1-120 UTF-8 bytes; storage must be available";
    }
    cJSON_AddBoolToObject(r, "ok", ok);
    if (!ok)
        cJSON_AddStringToObject(r, "error", error);
    char* json = cJSON_PrintUnformatted(r);
    if (json && strlen(json) <= 512)
        provision_reply(json);
    else {
        char fallback[100];
        snprintf(fallback, sizeof(fallback), "{\"id\":%d,\"ok\":false,\"error\":\"Response too large\"}", id);
        provision_reply(fallback);
    }
    if (json)
        cJSON_free(json);
    cJSON_Delete(r);
    cJSON_Delete(j);
    memset(input, 0, sizeof(input));
}
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    if (!settings_load(&settings)) {
        memset(&settings, 0, sizeof(settings));
        settings.pin = -3;
        settings.hops = 3;
    }
    esp_err_t rc = nvs_flash_init_partition("bbs");
    if (rc == ESP_OK)
        rc = nvs_open_from_partition("bbs", "mc_threads", NVS_READWRITE, &store);
    storage_ok = rc == ESP_OK;
    bulletins_init(&board, persist, NULL);
    bulletins_queue_storage(&board, persist_queue);
    bans_init(&bans);
    diagnostics_init();
    load_identities();
    if (storage_ok) {
        size_t size = sizeof(bans);
        rc = nvs_get_blob(store, "bans", &bans, &size);
        if (rc != ESP_ERR_NVS_NOT_FOUND && (rc != ESP_OK || size != sizeof(bans) || !bans_valid(&bans))) {
            bans_ok = false;
            ESP_LOGE(TAG, "Ban record invalid; visitor access disabled");
        }
    } else
        bans_ok = false;
    if (storage_ok) {
        rules_t saved;
        size_t rules_size = sizeof(saved);
        rc = nvs_get_blob(store, "rules", &saved, &rules_size);
        if (rc == ESP_OK && rules_size == sizeof(saved) && rules_valid(&saved))
            board.rules = saved;
        else if (rc != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGW(TAG, "Rules unreadable; using default community rules");
        bulletin_t* t = malloc(sizeof(*t));
        configASSERT(t);
        for (unsigned i = 0; i < THREAD_SLOTS; i++) {
            char key[12];
            snprintf(key, sizeof(key), "thread%u", i);
            memset(t, 0, sizeof(*t));
            size_t size = sizeof(*t);
            rc = nvs_get_blob(store, key, t, &size);
            if (rc == ESP_ERR_NVS_NOT_FOUND)
                continue;
            if (rc == ESP_OK && t->version == 2 && size == offsetof(bulletin_t, author_node)) {
                t->version = THREAD_VERSION;
                t->generation = i + 1;
                t->lifetime_threads = t->generation;
                t->lifetime_replies = t->count;
                strcpy(t->author_name, "Owner");
                size = sizeof(*t);
            }
            if (rc != ESP_OK || size != sizeof(*t) || !bulletin_restore(&board, i, t)) {
                storage_ok = false;
                ESP_LOGE(TAG, "Thread record invalid; storage writes disabled");
            }
        }
        free(t);
        bulletin_queue_t* q = malloc(sizeof(*q));
        configASSERT(q);
        size_t size = sizeof(*q);
        rc = nvs_get_blob(store, "waiting", q, &size);
        if (rc != ESP_ERR_NVS_NOT_FOUND && (rc != ESP_OK || size != sizeof(*q) || !bulletins_restore_queue(&board, q))) {
            storage_ok = false;
            ESP_LOGE(TAG, "Waiting queue invalid; storage writes disabled");
        }
        free(q);
    }
    jobs = heap_caps_calloc(2, sizeof(*jobs), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    configASSERT(jobs);
    uart_config_t uart_cfg = { .baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_cfg));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    usb_serial_jtag_driver_config_t usb_cfg = { .tx_buffer_size = 1024, .rx_buffer_size = 1024 };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    ESP_LOGI(TAG, "MESHBBS MeshCore 0.1.0: compatible owner app; nine threaded bulletins");
    provision_button_init();
    status_led_init();
    diagnostics_record("MeshCore BBS 0.1.0 boot; app-compatible");
    ble_link_start(&settings);
    for (;;) {
        console_poll();
        provision_button_tick();
        static int64_t maintenance_at;
        if (esp_timer_get_time() - maintenance_at >= 1000000) {
            maintenance_at = esp_timer_get_time();
            maintain_threads();
        }
        admin_poll();
        int64_t now = esp_timer_get_time();
        while (ble_link_receive(&event)) {
            if (event.kind == LINK_ERROR)
                diagnostics_record("%.*s", (int)event.length, (char*)event.bytes);
            else if (event.kind == LINK_UP) {
                ble_up = true;
                api_ready = false;
                own_node = 0;
                node_hw = 0; node_hardware[0] = 0; node_pin_mode = -1;
                job_head = job_count = 0;
                sync_started = now;
                sync_step = 1;
                command_pending = 0;
                deferred_valid = false;
                refresh_contacts = false;
                next_pull = 0;
                outbound_timestamp = 0;
            } else if (event.kind == LINK_DOWN || event.kind == LINK_TX_FAILED) {
                ble_up = api_ready = false;
                command_pending = 0;
                deferred_valid = false;
                own_node = 0;
                job_head = job_count = 0;
                ++revision;
                diagnostics_record("Node link down; delivery unconfirmed");
            } else if (event.kind == LINK_DATA) {
                if (mc_parse(event.bytes, event.length, &radio))
                    process_radio(&radio);
                else
                    diagnostics_record("Ignored unknown/invalid MeshCore frame type=%u", event.length ? event.bytes[0] : 0);
                memset(event.bytes, 0, sizeof(event.bytes));
            }
        }
        if (ble_up && !api_ready && now - sync_started > 120000000) {
            sync_started = now;
            ble_link_reconnect();
            diagnostics_record("Node API sync timeout");
        }
        radio_tick(now);
        for (unsigned i = 0; i < 32; i++)
            if (pending[i].id && !pending[i].reported && now - pending[i].sent > 120000000) {
                pending[i].reported = true;
                diagnostics_record("TX id=%" PRIu32 " no ACK observed", pending[i].id);
            }
        status_led_mode_t mode = ble_link_status();
        if (mode == STATUS_READY && !api_ready)
            mode = STATUS_CONNECTING;
        status_led_mode_t admin = provision_status();
        if (admin != STATUS_WAITING)
            mode = admin;
        status_led_unread(bulletins_unread(&board, utc()));
        status_led_tick(mode);
        diagnostics_tick();
        ble_link_tick();
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
