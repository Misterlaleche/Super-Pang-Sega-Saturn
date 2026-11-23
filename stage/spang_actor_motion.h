#ifndef SPANG_ACTOR_MOTION_H
#define SPANG_ACTOR_MOTION_H
#include <stdint.h>

/* Original 32-byte actor records, in arcade coordinates. These translate
 * only the memory effects of fixed ROM 12FC..134C, not collision/bounce,
 * actor dispatch, CPU flags, or frame scheduling. actor must be non-null. */
void spang_actor_step_x(uint8_t actor[32]);
void spang_actor_step_y(uint8_t actor[32], uint16_t incoming_delta);
#endif
