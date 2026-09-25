#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "status_led.h"
/* One NimBLE host shared with the node link; owner GATT runs concurrently. */
void provision_init(void);
void provision_sync(uint8_t address_type);
void provision_reset(void);
void provision_tick(void);
void provision_button_init(void);
void provision_button_tick(void);
bool provision_pairing_open(void);
bool provision_authorized(void);
const char *provision_id(void);
bool provision_take(char *out, size_t capacity);
void provision_reply(const char *json);
status_led_mode_t provision_status(void);
