#ifndef SPANG_LIFE_SEQUENCE_H
#define SPANG_LIFE_SEQUENCE_H
#include <stdint.h>
/* Fixed1E40: phase E188 is0,2,4; mode isE09A. Copy the32 logical
 * palette bytes destined for C3E0 (palette31), not Saturn RGB words. */
int spang_life_flash_palette(const uint8_t fixed[32768],const uint8_t bank13[16384],uint8_t mode,uint8_t phase,uint8_t out[32]);
/* Bank2 9EBF/9EE0/9F12, standard READY bootstrap. Owns first9 objects.
 * Actor is E1E0..E1FF. Tick returns1 on completion: caller dispatches
 * palette service bank12, command16 hex. It does not unlock player itself. */
void spang_life_ready_begin(uint8_t actor[32],uint8_t objects[288]);
int spang_life_ready_tick(uint8_t actor[32],uint8_t objects[288]);
/* AF20 death reserve animation: call after player_death; feed returned
 * low/high tile to the166E reserve-life draw. -1 is invalid ROM/state.
 * Outside death state0A returns the ordinary166B tile. */
int spang_life_reserve_code(const uint8_t player[128],const uint8_t bank0[16384]);
#endif
