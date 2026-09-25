#pragma once
#include <stddef.h>
#include "node_scan.h"
#include "settings.h"
#include "status_led.h"

typedef enum { LINK_DATA,
    LINK_UP,
    LINK_DOWN,
    LINK_TX_FAILED,
    LINK_ERROR } link_event_kind_t;
typedef struct {
    link_event_kind_t kind;
    uint16_t length;
    uint8_t bytes[1024];
} link_event_t;

void ble_link_start(const settings_t* settings);
void ble_link_tick(void);
bool ble_link_receive(link_event_t* event);
bool ble_link_send(const uint8_t* data, size_t size);
void ble_link_reconnect(void);
status_led_mode_t ble_link_status(void);
void ble_link_configure(const settings_t* settings);
bool ble_link_pair(void);
bool ble_link_submit_pin(uint32_t pin);
const char* ble_link_pair_state(void);
bool ble_link_scan_nodes(void);
bool ble_link_scan_result(unsigned index, node_scan_entry_t* entry, unsigned* count, bool* scanning);
bool ble_link_selectable(const char* address);
bool ble_link_select_node(const settings_t* settings);
bool ble_link_cancel_pair(void);
uint32_t ble_link_pair_attempt(void);
bool ble_link_pin_for_attempt(uint32_t pin, uint32_t attempt);
const char* ble_link_pair_stage(void);
int ble_link_last_error(void);
