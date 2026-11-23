#include "spang_round_speed.h"
#include <string.h>
int spang_round_speed_adjust(uint8_t g[128],uint8_t delta,const uint8_t fixed[32768])
{
 if(g[0x19]>=40 || g[0x52]>=32)return -1;
 int rank=g[0x52]+(delta<128?(int)delta:(int)delta-256);
 if(rank<0)rank=0;
 if(rank>31)rank=31;
 uint8_t floor=fixed[0x3232+g[0x19]];
 g[0x52]=(uint8_t)(rank<floor?floor:rank);
 return 0;
}
int spang_round_speed_apply(uint8_t g[128],uint8_t freeze,const uint8_t fixed[32768])
{
 if(g[0x52]>=32 || (!freeze && g[0x23]))return -1;
 uint8_t step=fixed[0x2f45+g[0x52]];
 g[0x28]=step;
 if(freeze)return 0;
 g[0x22]=step;
 memcpy(g+0x24,fixed+0x2f65+2*step,2);
 memcpy(g+0x30,fixed+0x2fb7+8*step,8);
 return 0;
}
