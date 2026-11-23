#ifndef SPANG_BONUS_LIFE_H
#define SPANG_BONUS_LIFE_H
#include <stdint.h>
/* Decision/memory slice 2D28..2DAE, stopping at audio/UI request boundaries.
 * state: IX+4A, +4B, +4C, +60 (index, threshold low/high, packed BCD lives).
 * score_high: IX+40:+41, most significant byte first, original BCD bits.
 * table: 8 bytes addressed by FCEA; index8 uses hardcoded threshold9000.
 * effects: [request sound13 via2BC0, redraw via166B], booleans. These are
 * requests only; callees' memory/CPU/device effects are NOT implemented here.
 * Valid indices0,2,4,6,8,10. Return number of awards, or -1 for unsupported
 * index without changing state/effects. Other inputs are original bytes. */
int spang_bonus_life(uint8_t state[4], uint16_t score_high, const uint8_t table[8],
                     uint8_t audio_mode, uint8_t audio_enabled,
                     uint8_t ui_disabled, uint8_t effects[2]);
#endif
