#include "spang_motion_bonus.h"
#include "spang_bonus_life.h"
int spang_motion_bonus(uint8_t player[128],const uint8_t table[8],uint8_t effects[2])
{
 uint8_t state[4]={player[0x4a],player[0x4b],player[0x4c],player[0x60]};
 int count=spang_bonus_life(state,(uint16_t)(((uint16_t)player[0x40]<<8)|player[0x41]),table,1,1,0,effects);
 if(count<0)return count;
 player[0x4a]=state[0];player[0x4b]=state[1];player[0x4c]=state[2];player[0x60]=state[3];
 return count;
}
