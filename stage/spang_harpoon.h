#ifndef SPANG_HARPOON_H
#define SPANG_HARPOON_H
#include <stdint.h>
int spang_harpoon_init(uint8_t actor[32], const uint8_t bank2[16384]);
int spang_harpoon_hit(uint8_t actor[32], uint8_t ball[32], uint8_t player[128]);
int spang_harpoon_break(uint8_t actor[32], uint8_t grid[2048]);
int spang_harpoon_flight(uint8_t actor[32], uint8_t frame,
                         const uint8_t bank2[16384], const uint8_t grid[2048]);
/* Optional sprite snapshot preserves 80CC's draw-before-collision ordering. */
int spang_harpoon_flight_draw(uint8_t actor[32], uint8_t frame,
                         const uint8_t bank2[16384], const uint8_t grid[2048],
                         uint8_t draw[32]);
int spang_harpoon_sprite(const uint8_t actor[32], const uint8_t bank2[16384],
                         uint8_t objects[4096], uint16_t *cursor,
                         uint8_t *remaining);
#endif
