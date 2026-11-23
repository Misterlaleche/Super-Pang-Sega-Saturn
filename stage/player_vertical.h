#ifndef R04_PLAYER_VERTICAL_H
#define R04_PLAYER_VERTICAL_H
#include <stdint.h>
#include "spang_weapons.h"
/* BANK0 A5CB..A8F0, ABDE/ACBB, B017..B35B on ordinary Tour surfaces.
 * Raw input is FCCD format, fire_edge includes any presentation-latched edge.
 * Actor bytes, 8.8 Y, 16.8 X and ladder E800 descriptor offsets stay original.
 * One original direct FM request is returned in sound, or255 for none.
 * Protection, ball/animal contact and final sprite drawing retain their own
 * stage owners. No CPU interpreter or copied arcade frame is used. */
int player_vertical_step(uint8_t player[128],uint8_t input,uint8_t previous,
                        int fire_edge,const uint8_t grid[2048],
                        const uint8_t ladders[128],const uint8_t bank0[16384],
                        const uint8_t bank2[16384],spang_weapons *weapons,
                        uint8_t *sound);
#endif
