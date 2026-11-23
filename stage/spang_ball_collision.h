#ifndef SPANG_BALL_COLLISION_H
#define SPANG_BALL_COLLISION_H
#include "spang_ball_phase.h"
/* Small-ball bank-1 helpers: entry 9E10/9E21 walls, 9E70 ceiling,
 * 9EAE floor. x and bc retain caller HL/BC bits, in arcade coordinates.
 * read must provide bank-1 ROM data and live collision grid F000..F7FF.
 * Returns 0 no hit, 1 hit, -1 unknown entry, -2 unported special floor
 * response for actor type >=26 hex. Negative results preserve actor.
 * Does not expose CPU flags or translate callers/scheduler/rendering. */
int spang_ball_collision(uint8_t actor[32], uint16_t entry, uint16_t x,
    uint16_t bc, spang_ball_read_fn read, void *context);
#endif
