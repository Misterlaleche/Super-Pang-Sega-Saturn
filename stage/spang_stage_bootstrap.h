#ifndef SPANG_STAGE_BOOTSTRAP_H
#define SPANG_STAGE_BOOTSTRAP_H
#include <stdint.h>
enum { SPANG_TOUR_STAGE_COUNT=40,SPANG_STAGE_TABLE_COUNT=42 };
/* BANK5:84E3. Creation data for all 40 Tour phases. This does not certify
 * runtime motion/split/score handlers for every created actor class.
 * The stage owner clears the pool (81BD); no captured state is used. */
int spang_stage_balls(unsigned stage,uint8_t actors[1024],const uint8_t bank5[16384],uint8_t *weight);
void spang_stage_pool_clear(uint8_t actors[1024]);
void spang_stage_grid_clear(uint8_t grid[2048]);
/* 85A9 / 86A5 / 87D0 / 8827, in ROM order: permanent, destructible,
 * invisible H/V and special surfaces. Invalid tables cause no writes. */
int spang_stage_platforms(unsigned stage,uint8_t grid[2048],uint8_t attrs[2048],uint8_t chars[4096],const uint8_t bank5[16384]);
/* 88C3. Eight 16-byte ladder descriptors, cleared separately by stage owner.
 * Creation is not climbing AI or the ladder renderer. */
int spang_stage_ladders(unsigned stage,uint8_t ladders[128],const uint8_t bank5[16384]);
void spang_stage_player(uint8_t player[128],unsigned stage,const uint8_t bank0[16384]);
#endif
