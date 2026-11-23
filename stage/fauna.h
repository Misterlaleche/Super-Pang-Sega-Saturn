#ifndef R04_FAUNA_H
#define R04_FAUNA_H
#include <stdint.h>
#include "spang_round_frame.h"
#include "crocodile.h"
/* Explicit owners of the original FA00 pool and E1A0 scheduler. Original
 * byte layouts and pool addresses are retained for linked bird flocks. */
typedef struct {
 uint8_t *actors,*scheduler,*globals,*player1,*player2,*balls,*items,*timer;
 const uint8_t *grid,*bank4,*bank1,*fixed;
 spang_round_frame *round;
 crocodile_sound_fn sound;
 void *sound_context;
 uint8_t input1,input2; /* Original FCC0/FCC2 held bits, not Saturn masks. */
 uint8_t *transients; /* E680..E77F impact/bubble pools, separate from F800. */
} fauna_context;
void fauna_init(uint8_t scheduler[32],uint8_t actors[320],uint8_t globals[128]);
int fauna_schedule(fauna_context *context);
int fauna_schedule_system(fauna_context *context,uint8_t previous_system);
/* 1 is the snapshot at original 20FB sprite call, 0 is no draw, <0 error. */
int fauna_step(fauna_context *context,unsigned slot,uint8_t draw[40]);
int fauna_step_with_ladders(fauna_context *context,unsigned slot,uint8_t draw[40],const uint8_t ladders[128]);
/* BANK4:8A06 reads current actor byte1 and current position/animation. */
int fauna_frozen_snapshot(const uint8_t actor[40],uint8_t draw[40]);
int fauna_sprite(const uint8_t actor[40],const uint8_t bank4[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
int fauna_rope_contact(uint8_t actors[320],uint8_t rope[32],uint8_t frame);
int fauna_bullet_contact(uint8_t actors[320],uint8_t bullet[24],uint8_t frame);
void fauna_player_contact(fauna_context *context,uint8_t player[128],uint8_t frame);
void fauna_protection_tick(uint8_t player[128],uint8_t frame);
#endif
