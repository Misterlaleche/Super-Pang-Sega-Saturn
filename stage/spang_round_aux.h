#ifndef SPANG_ROUND_AUX_H
#define SPANG_ROUND_AUX_H
#include "spang_round_frame.h"
/* BANK3:9374, called before FIXED05DE. Random clocks consume13BF here,
 * not when an item first requests the clock. No actor mutation on draw-only. */
int spang_round_stop_step(spang_round_frame *state,uint8_t globals[128],
 const uint8_t bank3[16384],uint8_t objects[4096],uint16_t *cursor,
 uint8_t *remaining,int advance);
/* BANK2:A085: native scheduler for all eight original16-byte E780 slots. */
int spang_round_scores_step(spang_round_frame *state,const uint8_t bank2[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance);
/* BANK2:9562/956B..95D6, followed by FIXED2F14 velocity tables. */
int spang_round_slow_step(spang_round_frame *state,uint8_t globals[128],
 const uint8_t fixed[32768],int advance);
/* Fixed03B3 subset: stopped-time actor, floating scores, then05DE.
 * Player, weapons, animals and clock/slow service remain separate owners. */
int spang_round_objects_step(spang_round_frame *state,uint8_t balls[1024],
 uint8_t items[256],uint8_t globals[128],uint8_t player1[128],uint8_t player2[128],
 const uint8_t bank1[16384],const uint8_t bank2[16384],const uint8_t bank3[16384],
 const uint8_t grid[2048],uint8_t objects[4096],uint16_t *cursor,
 uint8_t *remaining,uint8_t frame,int advance);
/* A085 including the stage music owner in E780 slot0. selection is the raw
 * FCC2 selector (0C chooses the alternate table), not Stereo/Mono. attract
 * is FCED. request={valid,command} represents a direct2BAF call; raw2BE3
 * writes still enter state.sounds. Paused view does not change either. */
int spang_round_scores_step_with_music(spang_round_frame *state,const uint8_t globals[128],
 uint8_t selection,uint8_t attract,uint8_t request[2],const uint8_t bank2[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance);
int spang_round_objects_step_with_music(spang_round_frame *state,uint8_t balls[1024],
 uint8_t items[256],uint8_t globals[128],uint8_t player1[128],uint8_t player2[128],
 const uint8_t bank1[16384],const uint8_t bank2[16384],const uint8_t bank3[16384],
 const uint8_t grid[2048],uint8_t objects[4096],uint16_t *cursor,
 uint8_t *remaining,uint8_t frame,int advance,uint8_t selection,uint8_t attract,uint8_t request[2]);
#endif
