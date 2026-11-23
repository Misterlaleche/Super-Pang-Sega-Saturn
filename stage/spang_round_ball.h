#ifndef SPANG_ROUND_BALL_H
#define SPANG_ROUND_BALL_H
#include "spang_small_ball.h"
/* Diamond types 4/16/28 additionally read their original little-endian
 * linear velocity words through read(context,E0B0..E0B7). */
/* Full owner additionally receives E0A6 before collision correction. Tiny
 * types0/2/4 leave it unchanged; frozen/error paths do not publish it. */
int spang_round_ball_update_full(uint8_t actor[32],uint16_t step,uint16_t dx,
 uint8_t freeze,spang_ball_read_fn read,void *context,uint8_t *flag,uint8_t scratch_x[2]);
int spang_round_ball_update(uint8_t actor[32],uint16_t step,uint16_t dx,
 uint8_t freeze,spang_ball_read_fn read,void *context,uint8_t *flag);
int spang_round_ball_init(uint8_t actor[32],uint8_t level,const uint8_t bank[16384]);
int spang_round_ball_sprite(uint8_t actor[32],const uint8_t bank[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
/* For types22/26/34 random must be the next13BF byte supplied by the caller.
 * E0AA scratch is returned through flag. Other types do not consume random.
 * Returns2 or4 child writes; saturated partitions retain ROM IX aliasing. */
int spang_round_ball_split_ex(uint8_t actor[32],uint8_t pool[1024],
 const uint8_t bank[16384],uint8_t random,uint8_t *flag);
int spang_round_ball_split(uint8_t actor[32],uint8_t pool[1024],const uint8_t bank[16384]);
#endif
