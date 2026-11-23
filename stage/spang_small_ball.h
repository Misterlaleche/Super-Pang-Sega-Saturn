#ifndef SPANG_SMALL_BALL_H
#define SPANG_SMALL_BALL_H
#include "spang_ball_phase.h"
/* One update of bank-1 states 8357/8395/83D5/8416, ending BEFORE 20FB.
 * phase_step=E0A2, dx=E0A4 (unsigned magnitude), freeze=E0AB.
 * Returns 0 on completion, -1 unknown state, -2 unsupported freeze/special
 * response. Errors preserve actor and E0AA. Reader must be side-effect-free.
 * This does not dispatch/suspend tasks, draw sprites, or advance a frame. */
int spang_small_ball_update(uint8_t actor[32], uint16_t phase_step,
    uint16_t dx, uint8_t freeze, spang_ball_read_fn read, void *context,
    uint8_t *flag_e0aa);
#endif
