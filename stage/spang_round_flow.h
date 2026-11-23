#ifndef SPANG_ROUND_FLOW_H
#define SPANG_ROUND_FLOW_H
#include <stdint.h>
/* Fixed 377E,38CC,391F,393A,3967,3985. Call once after the current phase's
 * actor/bonus work: world during delay, clear actor/player during slide,
 * bonus/player during award/hold. g is E080..E0FF, a is E1E0.
 * events[0]=sound; events[1] bits: 1=8706 object mask, 2=map handoff,
 * 4=next-stage bootstrap, 8=ending handoff. Map/ending are separate owners.
 * Returns 0 handled, 1 outside supported control phases, -1 invalid state.
 * No repeated handoff: map/ending states are left for their owner. */
int spang_round_flow(uint8_t g[128],uint8_t p1[128],uint8_t p2[128],uint8_t a[32],const uint8_t fixed[32768],uint8_t events[2]);
/* FIXED3A1D..3A42: common advance after bonus, including map completion.
 * Preserves RNG/player/session state (not passed); clears exactly the stage
 * use flags and retries. Phase39 is owned by the ending, never wraps to0. */
int spang_round_advance(uint8_t globals[128],const uint8_t fixed[32768]);
/* FIXED37C7: account one death, clear both per-player life-end markers,
 * increment E08F and undo the current time speed increment E0D7. */
int spang_round_retry_account(uint8_t globals[128],uint8_t player1[128],
 uint8_t player2[128],const uint8_t fixed[32768]);
#endif
