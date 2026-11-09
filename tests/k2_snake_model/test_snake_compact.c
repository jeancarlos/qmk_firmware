#include <assert.h>
#include <stdint.h>
#include "k2_snake_compact.h"

/* A path with a branch at its tail forces the reversal case. */
static const uint8_t edges[10][3] = {
    {1, 255, 255}, {0, 2, 255}, {1, 3, 255}, {2, 4, 255},
    {3, 5, 255}, {4, 6, 255}, {5, 7, 255}, {6, 8, 255},
    {7, 9, 255}, {8, 255, 255}
};
uint8_t snake_neighbor(uint8_t led, uint8_t slot) { return slot < 3 ? edges[led][slot] : 255; }

int main(void) {
    const uint8_t body[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    snake_state s;
    snake_init(&s, body);
    assert(sizeof(s) == 19);
    snake_press(&s, 0); /* safe zone */
    assert(!snake_food(&s, 0));
    snake_press(&s, 8);
    snake_press(&s, 83);
    snake_press(&s, 84);
    assert(snake_food(&s, 8) && snake_food(&s, 83));
    assert(snake_step(&s, 0));
    assert(s.body[0] == 8 && s.body[1] == 7 && s.body[7] == 1);
    assert(!snake_food(&s, 8) && snake_food(&s, 83));
    return 0;
}
