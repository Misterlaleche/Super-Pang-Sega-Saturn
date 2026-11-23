#include "spang_round_clock.h"
static unsigned unbcd(uint16_t b)
{return (b&15)+10*((b>>4)&15)+100*((b>>8)&15)+1000*((b>>12)&15);}
static uint32_t packed(unsigned n)
{uint32_t result=0;for(unsigned shift=0;n && shift<32;shift+=4,n/=10)result|=(n%10)<<shift;return result;}
int spang_round_clock_init(uint8_t t[32],uint8_t stage,uint8_t difficulty,const uint8_t tables[113])
{
 if(stage>=40 || difficulty>=4)return -1;
 unsigned a=2*stage,b=80+2*difficulty;
 uint16_t value=(uint16_t)packed(unbcd(tables[a]|((uint16_t)tables[a+1]<<8))+unbcd(tables[b]|((uint16_t)tables[b+1]<<8)));
 t[0]=60;t[1]=(uint8_t)value;t[2]=(uint8_t)(value>>8);
 t[27]=8;t[28]=t[29]=0;t[30]=value>=0x50?0x50:0x20;t[31]=0;
 return 0;
}
void spang_round_clock_step(uint8_t t[32],const uint8_t g[128],const uint8_t tables[113],uint8_t video_busy,uint8_t e[4])
{
 e[0]=e[1]=e[2]=e[3]=0;
 /* The warning palette service precedes all countdown early returns. */
 if(t[29] && !--t[27]){
  t[27]=8;t[28]=(uint8_t)(t[28]+2);if(t[28]>=19)t[28]=0;
  if(!video_busy)e[2]=(uint8_t)(t[28]+1);
 }
 if(g[0x4e] || g[0x2b] || !g[0x20])return;
 uint16_t value=t[1]|((uint16_t)t[2]<<8),mark=t[30]|((uint16_t)t[31]<<8);
 if(mark && value==mark){
  t[30]=t[31]=0;
  unsigned index=g[0x17];
  /* Normal Tour difficulty selector has three values, 0..2. */
  if(index<3)e[0]=tables[107+(mark==0x20?3:0)+index];
  if(mark!=0x20)t[30]=0x20;
  else{t[29]=2;e[1]=0x31;}
 }
 if(--t[0])return;
 t[0]=40;
 if(t[1]){t[1]=(uint8_t)(t[1]-((t[1]&15)?1:7));return;}
 t[1]=0x99;
 if(t[2]){--t[2];return;}
 t[1]=0;e[3]=1;
}
uint32_t spang_round_time_bonus(uint16_t time,uint8_t retries,uint8_t bonus_selector,const uint8_t tables[113])
{
 if(bonus_selector>=15)return 0;
 unsigned r=retries<3?retries:3;
 /* 87DC adds the original multiplicand B times to itself: factor B+1. */
 unsigned first=(tables[88+r]+1)*unbcd(time);
 /* 87C3 stores HL only; the decimal carry in A is discarded. */
 unsigned second=(tables[92+bonus_selector]+1)*(first%10000);
 return packed(second);
}
void spang_round_clock_draw(const uint8_t t[32],const uint8_t table[20],spang_video_write write,void *context)
{
 unsigned digits[2]={t[1]>>4,t[1]&15};
 if(digits[0]>9 || digits[1]>9)return;
 for(unsigned digit=0;digit<3;++digit){
  uint16_t code=digit?(uint16_t)(table[2*digits[digit-1]]|((uint16_t)table[2*digits[digit-1]+1]<<8)):(t[2]?0x06c2:0x0122);
  for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){
   uint16_t address=(uint16_t)(0xddc2+digit*4+y*128+x*2);
   /* ROM adds in A, retaining the high tile byte across low-byte wrap. */
   write(context,address,(uint8_t)(code+y*16+x));
   write(context,(uint16_t)(address+1),(uint8_t)(code>>8));
  }
 }
}
