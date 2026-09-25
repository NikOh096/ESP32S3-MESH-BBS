#include "meshcore_protocol.h"
#include "bulletins.h"
#include <string.h>
/* Independent implementation from the pinned companion-v1.17.1 wire layout.
 * Device-query protocol v2 keeps a 160-byte DM within a single 173-byte ATT value
 * on ESP32 radios whose MTU is 176. No private-key or PIN queries are exposed. */
static uint32_t u32(const uint8_t* b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static void put32(uint8_t* b, uint32_t n)
{
    for (unsigned i = 0; i < 4; i++)
        b[i] = (uint8_t)(n >> (i * 8));
}
bool mc_key_valid(const uint8_t key[32])
{
    unsigned any = 0;
    for (unsigned i = 0; i < 32; i++)
        any |= key[i];
    return any != 0;
}
static bool fixed_text(char* out, size_t capacity, const uint8_t* src, size_t n, bool spaces)
{
    if (n >= capacity)
        return false;
    memcpy(out, src, n);
    out[n] = 0;
    if (memchr(src, 0, n))
        return false;
    if (spaces)
        for (size_t i = 0; i < n; i++)
            if (out[i] == '\r' || out[i] == '\n' || out[i] == '\t')
                out[i] = ' ';
    return !n || bulletin_text_valid(out, capacity - 1);
}
static void field(char* out, size_t capacity, const uint8_t* src, size_t n)
{
    size_t len = 0;
    while (len < n && src[len])
        len++;
    if (len >= capacity)
        len = capacity - 1;
    while (len && len < n && (src[len] & 0xc0) == 0x80)
        --len;
    if (!fixed_text(out, capacity, src, len, true))
        strcpy(out, "Unknown");
}
bool mc_parse(const uint8_t* b, size_t n, mc_packet_t* p)
{
    if (!b || !p || !n || n > MC_FRAME_MAX)
        return false;
    memset(p, 0, sizeof(*p));
    p->type = b[0];
    switch (p->type) {
    case MC_OK:
    case MC_EMPTY:
    case MC_WAITING:
    case MC_CONTACTS_FULL:
        return n == 1;
    case MC_ERROR:
        if (n != 2)
            return false;
        p->error = b[1];
        return true;
    case MC_SELF:
        if (n < 58 || n > 90)
            return false;
        memcpy(p->key, b + 4, 32);
        field(p->name, sizeof(p->name), b + 58, n - 58);
        return mc_key_valid(p->key);
    case MC_DEVICE:
        if (n < 80)
            return false;
        /* bytes 4..7 contain the radio PIN. Deliberately do not retain it. */
        p->fixed_pin = u32(b + 4) != 0; /* Policy only; do not retain the secret. */
        field(p->hardware, sizeof(p->hardware), b + 20, 40);
        field(p->version, sizeof(p->version), b + 60, 20);
        return true;
    case MC_CONTACTS_START:
    case MC_CONTACTS_END:
    case MC_TIME:
        if (n != 5)
            return false;
        p->count = p->timestamp = u32(b + 1);
        return true;
    case MC_CONTACT:
    case MC_NEW_CONTACT:
        if (n != 148)
            return false;
        memcpy(p->key, b + 1, 32);
        p->contact_type = b[33];
        field(p->name, sizeof(p->name), b + 100, 32);
        return mc_key_valid(p->key);
    case MC_ADVERT:
    case MC_PATH:
    case MC_CONTACT_DELETED:
        if (n != 33)
            return false;
        memcpy(p->key, b + 1, 32);
        return mc_key_valid(p->key);
    case MC_ACK:
        if (n != 9)
            return false;
        p->ack = u32(b + 1);
        return true;
    case MC_SENT:
        if (n != 10 || b[1] > 1)
            return false;
        p->ack = u32(b + 2);
        p->timeout_ms = u32(b + 6);
        return true;
    case MC_MESSAGE:
    case MC_MESSAGE_V3: {
        size_t o = p->type == MC_MESSAGE ? 1 : 4;
        if (n <= o + 12 || n > o + 12 + MC_TEXT_MAX)
            return false;
        memcpy(p->prefix, b + o, 6);
        p->text_type = b[o + 7];
        p->timestamp = u32(b + o + 8);
        /* CLI/admin data and signed room relays are never public BBS commands. */
        if (p->text_type != 0)
            return true;
        return fixed_text(p->text, sizeof(p->text), b + o + 12, n - o - 12, true);
    }
    case MC_CHANNEL:
    case MC_CHANNEL_V3:
        return n >= (p->type == MC_CHANNEL ? 8u : 11u);
    default:
        return false;
    }
}
static size_t one(uint8_t* b, size_t cap, uint8_t code)
{
    if (cap < 1)
        return 0;
    b[0] = code;
    return 1;
}
size_t mc_start(uint8_t* b, size_t cap)
{
    if (cap < 15)
        return 0;
    memset(b, 0, 15);
    b[0] = 1;
    memcpy(b + 8, "MESHBBS", 7);
    return 15;
}
size_t mc_query(uint8_t* b, size_t cap)
{
    if (cap < 2)
        return 0;
    b[0] = 22;
    b[1] = 2;
    return 2;
}
size_t mc_get_contacts(uint8_t* b, size_t cap) { return one(b, cap, 4); }
size_t mc_get_time(uint8_t* b, size_t cap) { return one(b, cap, 5); }
size_t mc_next(uint8_t* b, size_t cap) { return one(b, cap, 10); }
size_t mc_send(const uint8_t key[32], const char* text, uint32_t stamp, uint8_t* b, size_t cap)
{
    if (!key || !mc_key_valid(key) || !text || stamp < 1700000000)
        return 0;
    size_t n = strlen(text);
    char checked[MC_TEXT_MAX + 1];
    if (!n || !fixed_text(checked, sizeof(checked), (const uint8_t*)text, n, true) || cap < 13 + n)
        return 0;
    b[0] = 2;
    b[1] = 0;
    b[2] = 0;
    put32(b + 3, stamp);
    memcpy(b + 7, key, 6);
    memcpy(b + 13, text, n);
    return 13 + n;
}
uint32_t mc_request_id(const uint8_t key[32], uint32_t stamp, const char* text)
{
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < 32; i++)
        h = (h ^ key[i]) * 16777619u;
    for (unsigned i = 0; i < 4; i++)
        h = (h ^ ((stamp >> (8 * i)) & 255)) * 16777619u;
    for (; *text; text++)
        h = (h ^ (uint8_t)*text) * 16777619u;
    return !h || h == UINT32_MAX ? 1 : h;
}
