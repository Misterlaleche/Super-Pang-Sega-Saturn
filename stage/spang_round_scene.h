#ifndef SPANG_ROUND_SCENE_H
#define SPANG_ROUND_SCENE_H
#include <stdint.h>
typedef uint8_t (*spang_round_random_fn)(void *context);
/* Complete ball dispatcher: globals are E080..E0FF, including difficulty,
 * speeds, diamond vectors and E0AA scratch. Only random split families call
 * the RNG owner, once per split. No RNG is consumed by draw-only frames. */
int spang_round_scene_step_full(uint8_t actors[1024],uint8_t objects[4096],
 uint8_t *remaining,const uint8_t bank[16384],const uint8_t grid[2048],
 uint8_t globals[128],int advance,spang_round_random_fn random,void *context);
/* 32-slot diagnostic pool EA00..EDFF. No score/items/damage scheduler. */
int spang_round_scene_step(uint8_t actors[1024],uint8_t objects[4096],
 uint8_t *remaining,const uint8_t bank[16384],const uint8_t grid[2048],
 uint16_t step,uint16_t dx,int advance);
/* advance=0 freezes all actor bytes. freeze=1 pauses only movement handlers;
 * initialization, hit transitions and forced splits still run. hide suppresses
 * frozen movement sprites (ROM9458), not the draw in pending hit handlers.
 * Caller owns 912E drop selection and popped BCD count; it awards no score. */
int spang_round_scene_step_ex(uint8_t actors[1024],uint8_t objects[4096],
 uint8_t *remaining,const uint8_t bank[16384],const uint8_t grid[2048],
 uint16_t step,uint16_t dx,int advance,int freeze,int hide);
#endif
