#include "k2_snake_compact.h"
#include <string.h>

void snake_init(snake_state *s, const uint8_t body[SNAKE_LENGTH]) {
    memset(s->food, 0, sizeof(s->food));
    memcpy(s->body, body, sizeof(s->body));
}
bool snake_food(const snake_state *s, uint8_t led) {
    return led < SNAKE_LEDS && (s->food[led / 8] & (1u << (led % 8)));
}
void snake_press(snake_state *s, uint8_t led) {
    if (led >= SNAKE_LEDS) return;
    for (uint8_t i = 0; i < SNAKE_LENGTH; ++i) if (s->body[i] == led) return;
    s->food[led / 8] |= 1u << (led % 8);
}
static bool available(const snake_state *s, uint8_t led, bool reverse) {
    if (led >= SNAKE_LEDS) return false;
    for (uint8_t i = reverse ? 1 : 0; i < (reverse ? SNAKE_LENGTH : SNAKE_LENGTH - 1); ++i)
        if (s->body[i] == led) return false;
    return true;
}
bool snake_step(snake_state *s, uint8_t random_byte) {
    for (uint8_t reverse = 0; reverse < 2; ++reverse) {
        uint8_t count = 0, end = s->body[reverse ? SNAKE_LENGTH - 1 : 0];
        for (uint8_t i = 0; i < SNAKE_NEIGHBORS; ++i)
            count += available(s, snake_neighbor(end, i), reverse);
        if (!count) continue;
        uint8_t pick = random_byte % count, led = SNAKE_NONE;
        for (uint8_t i = 0; i < SNAKE_NEIGHBORS; ++i) {
            uint8_t candidate = snake_neighbor(end, i);
            if (available(s, candidate, reverse) && pick-- == 0) { led = candidate; break; }
        }
        if (reverse) for (uint8_t i = 0; i < SNAKE_LENGTH / 2; ++i) {
            uint8_t tmp = s->body[i]; s->body[i] = s->body[SNAKE_LENGTH - 1 - i]; s->body[SNAKE_LENGTH - 1 - i] = tmp;
        }
        for (uint8_t i = SNAKE_LENGTH - 1; i > 0; --i) s->body[i] = s->body[i - 1];
        s->body[0] = led;
        s->food[led / 8] &= ~(1u << (led % 8));
        return true;
    }
    return false;
}
