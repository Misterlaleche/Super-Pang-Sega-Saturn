#include "spang_life_sequence.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
int spang_life_flash_palette(const uint8_t fixed[32768],const uint8_t bank13[16384],uint8_t mode,uint8_t phase,uint8_t out[32])
{
 if(mode>2 || phase>4 || (phase&1))return -1;
 unsigned p=word(fixed+0x1f8e + mode*4+phase);
 if(p<0x8000 || p>0xbfe0)return -1;
 memcpy(out,bank13+(p-0x8000),32);return 0;
}
void spang_life_ready_begin(uint8_t p[32],uint8_t o[288])
{
 p[0]=1;p[2]=2;p[31]=1;p[19]=25;p[10]=208;p[11]=0;p[13]=112;
 p[6]=0x23;p[7]=0xa0;p[8]=2;
 /* A023 and A2EB: three FF filler objects are emitted by the ROM too. */
 for(unsigned i=0;i<9;++i){unsigned x=208+16*i;
  o[32*i]=(uint8_t)(i<6?10+i:255);o[32*i+1]=(uint8_t)(0xe0|((x>>8)<<4));
  o[32*i+2]=112;o[32*i+3]=(uint8_t)x;
 }
}
int spang_life_ready_tick(uint8_t p[32],uint8_t o[288])
{
 if(!p[0])return 0;
 if(p[2]==24){p[0]=p[2]=p[31]=0;return 1;}
 if(p[2]<2 || p[2]>22 || (p[2]&1))return -1;
 if(--p[19])return 0;
 p[19]=5;p[2]=(uint8_t)(p[2]+2);
 uint8_t y=p[31]?248:112;p[31]=(uint8_t)((p[31]+1)&1);
 for(unsigned i=0;i<6;++i)o[32*i+2]=y;
 return 0;
}
int spang_life_reserve_code(const uint8_t p[128],const uint8_t b[16384])
{
 if(p[3]!=10)return p[17]?0x724:0x6e4;
 if(p[17]>1 || p[48]>6 || (p[48]&1))return -1;
 unsigned table=word(b+0x2f20+2*p[17]);
 if(table<0x8000 || table>0xbff8)return -1;
 return (int)word(b+(table-0x8000+p[48]));
}
