#ifndef SPANG_ITEMS_H
#define SPANG_ITEMS_H
#include <stdint.h>
#include "spang_video_commands.h"
int spang_items_weapon_hud(unsigned type,unsigned owner,const uint8_t bank1[16384],spang_video_write write,void *context);
/* Events returned by tick; caller dispatches sound/score and time effects. */
enum { SPANG_ITEM_DRAW=1, SPANG_ITEM_SOUND=2, SPANG_ITEM_FREEZE=4,
 SPANG_ITEM_BOMB=8, SPANG_ITEM_SCORE=16, SPANG_ITEM_LIFE=32,
 SPANG_ITEM_SPECIAL=64, SPANG_ITEM_SLOWDOWN=4, SPANG_ITEM_TIMESTOP=8 };
int spang_items_shield_contact(uint8_t player[128],uint8_t balls[1024],uint8_t frame,uint8_t blocked);
void spang_items_shield_tick(uint8_t player[128],uint8_t frame);
int spang_items_shield_sprite(uint8_t player[128],uint8_t frame,const uint8_t bank0[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
void spang_items_init(uint8_t pool[256]);
void spang_items_stage(uint8_t globals[128],unsigned stage,const uint8_t bank5[16384]);
int spang_items_spawn_platform(uint8_t pool[256],unsigned index,unsigned x,unsigned y);
uint8_t spang_items_random(uint16_t *seed);
uint16_t spang_items_special_bonus(const uint8_t actor[32],const uint8_t balls[1024],const uint8_t bank1[16384]);
void spang_items_stop_start_draw(uint8_t stop[6],uint8_t clock[32],uint8_t globals[128],const uint8_t bank3[16384],uint8_t random);
/* Call while stop[0]!=0, before stop_tick, using bank3 data. */
int spang_items_stop_sprite(uint8_t clock[32],const uint8_t bank3[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
void spang_items_stop_start(uint8_t stop[6],uint8_t globals[128],const uint8_t bank3[16384],uint8_t random);
void spang_items_stop_tick(uint8_t stop[6],uint8_t globals[128]);
void spang_items_slow_start(uint8_t slow[4],uint8_t globals[128]);
int spang_items_slow_tick(uint8_t slow[4],uint8_t globals[128]);
int spang_items_spawn(uint8_t pool[256],unsigned type,unsigned x,unsigned y);
/* ROM 9460 regular ball pop / 94BD alternate spawn. random is 13BF output. */
int spang_items_drop_rng(uint8_t pool[256],uint8_t globals[128],const uint8_t source[32],const uint8_t bank1[16384],uint16_t *seed,int alternate);
int spang_items_drop(uint8_t pool[256],uint8_t globals[128],const uint8_t source[32],const uint8_t bank1[16384],uint8_t random,int alternate);
/* Returns 1 when collected, 0 on miss, -1 unsupported type. Player updates
 * are synchronous; deferred audiovisual/world effects occur on next tick. */
int spang_items_collect(uint8_t actor[32],uint8_t player[128],uint8_t globals[128]);
/* E080 globals, F000 grid, EA00 balls; return event mask, -1 unknown state.
 * DRAW uses existing spang_small_sprite with bank1 (advances animation).
 * SCORE: ROM table parameter=2<<actor29 for types0..6, otherwise2.
 * Current ROM awards100,200,400,800 for weapons;100 for other SCORE events.
 * SOUND: type14=0x13,type18/20=0x19,otherwise3. */
int spang_items_tick(uint8_t actor[32],uint8_t globals[128],const uint8_t grid[2048],uint8_t balls[1024],const uint8_t bank1[16384],uint8_t frame);
#endif
