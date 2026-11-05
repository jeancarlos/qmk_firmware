#pragma once
#include <stdint.h>
typedef struct {
    uint8_t r, g, b;
} RGB;
extern struct led_config {
    uint8_t matrix_co[6][16];
} g_led_config;
