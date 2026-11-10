#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ENABLE_RGB_MATRIX_PIXEL_FRACTAL
#define RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#define RGB_MATRIX_EFFECT(name)
#define MATRIX_ROWS 6
#ifndef MATRIX_COLS
#define MATRIX_COLS 16
#endif
#define HAS_ANY_FLAGS(a, b) ((a) & (b))
#define RGB_MATRIX_USE_LIMITS(lo, hi) uint8_t lo = params->min, hi = params->max

typedef struct { uint8_t r, g, b; } rgb_t;
typedef struct { uint8_t init, min, max, flags; } effect_params_t;
static struct { uint8_t matrix_co[MATRIX_ROWS][MATRIX_COLS]; uint8_t flags[MATRIX_ROWS * MATRIX_COLS]; } g_led_config;
static struct { uint8_t speed; rgb_t hsv; } rgb_matrix_config;
static uint32_t g_rgb_timer;
static uint8_t pixels[MATRIX_ROWS * MATRIX_COLS][3];
static uint32_t rng = 1;
static uint8_t random8(void) { rng = rng * 1664525u + 1013904223u; return rng >> 24; }
static uint8_t qadd8(uint8_t x, uint8_t y) { unsigned n = x + y; return n > 255 ? 255 : n; }
static uint16_t scale16by8(uint16_t x, uint8_t y) { return (x * (uint16_t)(y + 1)) >> 8; }
static rgb_t rgb_matrix_hsv_to_rgb(rgb_t hsv) { return hsv; }
static void rgb_matrix_set_color(uint8_t i, uint8_t r, uint8_t g, uint8_t b) { pixels[i][0] = r; pixels[i][1] = g; pixels[i][2] = b; }
static void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b) { for (unsigned i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) rgb_matrix_set_color(i, r, g, b); }
static bool rgb_matrix_check_finished_leds(uint8_t max) { return max < MATRIX_ROWS * MATRIX_COLS; }
#include "../../quantum/rgb_matrix/animations/pixel_fractal_anim.h"
int main(void) {
    for (unsigned i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) { g_led_config.matrix_co[i / MATRIX_COLS][i % MATRIX_COLS] = i; g_led_config.flags[i] = (i % 5) ? 1 : 2; }
    rgb_matrix_config.speed = 123; rgb_matrix_config.hsv = (rgb_t){11, 22, 33};
    uint32_t hash = 2166136261u;
    for (unsigned frame = 0; frame < 400; frame++) {
        g_rgb_timer += 19;
        for (unsigned start = 0; start < MATRIX_ROWS * MATRIX_COLS; start += 17) {
            effect_params_t p = { frame == 0, start, start + 17 < MATRIX_ROWS * MATRIX_COLS ? start + 17 : MATRIX_ROWS * MATRIX_COLS, 1 };
            PIXEL_FRACTAL(&p);
        }
        for (unsigned i = 0; i < sizeof pixels; i++) hash = (hash ^ ((uint8_t *)pixels)[i]) * 16777619u;
    }
    printf("%08x\n", hash);
}
