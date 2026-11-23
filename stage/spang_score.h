#ifndef SPANG_SCORE_H
#define SPANG_SCORE_H
#include <stdint.h>
/* ROM 2D0C and 2D04 effects on E240..E243 (or equivalent player block).
 * Four original packed-BCD bytes, most significant first. amount contains
 * original D:E bytes, not a binary integer number of points. The low entry
 * first adds low to score[3]. Preserves original DAA even for non-BCD inputs.
 * Overflow saturates to 99 99 99 90. No display/lives/audio side effects. */
void spang_score_add(uint8_t score[4], uint16_t amount);
void spang_score_add_low(uint8_t score[4], uint16_t amount, uint8_t low);
/* ROM 2DB1..2DE1 comparison/copy only. Deliberately compares score[3] to
 * best[2], repeating E156 exactly as the ROM does. Returns 1 iff copied.
 * Arrays may be identical, otherwise must not overlap. No HUD tail call. */
int spang_score_promote(uint8_t best[4], const uint8_t score[4]);
#endif
