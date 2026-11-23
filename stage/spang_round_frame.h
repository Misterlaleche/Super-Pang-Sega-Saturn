#ifndef SPANG_ROUND_FRAME_H
#define SPANG_ROUND_FRAME_H
#include <stdint.h>
#include "spang_panic.h"
/* Native FIXED05DE owner. These are byte layouts, not an interpreted CPU.
 * Effects:F800, floating scores:E780, stopped-time actor:E1E0, lucky:E880.
 * Item events and ordered queue7 command/parameter pairs are outputs for the
 * video/audio/score owners. seed is the original13BF state, shared by drops
 * and splits. A caller must preserve it across stages rather than reseeding. */
typedef struct {
 uint8_t fx[256],scores[128],stop[32],lucky[32],slow[32],sounds[8],queue[64],events[8];
 uint8_t queue_length;
 uint16_t seed;
 /* B237 synchronous awards, sampled before ED00. Triples: owner,BCDlo,BCDhi.
  * Score consumer commits these before queue7, once per tick. */
 uint8_t direct[24],direct_length;
} spang_round_frame;
void spang_round_frame_init(spang_round_frame *state,uint16_t seed);
/* Caller initializes cursor/remaining and object storage. Existing prefix
 * sprites (for example the stopped-time clock) are preserved. advance=0 is
 * a display-only view: no actor, counter, timer, item, RNG or owner mutation.
 * This owner is for Tour (E09A=0), not an implicit implementation of Panic.
 * Unknown active handlers return an error, never a made-up fallback. */
int spang_round_frame_step(spang_round_frame *state,uint8_t balls[1024],
 uint8_t items[256],uint8_t globals[128],uint8_t player1[128],uint8_t player2[128],
 const uint8_t bank1[16384],const uint8_t grid[2048],uint8_t objects[4096],
 uint16_t *cursor,uint8_t *remaining,uint8_t frame,int advance);
int spang_round_frame_panic_step(spang_round_frame *state,uint8_t balls[1024],
 uint8_t items[256],uint8_t globals[128],uint8_t player1[128],uint8_t player2[128],
 const uint8_t bank1[16384],const uint8_t grid[2048],uint8_t objects[4096],
 uint16_t *cursor,uint8_t *remaining,uint8_t frame,int advance,
 spang_panic *panic,const uint8_t fixed[32768]);
/* Panic keeps the same FIXED05DE actor order and uses g1A=2. It additionally
 * emits metadata at object18..23 for signed incoming-ball body bounds; the
 * original first four hardware bytes remain unchanged. The caller owns the
 * preceding scheduler, READY/time-stop, score sprites and following weapons. */
/* BANK0:B60D after player/item contacts. Samples one of the two lucky
 * actors on alternating quarters of the original frame counter. */
int spang_round_lucky_contact(spang_round_frame *state,const uint8_t player[128],uint8_t frame);
#endif
