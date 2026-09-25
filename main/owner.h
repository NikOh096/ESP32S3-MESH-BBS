#pragma once
#include "host/ble_gap.h"
#include <stdbool.h>
#include <stdint.h>
bool owner_load(void);
bool owner_exists(void);
bool owner_peer(const ble_addr_t*);
const ble_addr_t* owner_address(void);
bool owner_enroll(const ble_addr_t*, const uint8_t key[32]);
bool owner_verify(const uint8_t nonce[32], const uint8_t proof[32], const char* device_id);
bool owner_unhex(const char*, uint8_t*, unsigned);
void owner_hex(const uint8_t*, unsigned, char*);
