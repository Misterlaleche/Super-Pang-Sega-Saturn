#ifndef SPANG_MOTION_BONUS_H
#define SPANG_MOTION_BONUS_H
#include <stdint.h>
/* Connect ROM2D28 to the player's original fields after a score award.
 * effects: sound13 request and original HUD redraw request (not playback).
 * Supports all five original thresholds; caller preserves index/threshold
 * across death, resets them for a new game, and redraws current life state. */
int spang_motion_bonus(uint8_t player[128],const uint8_t table[8],uint8_t effects[2]);
#endif
