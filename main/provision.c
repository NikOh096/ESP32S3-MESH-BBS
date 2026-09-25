#include "provision.h"
#include "ble_link.h"
#include "cJSON.h"
#include "diagnostics.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "owner.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define UUID(id) BLE_UUID128_INIT(0x16, 0x27, 0xad, 0x74, 0xb9, 0x75, 0x49, 0x9d, 0x15, 0x4d, 0xa2, 0x6c, id, 0x00, 0x3a, 0x8f)
static const ble_uuid128_t svc = UUID(1), info_uuid = UUID(2), rpc_uuid = UUID(6);
static const char* TAG = "owner";
static uint8_t own_type, nonce[32];
static char uid[13], name[24], request[513], response[513];
static uint16_t phone = BLE_HS_CONN_HANDLE_NONE;
static _Atomic bool authorized, open_window, refresh_adv, enrollment_requested;
static bool synced, pending, owner_store_ok;
static int64_t deadline, connected_at, pressed_at, sequence_at;
static unsigned clicks;
static bool held;
static SemaphoreHandle_t lock;

const char* provision_id(void) { return uid; }
bool provision_pairing_open(void) { return atomic_load(&open_window); }
bool provision_authorized(void) { return atomic_load(&authorized); }
static bool allowed(uint16_t handle, struct ble_gap_conn_desc* desc)
{
    return handle == phone && !ble_gap_conn_find(handle, desc) && (provision_pairing_open() || owner_peer(&desc->peer_id_addr));
}
static void respond_error(int id, const char* error)
{
    cJSON* r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "id", id);
    cJSON_AddBoolToObject(r, "ok", false);
    cJSON_AddStringToObject(r, "error", error);
    char* s = cJSON_PrintUnformatted(r);
    if (s) {
        snprintf(response, sizeof(response), "%s", s);
        cJSON_free(s);
    }
    cJSON_Delete(r);
}
static int access_cb(uint16_t connection, uint16_t attr, struct ble_gatt_access_ctxt* ctx, void* arg)
{
    (void)attr;
    struct ble_gap_conn_desc desc;
    if (!allowed(connection, &desc))
        return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    if (!desc.sec_state.encrypted)
        return BLE_ATT_ERR_INSUFFICIENT_ENC;
    if (!xSemaphoreTake(lock, pdMS_TO_TICKS(50)))
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    int rc = 0;
    if ((uintptr_t)arg == 0 && ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char hex[65], value[240];
        owner_hex(nonce, 32, hex);
        snprintf(value, sizeof(value), "{\"v\":2,\"id\":\"%s\",\"name\":\"%s\",\"enroll\":%s,\"owned\":%s,\"nonce\":\"%s\"}",
            uid, name, provision_pairing_open() ? "true" : "false", owner_exists() ? "true" : "false", hex);
        if (os_mbuf_append(ctx->om, value, strlen(value)))
            rc = BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if ((uintptr_t)arg == 1 && ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (os_mbuf_append(ctx->om, response, strlen(response)))
            rc = BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if ((uintptr_t)arg == 1 && ctx->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        char value[513] = { 0 };
        uint16_t n = 0;
        if (ble_hs_mbuf_to_flat(ctx->om, value, 512, &n) || !n || memchr(value, 0, n)) {
            rc = BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            goto end;
        }
        cJSON* json = cJSON_ParseWithOpts(value, NULL, true);
        const cJSON* op = json ? cJSON_GetObjectItemCaseSensitive(json, "op") : NULL;
        const cJSON* seq = json ? cJSON_GetObjectItemCaseSensitive(json, "id") : NULL;
        int id = cJSON_IsNumber(seq) ? seq->valueint : 0;
        if (!cJSON_IsString(op) || id <= 0) {
            cJSON_Delete(json);
            rc = BLE_ATT_ERR_VALUE_NOT_ALLOWED;
            goto end;
        }
        if (!strcmp(op->valuestring, "enroll") || !strcmp(op->valuestring, "auth")) {
            uint8_t key[32] = { 0 };
            const cJSON* proof = cJSON_GetObjectItemCaseSensitive(json, "proof");
            bool valid = cJSON_IsString(proof) && owner_unhex(proof->valuestring, key, 32);
            if (!strcmp(op->valuestring, "enroll")) {
                valid = valid && owner_store_ok && provision_pairing_open() && owner_enroll(&desc.peer_id_addr, key);
                if (valid) {
                    atomic_store(&open_window, false);
                    atomic_store(&refresh_adv, true);
                    ESP_LOGI(TAG, "Owner app enrolled; physical window closed");
                }
            } else
                valid = valid && owner_peer(&desc.peer_id_addr) && owner_verify(nonce, key, uid);
            memset(key, 0, sizeof(key));
            esp_fill_random(nonce, 32);
            atomic_store(&authorized, valid);
            if (valid)
                snprintf(response, sizeof(response), "{\"id\":%d,\"ok\":true}", id);
            else
                respond_error(id, "Owner authentication failed. Use the physical enrollment sequence to replace the owner.");
        } else if (!provision_authorized())
            respond_error(id, "Owner app authentication required");
        else if (pending)
            respond_error(id, "Previous request is still running; retry");
        else {
            memcpy(request, value, n + 1);
            pending = true;
            snprintf(response, sizeof(response), "{\"id\":%d,\"busy\":true}", id);
        }
        cJSON_Delete(json);
        memset(value, 0, sizeof(value));
    } else
        rc = BLE_ATT_ERR_WRITE_NOT_PERMITTED;
end:
    xSemaphoreGive(lock);
    return rc;
}
static const struct ble_gatt_chr_def chars[] = {
    { .uuid = &info_uuid.u, .access_cb = access_cb, .arg = (void*)0, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC },
    { .uuid = &rpc_uuid.u, .access_cb = access_cb, .arg = (void*)1, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC }, { 0 }
};
static const struct ble_gatt_svc_def services[] = { { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &svc.u, .characteristics = chars }, { 0 } };
static int gap(struct ble_gap_event* e, void* arg)
{
    (void)arg;
    if (e->type == BLE_GAP_EVENT_CONNECT) {
        if (e->connect.status) {
            atomic_store(&refresh_adv, true);
            return 0;
        }
        phone = e->connect.conn_handle;
        connected_at = esp_timer_get_time();
        atomic_store(&authorized, false);
        xSemaphoreTake(lock, portMAX_DELAY);
        esp_fill_random(nonce, 32);
        pending = false;
        strcpy(response, "{\"id\":0,\"ok\":false}");
        xSemaphoreGive(lock);
        struct ble_gap_conn_desc d;
        if (!allowed(phone, &d)) {
            ble_gap_terminate(phone, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        ESP_LOGI(TAG, "Owner app transport connected; application authentication required");
    } else if (e->type == BLE_GAP_EVENT_DISCONNECT) {
        phone = BLE_HS_CONN_HANDLE_NONE;
        atomic_store(&authorized, false);
        atomic_store(&refresh_adv, true);
    } else if (e->type == BLE_GAP_EVENT_REPEAT_PAIRING) {
        struct ble_gap_conn_desc d;
        if (provision_pairing_open() && !ble_gap_conn_find(e->repeat_pairing.conn_handle, &d) && !ble_store_util_delete_peer(&d.peer_id_addr))
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    } else if (e->type == BLE_GAP_EVENT_ADV_COMPLETE)
        atomic_store(&refresh_adv, true);
    return 0;
}
static void advertise(void)
{
    if (!synced || phone != BLE_HS_CONN_HANDLE_NONE || ble_gap_adv_active())
        return;
    bool enroll = provision_pairing_open();
    if (!enroll && !owner_exists())
        return;
    struct ble_hs_adv_fields fields = { 0 }, rsp = { 0 };
    fields.flags = BLE_HS_ADV_F_BREDR_UNSUP | (enroll ? BLE_HS_ADV_F_DISC_GEN : 0);
    /* UUID/name are published only for physical enrollment. The owner app keeps
     * the board's address; normal advertising accepts only its bonded identity. */
    if (enroll) {
        fields.uuids128 = (ble_uuid128_t*)&svc;
        fields.num_uuids128 = 1;
        fields.uuids128_is_complete = 1;
        rsp.name = (uint8_t*)name;
        rsp.name_len = strlen(name);
        rsp.name_is_complete = 1;
    }
    int rc = ble_gap_adv_set_fields(&fields);
    if (!rc)
        rc = ble_gap_adv_rsp_set_fields(&rsp);
    struct ble_gap_adv_params params = { .conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = enroll ? BLE_GAP_DISC_MODE_GEN : BLE_GAP_DISC_MODE_NON, .itvl_min = 320, .itvl_max = 480, .filter_policy = enroll ? 0 : 3 };
    if (!enroll && !rc)
        rc = ble_gap_wl_set(owner_address(), 1);
    if (!rc)
        rc = ble_gap_adv_start(own_type, NULL, BLE_HS_FOREVER, &params, gap, NULL);
    if (rc) {
        ESP_LOGW(TAG, "Owner advertising failed code=%d", rc);
        atomic_store(&refresh_adv, true);
    }
}
void provision_init(void)
{
    lock = xSemaphoreCreateMutex();
    configASSERT(lock);
    owner_store_ok = owner_load();
    if (!owner_store_ok)
        ESP_LOGE(TAG, "Owner storage invalid; enrollment is disabled to preserve it");
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    strcpy(response, "{\"id\":0,\"ok\":false}");
}
void provision_sync(uint8_t type)
{
    own_type = type;
    uint8_t mac[6];
    ESP_ERROR_CHECK(ble_hs_id_copy_addr(type, mac, NULL));
    snprintf(uid, sizeof(uid), "%02X%02X%02X%02X%02X%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    snprintf(name, sizeof(name), "MESHBBS-%s", uid);
    ble_svc_gap_device_name_set(name);
    ble_svc_gatt_changed(1, UINT16_MAX);
    synced = true;
    atomic_store(&refresh_adv, true);
    ESP_LOGI(TAG, "Board %s; owner %s. BOOT: three taps, then hold 3s and release to enroll.", name, owner_exists() ? "enrolled" : "not enrolled");
}
void provision_reset(void)
{
    synced = false;
    phone = BLE_HS_CONN_HANDLE_NONE;
    atomic_store(&authorized, false);
}
void provision_tick(void)
{
    int64_t now = esp_timer_get_time();
    if (atomic_exchange(&enrollment_requested, false)) {
        deadline = now + 180000000;
        atomic_store(&open_window, true);
        atomic_store(&refresh_adv, true);
        if (phone != BLE_HS_CONN_HANDLE_NONE)
            ble_gap_terminate(phone, BLE_ERR_REM_USER_CONN_TERM);
        ble_link_reconnect();
        ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
        ble_hs_cfg.sm_mitm = 0;
        ESP_LOGI(TAG, "Physical owner enrollment open for 3 minutes");
    }
    if (provision_pairing_open() && now >= deadline) {
        atomic_store(&open_window, false);
        atomic_store(&refresh_adv, true);
    }
    if (phone != BLE_HS_CONN_HANDLE_NONE && !provision_authorized() && !provision_pairing_open() && now - connected_at > 30000000)
        ble_gap_terminate(phone, BLE_ERR_REM_USER_CONN_TERM);
    if (atomic_exchange(&refresh_adv, false)) {
        if (ble_gap_adv_active())
            ble_gap_adv_stop();
        return;
    }
    advertise();
}
void provision_button_init(void)
{
    gpio_config_t c = { .pin_bit_mask = 1ULL << GPIO_NUM_0, .mode = GPIO_MODE_INPUT, .pull_up_en = 1 };
    ESP_ERROR_CHECK(gpio_config(&c));
}
void provision_button_tick(void)
{
    int64_t now = esp_timer_get_time();
    bool down = !gpio_get_level(GPIO_NUM_0);
    if (sequence_at && now - sequence_at > 12000000) {
        clicks = 0;
        sequence_at = 0;
    }
    if (down) {
        if (!pressed_at)
            pressed_at = now;
        if (clicks == 3 && now - pressed_at >= 3000000)
            held = true;
    } else if (pressed_at) {
        int64_t duration = now - pressed_at;
        pressed_at = 0;
        if (held) {
            held = false;
            clicks = 0;
            sequence_at = 0;
            atomic_store(&enrollment_requested, true);
            diagnostics_record("Physical owner enrollment opened");
        } else if (duration >= 50000 && duration < 700000) {
            if (!clicks)
                sequence_at = now;
            if (++clicks > 3) {
                clicks = 0;
                sequence_at = 0;
            }
        } else {
            clicks = 0;
            sequence_at = 0;
        }
    }
}
bool provision_take(char* out, size_t capacity)
{
    bool have = false;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (pending && provision_authorized() && capacity > strlen(request)) {
        strcpy(out, request);
        memset(request, 0, sizeof(request));
        pending = false;
        have = true;
    }
    xSemaphoreGive(lock);
    return have;
}
void provision_reply(const char* json)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    snprintf(response, sizeof(response), "%s", json);
    xSemaphoreGive(lock);
}
status_led_mode_t provision_status(void)
{
    return provision_pairing_open() ? STATUS_PAIRING : (phone != BLE_HS_CONN_HANDLE_NONE && !provision_authorized() ? STATUS_CONNECTING : STATUS_WAITING);
}
