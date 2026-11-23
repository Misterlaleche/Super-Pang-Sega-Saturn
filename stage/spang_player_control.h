#ifndef SPANG_PLAYER_CONTROL_H
#define SPANG_PLAYER_CONTROL_H
#include <stdint.h>
/* Bounded memory-effect translations, not the complete player update.
 * Return -1 for unsupported paths without changing actor. */
int spang_player_walk(uint8_t actor[128], int direction);
int spang_player_side(uint8_t actor[128], const uint8_t grid[2048]);
int spang_player_animate(uint8_t actor[128], const uint8_t bank0[16384]);
int spang_player_input(uint8_t actor[128], uint8_t input, uint8_t previous,
                       const uint8_t bank0[16384]);
/* BDE6..BE6E: emit the four 16x16 tiles of an ordinary player frame.
 * Cursor is an aligned offset into the 4096-byte object list, not a pointer. */
int spang_player_sprite(const uint8_t actor[128], uint8_t code, uint8_t attr,
                        uint8_t frame, uint8_t objects[4096],
                        uint16_t *cursor, uint8_t *remaining);
#endif
