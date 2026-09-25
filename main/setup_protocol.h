#pragma once
#include <stddef.h>
#include <stdint.h>
#include "settings.h"

/* v1: version, hops, target length, signed PIN int32 LE, target UTF-8.
 * PIN -1 = no PIN, -2 = retain existing PIN. No PIN is ever read back. */
bool setup_decode(const uint8_t *data, size_t size, const settings_t *current, settings_t *result);
size_t setup_status(const settings_t *settings, bool saved, uint8_t *out, size_t capacity);
