#include "ble_link.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "provision.h"
#include "services/gap/ble_svc_gap.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* MeshCore companion UART service. Owner BLE service stays unchanged. */
static ble_uuid_any_t other_service_uuid;
static ble_uuid_any_t service_uuid, to_uuid, from_uuid;
static const char* TAG = "ble_link";
static settings_t cfg;
static QueueHandle_t rxq, txq;
static QueueHandle_t controlq;
typedef struct {
    unsigned kind;
    settings_t settings;
    uint32_t pin;
    uint32_t attempt;
} control_t;
static bool interactive, blocked_auth;
static _Atomic int pair_state; /* 0 idle, 1 pairing, 2 PIN required, 3 bonded, 4 needs pairing */
enum { NODE_IDLE,
    NODE_SEARCH,
    NODE_CONNECT,
    NODE_PAIR,
    NODE_PIN,
    NODE_SECURE,
    NODE_DISCOVER,
    NODE_READY,
    NODE_FAILED };
static _Atomic int node_stage, last_error;
static _Atomic uint32_t pair_attempt;
static _Atomic bool scanning_nodes;
static int64_t manual_scan_until, pair_deadline;
static node_scan_t scan_cache;
static SemaphoreHandle_t scan_mutex;
static bool mtu_started, reconnect_pending, cancel_connect_pending;
static struct ble_npl_event pump_event;
static uint16_t conn = BLE_HS_CONN_HANDLE_NONE;
static uint16_t svc_start, svc_end, to_handle, from_handle, from_end, cccd;
static uint8_t own_type;
static bool synced, connecting, ready, busy;
static _Atomic bool force_reconnect;
static _Atomic status_led_mode_t link_status = STATUS_WAITING;
static int64_t next_scan, op_started;
static char last_logged[20];
static int64_t last_logged_at;
void ble_store_config_init(void);

static int gap_cb(struct ble_gap_event* event, void* arg);
static void begin_discovery(void);
static void begin_mtu(void);
static void pump(struct ble_npl_event* event);

static void emit(link_event_kind_t kind)
{
    link_event_t evt = { .kind = kind };
    if (xQueueSend(rxq, &evt, 0) != pdTRUE) {
        /* A full queue must not hide a disconnect/config restart. */
        xQueueReset(rxq);
        xQueueSend(rxq, &evt, 0);
    }
}

static void fail(const char* operation, int rc)
{
    atomic_store(&last_error, rc);
    atomic_store(&node_stage, NODE_FAILED);
    if (interactive) {
        blocked_auth = true;
        interactive = false;
        pair_deadline = 0;
        atomic_store(&pair_state, 4);
    }
    if (rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) || rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC)) {
        blocked_auth = true;
        atomic_store(&pair_state, 4);
    }
    ESP_LOGW(TAG, "%s failed: %d; reconnecting", operation, rc);
    /* Keep diagnostic storage on the main task, not this NimBLE callback. */
    link_event_t error = { .kind = LINK_ERROR };
    const char* hint = rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) ? "; check node PIN" : "";
    snprintf((char*)error.bytes, sizeof(error.bytes), "BLE %s code=%d%s", operation, rc, hint);
    error.length = (uint16_t)strlen((char*)error.bytes);
    xQueueSend(rxq, &error, 0);
    ready = false;
    atomic_store(&link_status, STATUS_WAITING);
    if (conn != BLE_HS_CONN_HANDLE_NONE)
        ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    else {
        connecting = false;
        next_scan = esp_timer_get_time() + 5000000;
    }
}

static void start_scan(void)
{
    bool manual = atomic_load(&scanning_nodes);
    if (!synced || provision_pairing_open() || connecting || ble_gap_disc_active())
        return;
    if (!manual && (!cfg.target[0] || blocked_auth || conn != BLE_HS_CONN_HANDLE_NONE))
        return;
    struct ble_gap_disc_params params = { 0 };
    params.passive = 0; /* Names on nRF52 nodes arrive in scan responses. */
    params.itvl = 160;
    params.window = 80;
    params.filter_duplicates = 0;
    int rc = ble_gap_disc(own_type, manual ? 12000 : 10000, &params, gap_cb, NULL);
    if (rc) {
        if (manual) {
            atomic_store(&scanning_nodes, false);
            atomic_store(&last_error, rc);
        }
        next_scan = esp_timer_get_time() + 5000000;
    }
}

static void inspect_advertisement(const struct ble_gap_disc_desc* disc)
{
    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data))
        return;
    char name[64] = { 0 }, address[18];
    if (fields.name) {
        size_t n = fields.name_len < sizeof(name) - 1 ? fields.name_len : sizeof(name) - 1;
        memcpy(name, fields.name, n);
    }
    snprintf(address, sizeof(address), "%02X:%02X:%02X:%02X:%02X:%02X",
        disc->addr.val[5], disc->addr.val[4], disc->addr.val[3],
        disc->addr.val[2], disc->addr.val[1], disc->addr.val[0]);
    bool mesh_service = false;
    unsigned protocol = 0;
    for (unsigned i = 0; i < fields.num_uuids128; ++i)
        if (ble_uuid_cmp(&fields.uuids128[i].u, &service_uuid.u) == 0) {
            mesh_service = true;
            protocol = 2;
        } else if (ble_uuid_cmp(&fields.uuids128[i].u, &other_service_uuid.u) == 0)
            protocol = 1;
    bool match = cfg.target[0] && (!strcasecmp(cfg.target, address) || !strcasecmp(cfg.target, name));
    int connectable = disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP ? -1
                                                                          : (disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND || disc->event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND);
    /* Preserve a name from SCAN_RSP and connectability from its advertisement. */
    xSemaphoreTake(scan_mutex, portMAX_DELAY);
    node_scan_record(&scan_cache, address, disc->addr.type, name, protocol, connectable, disc->rssi, esp_timer_get_time() / 1000);
    const node_scan_entry_t* selected = node_scan_find(&scan_cache, address, esp_timer_get_time() / 1000);
    bool selectable = selected && selected->protocol == 2;
    if (selected && !strcasecmp(cfg.target, selected->name))
        match = true;
    xSemaphoreGive(scan_mutex);
    if ((mesh_service || !strncasecmp(name, "MeshCore", 8) || match) && (strcmp(address, last_logged) || esp_timer_get_time() - last_logged_at > 10000000)) {
        ESP_LOGI(TAG, "Found %s name='%s' RSSI=%d", address, name, disc->rssi);
        strcpy(last_logged, address);
        last_logged_at = esp_timer_get_time();
    }
    /* Explicit selection avoids pairing with an arbitrary nearby mesh node. */
    if (!match || !selectable || atomic_load(&scanning_nodes) || blocked_auth || provision_pairing_open() || connecting || conn != BLE_HS_CONN_HANDLE_NONE)
        return;
    if (ble_gap_disc_cancel())
        return;
    connecting = true;
    op_started = esp_timer_get_time();
    atomic_store(&link_status, STATUS_CONNECTING);
    atomic_store(&node_stage, NODE_CONNECT);
    ESP_LOGI(TAG, "Connecting to selected node %s", address);
    int rc = ble_gap_connect(own_type, &disc->addr, 15000, NULL, gap_cb, NULL);
    if (rc)
        fail("connect", rc);
}

static int subscribed(uint16_t handle, const struct ble_gatt_error* error,
    struct ble_gatt_attr* attr, void* arg)
{
    (void)handle;
    (void)attr;
    (void)arg;
    if (error->status) {
        fail("subscribe", error->status);
        return 0;
    }
    ready = true;
    busy = false;
    atomic_store(&link_status, STATUS_READY);
    atomic_store(&node_stage, NODE_READY);
    if (cfg.pin == -1)
        atomic_store(&pair_state, 0);
    pair_deadline = 0;
    ESP_LOGI(TAG, "MeshCore BLE service ready; MTU=%u", ble_att_mtu(conn));
    emit(LINK_UP);
    return 0;
}

static int descriptor_found(uint16_t handle, const struct ble_gatt_error* error,
    uint16_t chr_handle, const struct ble_gatt_dsc* dsc, void* arg)
{
    (void)handle;
    (void)chr_handle;
    (void)arg;
    if (!error->status) {
        if (ble_uuid_u16(&dsc->uuid.u) == 0x2902)
            cccd = dsc->handle;
    } else if (error->status == BLE_HS_EDONE) {
        if (!cccd) {
            fail("TX notification descriptor missing", -1);
            return 0;
        }
        uint8_t value[2] = { 1, 0 };
        int rc = ble_gattc_write_flat(conn, cccd, value, sizeof(value), subscribed, NULL);
        if (rc)
            fail("subscribe start", rc);
    } else
        fail("descriptor discovery", error->status);
    return 0;
}

static int characteristic_found(uint16_t handle, const struct ble_gatt_error* error,
    const struct ble_gatt_chr* chr, void* arg)
{
    (void)handle;
    (void)arg;
    if (!error->status) {
        if (from_handle && chr->def_handle > from_handle && chr->def_handle - 1 < from_end)
            from_end = chr->def_handle - 1;
        if (!ble_uuid_cmp(&chr->uuid.u, &to_uuid.u))
            to_handle = chr->val_handle;
        if (!ble_uuid_cmp(&chr->uuid.u, &from_uuid.u)) {
            from_handle = chr->val_handle;
            from_end = svc_end;
        }
    } else if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "MeshCore handles RX=%u TX=%u",
            to_handle, from_handle);
        if (!to_handle || !from_handle) {
            fail("required characteristic missing", -1);
            return 0;
        }
        int rc = ble_gattc_disc_all_dscs(conn, from_handle, from_end, descriptor_found, NULL);
        if (rc)
            fail("descriptor discovery start", rc);
    } else
        fail("characteristic discovery", error->status);
    return 0;
}

static int service_found(uint16_t handle, const struct ble_gatt_error* error,
    const struct ble_gatt_svc* svc, void* arg)
{
    (void)handle;
    (void)arg;
    if (!error->status) {
        svc_start = svc->start_handle;
        svc_end = svc->end_handle;
    } else if (error->status == BLE_HS_EDONE) {
        if (!svc_start) {
            fail("MeshCore service missing", -1);
            return 0;
        }
        int rc = ble_gattc_disc_all_chrs(conn, svc_start, svc_end, characteristic_found, NULL);
        if (rc)
            fail("characteristic discovery start", rc);
    } else
        fail("service discovery", error->status);
    return 0;
}

static void begin_discovery(void)
{
    atomic_store(&node_stage, NODE_DISCOVER);
    op_started = esp_timer_get_time();
    int rc = ble_gattc_disc_svc_by_uuid(conn, &service_uuid.u, service_found, NULL);
    if (rc)
        fail("service discovery start", rc);
}

static int mtu_exchanged(uint16_t handle, const struct ble_gatt_error* error,
    uint16_t mtu, void* arg)
{
    (void)handle;
    (void)arg;
    if (error->status)
        ESP_LOGW(TAG, "MTU negotiation status %d", error->status);
    else
        ESP_LOGI(TAG, "Negotiated MTU %u", mtu);
    if (ble_att_mtu(conn) < 176)
        fail("MeshCore requires ATT MTU >=176", -1);
    else
        begin_discovery();
    return 0;
}

static void begin_mtu(void)
{
    if (mtu_started || conn == BLE_HS_CONN_HANDLE_NONE)
        return;
    mtu_started = true;
    atomic_store(&node_stage, NODE_DISCOVER);
    op_started = esp_timer_get_time();
    int rc = ble_gattc_exchange_mtu(conn, mtu_exchanged, NULL);
    if (rc)
        fail("MTU start", rc);
}

static void begin_pairing(void)
{
    interactive = cfg.pin != -1;
    blocked_auth = false;
    atomic_store(&pair_state, 1);
    atomic_store(&node_stage, NODE_SEARCH);
    atomic_store(&last_error, 0);
    uint32_t attempt;
    do {
        attempt = esp_random();
    } while (!attempt);
    atomic_store(&pair_attempt, attempt);
    pair_deadline = esp_timer_get_time() + 90000000;
    atomic_store(&scanning_nodes, false);
    atomic_store(&force_reconnect, true);
}

static int write_complete(uint16_t handle, const struct ble_gatt_error* error,
    struct ble_gatt_attr* attr, void* arg)
{
    (void)handle;
    (void)attr;
    (void)arg;
    busy = false;
    if (error->status) {
        emit(LINK_TX_FAILED);
        fail("ToRadio write", error->status);
    }
    return 0;
}

static void pump(struct ble_npl_event* event)
{
    (void)event;
    int64_t now = esp_timer_get_time();
    control_t control;
    while (xQueueReceive(controlq, &control, 0) == pdTRUE) {
        if (control.kind == 1) {
            cfg = control.settings;
            blocked_auth = false;
            interactive = false;
            atomic_store(&pair_state, 0);
            atomic_store(&force_reconnect, true);
        } else if (control.kind == 2) {
            begin_pairing();
        } else if (control.kind == 3 && control.attempt == atomic_load(&pair_attempt) && atomic_load(&pair_state) == 2 && conn != BLE_HS_CONN_HANDLE_NONE) {
            struct ble_sm_io io = { .action = BLE_SM_IOACT_INPUT, .passkey = control.pin };
            atomic_store(&pair_state, 1);
            atomic_store(&node_stage, NODE_SECURE);
            int rc = ble_sm_inject_io(conn, &io);
            if (rc)
                fail("PIN submission", rc);
        } else if (control.kind == 4) {
            if (ble_gap_disc_active())
                ble_gap_disc_cancel();
            xSemaphoreTake(scan_mutex, portMAX_DELAY);
            node_scan_clear(&scan_cache);
            xSemaphoreGive(scan_mutex);
            atomic_store(&last_error, 0);
            atomic_store(&scanning_nodes, true);
            manual_scan_until = now + 12000000;
            start_scan();
        } else if (control.kind == 5) {
            cfg = control.settings;
            begin_pairing();
        } else if (control.kind == 6) {
            interactive = false;
            blocked_auth = true;
            pair_deadline = 0;
            atomic_store(&pair_attempt, 0);
            atomic_store(&pair_state, 0);
            atomic_store(&node_stage, NODE_IDLE);
            atomic_store(&scanning_nodes, false);
            atomic_store(&force_reconnect, true);
        }
        memset(&control, 0, sizeof(control));
    }
    provision_tick();
    if (atomic_load(&scanning_nodes) && now >= manual_scan_until) {
        atomic_store(&scanning_nodes, false);
        if (ble_gap_disc_active())
            ble_gap_disc_cancel();
    }
    if (atomic_exchange(&force_reconnect, false)) {
        if (ble_gap_disc_active())
            ble_gap_disc_cancel();
        if (connecting) {
            cancel_connect_pending = true;
            if (ble_gap_conn_cancel())
                cancel_connect_pending = false;
            if (!cancel_connect_pending)
                connecting = false;
        }
        next_scan = now + 500000;
        if (conn != BLE_HS_CONN_HANDLE_NONE) {
            reconnect_pending = true;
            ready = false;
            ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
            return;
        }
    }
    if (reconnect_pending || cancel_connect_pending)
        return;
    if (pair_deadline && now >= pair_deadline) {
        blocked_auth = true;
        pair_deadline = 0;
        atomic_store(&pair_state, 4);
        if (connecting) {
            ble_gap_conn_cancel();
            connecting = false;
        }
        if (ble_gap_disc_active())
            ble_gap_disc_cancel();
        fail("node pairing timed out; tap to retry", BLE_HS_ETIMEOUT);
        return;
    }
    if (conn == BLE_HS_CONN_HANDLE_NONE) {
        if (now >= next_scan)
            start_scan();
        return;
    }
    if ((!ready || busy) && now - op_started > (atomic_load(&pair_state) == 2 ? 120000000 : 30000000)) {
        fail("GATT timeout", -1);
        return;
    }
    if (!ready || busy)
        return;
    link_event_t tx;
    if (xQueueReceive(txq, &tx, 0) == pdTRUE) {
        busy = true;
        op_started = now;
        int rc;
        if (tx.length <= ble_att_mtu(conn) - 3) {
            rc = ble_gattc_write_flat(conn, to_handle, tx.bytes, tx.length, write_complete, NULL);
        } else
            rc = BLE_HS_EMSGSIZE; /* One MeshCore frame per ATT write. */
        if (rc) {
            busy = false;
            emit(LINK_TX_FAILED);
            fail("ToRadio write start", rc);
        }
    }
}

static int gap_cb(struct ble_gap_event* event, void* arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        inspect_advertisement(&event->disc);
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        next_scan = esp_timer_get_time() + 2000000;
        break;
    case BLE_GAP_EVENT_CONNECT:
        connecting = false;
        if (cancel_connect_pending) {
            cancel_connect_pending = false;
            if (!event->connect.status) {
                conn = event->connect.conn_handle;
                reconnect_pending = true;
                ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
            } else
                next_scan = esp_timer_get_time() + 500000;
            break;
        }
        if (event->connect.status) {
            fail("connect result", event->connect.status);
            break;
        }
        conn = event->connect.conn_handle;
        op_started = esp_timer_get_time();
        if (provision_pairing_open()) {
            ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
        atomic_store(&link_status, STATUS_CONNECTING);
        svc_start = svc_end = to_handle = from_handle = from_end = cccd = 0;
        ready = busy = mtu_started = false;
        /* Bond before starting the radio GATT service, like the official client.
         * SMP Security Request triggers the radio's display-PIN callback. */
        if (cfg.pin != -1) {
            ble_hs_cfg.sm_io_cap = BLE_HS_IO_KEYBOARD_ONLY;
            ble_hs_cfg.sm_mitm = 1;
            atomic_store(&pair_state, 1);
            atomic_store(&node_stage, NODE_PAIR);
            int rc = ble_gap_security_initiate(conn);
            if (rc == BLE_HS_EALREADY) {
                struct ble_gap_conn_desc desc;
                if (!ble_gap_conn_find(conn, &desc) && desc.sec_state.encrypted)
                    begin_mtu();
            } else if (rc)
                fail("pairing start", rc);
        } else
            begin_mtu();
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status) {
            blocked_auth = true;
            atomic_store(&pair_state, 4);
            fail("pairing/encryption; retry pairing in app", event->enc_change.status);
        } else {
            atomic_store(&pair_state, 3);
            interactive = false;
            begin_mtu();
        }
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io io = { 0 };
        io.action = event->passkey.params.action;
        if (io.action == BLE_SM_IOACT_INPUT && interactive && provision_authorized()) {
            atomic_store(&pair_state, 2);
            atomic_store(&node_stage, NODE_PIN);
            op_started = esp_timer_get_time();
            ESP_LOGI(TAG, "NODE PIN REQUIRED: enter the six digits displayed by the node in the owner app");
        } else if (io.action == BLE_SM_IOACT_INPUT && cfg.pin >= 0) {
            io.passkey = (uint32_t)cfg.pin;
            int rc = ble_sm_inject_io(conn, &io);
            if (rc)
                fail("passkey entry", rc);
        } else {
            blocked_auth = true;
            atomic_store(&pair_state, 4);
            fail("node needs pairing; use owner app", -1);
        }
        break;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* The authenticated owner explicitly requested pairing. Replace only
         * this node's stale key, preserving phone and other node bonds. */
        if (interactive && provision_authorized()) {
            struct ble_gap_conn_desc d;
            if (!ble_gap_conn_find(event->repeat_pairing.conn_handle, &d) && !ble_store_util_delete_peer(&d.peer_id_addr))
                return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
        blocked_auth = true;
        atomic_store(&pair_state, 4);
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    case BLE_GAP_EVENT_NOTIFY_RX:
        if (event->notify_rx.conn_handle == conn && event->notify_rx.attr_handle == from_handle) {
            link_event_t message = { .kind = LINK_DATA };
            size_t size = OS_MBUF_PKTLEN(event->notify_rx.om);
            if (!size || size > 176 || os_mbuf_copydata(event->notify_rx.om, 0, size, message.bytes)) {
                fail("invalid MeshCore notification", -1);
                break;
            }
            message.length = (uint16_t)size;
            if (xQueueSend(rxq, &message, 0) != pdTRUE)
                fail("MeshCore receive overflow", -1);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGW(TAG, "Disconnected (reason %d); retry in 5s", event->disconnect.reason);
        conn = BLE_HS_CONN_HANDLE_NONE;
        ready = busy = connecting = false;
        if (!reconnect_pending && (atomic_load(&pair_state) == 2 || pair_deadline)) {
            blocked_auth = true;
            atomic_store(&pair_state, 4);
            atomic_store(&node_stage, NODE_FAILED);
            if (!atomic_load(&last_error))
                atomic_store(&last_error, event->disconnect.reason);
            interactive = false;
            pair_deadline = 0;
        }
        if (!reconnect_pending && atomic_load(&node_stage) == NODE_READY)
            atomic_store(&node_stage, NODE_SEARCH);
        reconnect_pending = false;
        ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
        ble_hs_cfg.sm_mitm = 0;
        atomic_store(&link_status, STATUS_WAITING);
        xQueueReset(txq);
        next_scan = esp_timer_get_time() + 5000000;
        emit(LINK_DOWN);
        break;
    default:
        break;
    }
    return 0;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc)
        rc = ble_hs_id_infer_auto(0, &own_type);
    if (rc) {
        ESP_LOGE(TAG, "BLE address initialization failed %d", rc);
        return;
    }
    synced = true;
    provision_sync(own_type);
    start_scan();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset: %d", reason);
    synced = ready = busy = connecting = false;
    conn = BLE_HS_CONN_HANDLE_NONE;
    reconnect_pending = false;
    cancel_connect_pending = false;
    atomic_store(&scanning_nodes, false);
    atomic_store(&node_stage, NODE_FAILED);
    atomic_store(&last_error, reason);
    atomic_store(&link_status, STATUS_WAITING);
    xQueueReset(txq);
    emit(LINK_DOWN);
    provision_reset();
}

static void host_task(void* arg)
{
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
static int store_full(struct ble_store_status_event* event, void* arg)
{
    (void)event;
    (void)arg;
    ESP_LOGE(TAG, "Bluetooth key storage full; existing bonds retained");
    return BLE_HS_ENOMEM;
}

void ble_link_start(const settings_t* settings)
{
    cfg = *settings;
    ESP_ERROR_CHECK(ble_uuid_from_str(&other_service_uuid, "6ba1b218-15a8-461f-9fa8-5dcae273eafd"));
    ESP_ERROR_CHECK(ble_uuid_from_str(&service_uuid, "6e400001-b5a3-f393-e0a9-e50e24dcca9e"));
    ESP_ERROR_CHECK(ble_uuid_from_str(&to_uuid, "6e400002-b5a3-f393-e0a9-e50e24dcca9e"));
    ESP_ERROR_CHECK(ble_uuid_from_str(&from_uuid, "6e400003-b5a3-f393-e0a9-e50e24dcca9e"));
    rxq = xQueueCreate(12, sizeof(link_event_t));
    txq = xQueueCreate(6, sizeof(link_event_t));
    controlq = xQueueCreate(4, sizeof(control_t));
    scan_mutex = xSemaphoreCreateMutex();
    configASSERT(rxq && txq && controlq && scan_mutex);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_npl_event_init(&pump_event, pump, NULL);
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = store_full;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    provision_init();
    ble_att_set_preferred_mtu(517);
#if CONFIG_BT_NIMBLE_GAP_SERVICE
    ble_svc_gap_device_name_set("MESHBBS");
#endif
    nimble_port_freertos_init(host_task);
}

void ble_link_tick(void) { ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &pump_event); }
bool ble_link_receive(link_event_t* event) { return xQueueReceive(rxq, event, 0) == pdTRUE; }
bool ble_link_send(const uint8_t* data, size_t size)
{
    if (!size || size > 173)
        return false;
    link_event_t msg = { .kind = LINK_DATA, .length = (uint16_t)size };
    memcpy(msg.bytes, data, size);
    return xQueueSend(txq, &msg, 0) == pdTRUE;
}
void ble_link_reconnect(void) { atomic_store(&force_reconnect, true); }
status_led_mode_t ble_link_status(void) { return atomic_load(&link_status); }
void ble_link_configure(const settings_t* s)
{
    control_t c = { .kind = 1, .settings = *s };
    xQueueSend(controlq, &c, portMAX_DELAY);
}
bool ble_link_pair(void)
{
    control_t c = { .kind = 2 };
    return xQueueSend(controlq, &c, 0) == pdTRUE;
}
bool ble_link_submit_pin(uint32_t pin)
{
    return ble_link_pin_for_attempt(pin, atomic_load(&pair_attempt));
}
bool ble_link_pin_for_attempt(uint32_t pin, uint32_t attempt)
{
    if (pin > 999999 || !attempt || attempt != atomic_load(&pair_attempt) || atomic_load(&pair_state) != 2)
        return false;
    control_t c = { .kind = 3, .pin = pin, .attempt = attempt };
    return xQueueSend(controlq, &c, 0) == pdTRUE;
}
bool ble_link_scan_nodes(void)
{
    if (provision_pairing_open() || atomic_load(&node_stage) == NODE_CONNECT || atomic_load(&pair_state) == 1 || atomic_load(&pair_state) == 2)
        return false;
    control_t c = { .kind = 4 };
    return xQueueSend(controlq, &c, 0) == pdTRUE;
}
bool ble_link_scan_result(unsigned index, node_scan_entry_t* entry, unsigned* count, bool* scanning)
{
    xSemaphoreTake(scan_mutex, portMAX_DELAY);
    *count = scan_cache.count;
    bool found = index < *count;
    if (found)
        *entry = scan_cache.entries[index];
    *scanning = atomic_load(&scanning_nodes);
    xSemaphoreGive(scan_mutex);
    return found;
}
bool ble_link_selectable(const char* address)
{
    xSemaphoreTake(scan_mutex, portMAX_DELAY);
    const node_scan_entry_t* e = node_scan_find(&scan_cache, address, esp_timer_get_time() / 1000);
    bool result = e && e->protocol == 2;
    xSemaphoreGive(scan_mutex);
    return result;
}
bool ble_link_select_node(const settings_t* s)
{
    control_t c = { .kind = 5, .settings = *s };
    return xQueueSend(controlq, &c, 0) == pdTRUE;
}
bool ble_link_cancel_pair(void)
{
    control_t c = { .kind = 6 };
    return xQueueSend(controlq, &c, 0) == pdTRUE;
}
uint32_t ble_link_pair_attempt(void) { return atomic_load(&pair_attempt); }
int ble_link_last_error(void) { return atomic_load(&last_error); }
const char* ble_link_pair_stage(void)
{
    static const char* stages[] = { "idle", "searching", "connecting", "pairing", "enter_pin", "securing", "discovering", "ready", "failed" };
    return stages[atomic_load(&node_stage)];
}
const char* ble_link_pair_state(void)
{
    switch (atomic_load(&pair_state)) {
    case 1:
        return "pairing";
    case 2:
        return "enter_pin";
    case 3:
        return "bonded";
    case 4:
        return "needs_pairing";
    default:
        return "idle";
    }
}
