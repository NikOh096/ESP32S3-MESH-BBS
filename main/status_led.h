#pragma once
#include <stdbool.h>

typedef enum { STATUS_WAITING, STATUS_PAIRING, STATUS_CONNECTING, STATUS_READY } status_led_mode_t;

void status_led_init(void);
/* Call from the main task; BLE callbacks only publish their connection state. */
void status_led_tick(status_led_mode_t mode);
void status_led_unread(bool unread);
