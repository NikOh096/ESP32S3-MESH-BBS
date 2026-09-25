#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char target[64]; /* Exact advertising name or AA:BB:CC:DD:EE:FF */
    int32_t pin;     /* -3 = interactive pairing; -1 = explicit NO_PIN; >=0 legacy fixed PIN */
    uint8_t hops;
} settings_t;

bool settings_load(settings_t *settings);
bool settings_save(const settings_t *settings);
