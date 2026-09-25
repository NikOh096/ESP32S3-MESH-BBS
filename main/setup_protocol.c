#include "setup_protocol.h"
#include <string.h>

static bool valid_name(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n;) {
        uint32_t cp = p[i++], minimum = 0; unsigned tail = 0;
        if (cp < 0x20 || cp == 0x7f) return false;
        if (cp < 0x80) continue;
        if (cp >= 0xc2 && cp <= 0xdf) { tail = 1; cp &= 0x1f; minimum = 0x80; }
        else if (cp >= 0xe0 && cp <= 0xef) { tail = 2; cp &= 15; minimum = 0x800; }
        else if (cp >= 0xf0 && cp <= 0xf4) { tail = 3; cp &= 7; minimum = 0x10000; }
        else return false;
        while (tail--) {
            if (i == n || (p[i] & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (p[i++] & 63);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}

bool setup_decode(const uint8_t *data, size_t size, const settings_t *current, settings_t *result)
{
    if (!data || !current || !result || size < 8 || data[0] != 1 || data[1] > 7) return false;
    size_t n = data[2];
    if (!n || n >= sizeof(result->target) || size != 7 + n || !valid_name(data + 7, n)) return false;
    uint32_t raw = (uint32_t)data[3] | (uint32_t)data[4] << 8 | (uint32_t)data[5] << 16 | (uint32_t)data[6] << 24;
    int32_t pin;
    if (raw == UINT32_MAX) pin = -1;
    else if (raw == UINT32_MAX - 1) pin = current->pin;
    else if (raw <= 999999) pin = (int32_t)raw;
    else return false;
    if (pin < -1 || pin > 999999) return false;
    settings_t next = {.pin = pin, .hops = data[1]};
    memcpy(next.target, data + 7, n);
    *result = next;
    return true;
}

size_t setup_status(const settings_t *s, bool saved, uint8_t *out, size_t capacity)
{
    size_t n = strlen(s->target);
    if (n >= sizeof(s->target) || capacity < n + 4) return 0;
    out[0] = 1; out[1] = (s->pin >= 0 ? 1 : 0) | (saved ? 2 : 0);
    out[2] = s->hops; out[3] = (uint8_t)n;
    memcpy(out + 4, s->target, n);
    return n + 4;
}
