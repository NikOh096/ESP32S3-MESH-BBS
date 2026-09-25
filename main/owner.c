#include "owner.h"
#include "host/ble_store.h"
#include "nvs.h"
#include "psa/crypto.h"
#include <string.h>
static struct {
    uint32_t version;
    ble_addr_t peer;
    uint8_t key[32];
} record;
static bool present;
bool owner_load(void)
{
    nvs_handle_t h;
    esp_err_t rc = nvs_open("owner", NVS_READONLY, &h);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        present = false;
        return true;
    }
    if (rc != ESP_OK)
        return false;
    size_t n = sizeof(record);
    rc = nvs_get_blob(h, "identity", &record, &n);
    nvs_close(h);
    if (rc == ESP_ERR_NVS_NOT_FOUND) {
        present = false;
        return true;
    }
    present = rc == ESP_OK && n == sizeof(record) && record.version == 1;
    return present;
}
bool owner_exists(void) { return present; }
bool owner_peer(const ble_addr_t* p) { return present && p->type == record.peer.type && !memcmp(p->val, record.peer.val, 6); }
const ble_addr_t* owner_address(void) { return &record.peer; }
bool owner_enroll(const ble_addr_t* peer, const uint8_t key[32])
{
    typeof(record) next = { .version = 1, .peer = *peer };
    memcpy(next.key, key, 32);
    nvs_handle_t h;
    esp_err_t rc = nvs_open("owner", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(h, "identity", &next, sizeof(next));
        if (rc == ESP_OK)
            rc = nvs_commit(h);
        nvs_close(h);
    }
    if (rc == ESP_OK) {
        /* Physical owner replacement revokes only the previous owner's bond. */
        if (present && !owner_peer(peer))
            ble_store_util_delete_peer(&record.peer);
        record = next;
        present = true;
    }
    memset(&next, 0, sizeof(next));
    return rc == ESP_OK;
}
bool owner_verify(const uint8_t nonce[32], const uint8_t proof[32], const char* id)
{
    if (!present || strlen(id) != 12)
        return false;
    uint8_t input[44], out[32] = { 0 };
    memcpy(input, nonce, 32);
    memcpy(input + 32, id, 12);
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key = 0;
    size_t n = 0;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, 256);
    psa_status_t rc = psa_crypto_init();
    if (rc == PSA_SUCCESS)
        rc = psa_import_key(&attr, record.key, 32, &key);
    if (rc == PSA_SUCCESS)
        rc = psa_mac_compute(key, PSA_ALG_HMAC(PSA_ALG_SHA_256), input, sizeof(input), out, sizeof(out), &n);
    if (key)
        psa_destroy_key(key);
    psa_reset_key_attributes(&attr);
    unsigned difference = 0;
    for (unsigned i = 0; i < 32; i++)
        difference |= out[i] ^ proof[i];
    memset(out, 0, sizeof(out));
    return rc == PSA_SUCCESS && n == 32 && !difference;
}
static int digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
bool owner_unhex(const char* s, uint8_t* out, unsigned n)
{
    if (!s || strlen(s) != n * 2)
        return false;
    for (unsigned i = 0; i < n; i++) {
        int a = digit(s[i * 2]), b = digit(s[i * 2 + 1]);
        if (a < 0 || b < 0)
            return false;
        out[i] = (a << 4) | b;
    }
    return true;
}
void owner_hex(const uint8_t* b, unsigned n, char* out)
{
    const char h[] = "0123456789abcdef";
    for (unsigned i = 0; i < n; i++) {
        out[2 * i] = h[b[i] >> 4];
        out[2 * i + 1] = h[b[i] & 15];
    }
    out[2 * n] = 0;
}
