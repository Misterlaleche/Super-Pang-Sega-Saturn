#ifndef R03_CROCODILE_H
#define R03_CROCODILE_H
#include <stdint.h>
#include "spang_round_frame.h"
#include "spang_weapons.h"
/* Native translations for Java1. Original byte layouts: E1A0 scheduler,
 * FA00 eight40-byte animals. No instruction interpreter is linked. */
typedef void (*crocodile_sound_fn)(void *context,unsigned command);
void crocodile_init(uint8_t scheduler[32],uint8_t actors[320],uint8_t globals[128]);
int crocodile_schedule(uint8_t scheduler[32],uint8_t actors[320],uint8_t globals[128],const uint8_t bank4[16384]);
/* Returns1 when draw holds the exact actor at its original20FB call;0 when
 * no sprite call occurred; negative for unsupported/unreachable data. */
int crocodile_step(uint8_t actor[40],uint8_t scheduler[32],uint8_t globals[128],
 const uint8_t p1[128],const uint8_t p2[128],uint8_t balls[1024],const uint8_t grid[2048],
 spang_round_frame *round,const uint8_t bank4[16384],uint8_t draw[40],
 crocodile_sound_fn sound,void *context);
/* R07 extends the same controller with the E800 ladder pool. The wrapper
 * above preserves the earlier no-ladder fixture interface. */
int crocodile_step_with_ladders(uint8_t actor[40],uint8_t scheduler[32],uint8_t globals[128],
 const uint8_t p1[128],const uint8_t p2[128],uint8_t balls[1024],const uint8_t grid[2048],
 spang_round_frame *round,const uint8_t bank4[16384],uint8_t draw[40],
 crocodile_sound_fn sound,void *context,const uint8_t ladders[128]);
int crocodile_sprite(const uint8_t actor[40],const uint8_t bank4[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining);
int crocodile_rope_contact(uint8_t actors[320],uint8_t rope[32],uint8_t frame);
int crocodile_bullet_contact(uint8_t actors[320],uint8_t bullet[24],uint8_t frame);
void crocodile_player_contact(uint8_t actors[320],const uint8_t player[128],uint8_t frame);
#endif
