/* Host experiment only: no storage, renderer, timing or hardware integration. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

enum { SNAKE_LEDS = 84, SNAKE_LENGTH = 8, SNAKE_NEIGHBORS = 5, SNAKE_NONE = 255 };
typedef struct { uint8_t body[SNAKE_LENGTH]; uint8_t food[(SNAKE_LEDS + 7) / 8]; } snake_state;
uint8_t snake_neighbor(uint8_t led, uint8_t slot); /* caller supplies flash-backed graph */
void snake_init(snake_state *s, const uint8_t body[SNAKE_LENGTH]);
void snake_press(snake_state *s, uint8_t led);
bool snake_food(const snake_state *s, uint8_t led);
bool snake_step(snake_state *s, uint8_t random_byte);
