#ifndef SPANG_ROUND_REWARDS_H
#define SPANG_ROUND_REWARDS_H
#include "spang_round_frame.h"
/* Consume ordered queue7 score requests (10/12 table,14/16 +50 BCD).
 * Pops have already been counted by05DE. Returns consumed command count;
 * invalid commands/bonus indices leave all caller state unchanged.
 * Uses the original best-score comparison, including its last-byte quirk.
 * Bonus-life sound/HUD requests remain requests, not audio playback. */
int spang_round_reward_queue(spang_round_frame *state,uint8_t player1[128],
 uint8_t player2[128],const uint8_t score_table[257],uint8_t best[4],
 const uint8_t bonus_table[8]);
#endif
