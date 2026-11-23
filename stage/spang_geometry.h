#ifndef SPANG_GEOMETRY_H
#define SPANG_GEOMETRY_H
#include <stdint.h>

/* Position result of fixed ROM 12FC..131B: low 24 bits of Q8 position,
 * signed 16-bit displacement supplied as its bit pattern.
 * CPU registers/flags and caller side effects are outside this interface. */
uint32_t spang_step_x(uint32_t position_q8, uint16_t delta_bits);

/* Geometric decision of bank 2, 8A2A..8AC1. Inputs preserve the original
 * B/C/D/E values and IY+0D (y); they are NOT Saturn screen coordinates.
 * Returns 1 iff the routine reaches 8FE9. Does not apply hit effects.
 * Selectors other than 1..4 use the original default (profile 5). */
int spang_harpoon_overlap(uint8_t selector, uint8_t b, uint8_t c,
                          uint8_t d, uint8_t e, uint8_t y);
#endif
