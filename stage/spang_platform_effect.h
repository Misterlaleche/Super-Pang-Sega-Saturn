#ifndef SPANG_PLATFORM_EFFECT_H
#define SPANG_PLATFORM_EFFECT_H
#include <stdint.h>
/* Four original E8C0 actors. Drops: four {kind,index,xlo,y} records;
 * kind 0 none, 1 normal item, 2 special actor; X high in actor[11]. */
int spang_platform_effect_spawn(uint8_t pool[128],const uint8_t harpoon[32],const uint8_t bank2[16384]);
int spang_platform_effect_step(uint8_t pool[128],const uint8_t bank2[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance,uint8_t drops[16]);
#endif
