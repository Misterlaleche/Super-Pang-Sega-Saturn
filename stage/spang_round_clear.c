#include "spang_round_clear.h"
#include <string.h>
static uint16_t word(const uint8_t *p){return p[0]|((uint16_t)p[1]<<8);}
int spang_round_victory_sprite(const uint8_t p[128],uint8_t code,uint8_t attr,uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining)
{
 if(*remaining<6)return 0;
 if((*cursor&31) || *cursor+192>0xfe0)return -1;
 const uint8_t dx[6]={0,0,16,16,32,32},dy[6]={240,0,0,240,240,0},dc[6]={0,8,9,1,2,10};
 uint16_t x=(uint16_t)(word(p+10)-24);uint8_t carried=attr;
 for(unsigned i=0;i<6;++i){
  unsigned at=*cursor+i*32;
  carried|=((uint16_t)(x+dx[i])>>4)&16;
  objects[at]=(uint8_t)(code+dc[i]);objects[at+1]=carried;
  objects[at+2]=(uint8_t)(p[13]+dy[i]);objects[at+3]=(uint8_t)(x+dx[i]);
 }
 *cursor+=192;*remaining-=6;return 6;
}
int spang_round_clear_player(uint8_t p[128],const uint8_t bank[16384],uint16_t *palette)
{
 *palette=0xffff;
 /* FIXED38FF retains active2 on a ladder; BANK0:A8F9 then enters victory. */
 if((p[0]!=1 && p[0]!=2 && p[0]!=3 && p[0]!=4 && p[0]!=5) || p[0x11]>1)return -1;
 if(!p[4]){
  unsigned animation=word(bank+0x2e2c+2*p[0x11]);
  if(animation<0x8000 || animation>=0xc000)return -1;
  p[0]=p[1]=1;p[0x23]=0;p[4]=2;p[0x30]=0;p[0x31]=8;
  p[6]=(uint8_t)animation;p[7]=(uint8_t)(animation>>8);p[8]=bank[animation-0x8000]&127;
 }else if(p[4]==2){
  if(!--p[0x31]){
   unsigned table=word(bank+0x2f1c+2*p[0x11]);
   if(table<0x8000 || table+7>=0xc000)return -1;
   p[0x31]=8;p[0x30]=(p[0x30]+2)&6;
   *palette=word(bank+(table-0x8000+p[0x30]));
  }
 }else return -1;
 return 0;
}
int spang_round_clear_actor(uint8_t a[32],uint8_t g[128],uint8_t lives1,uint8_t lives2,const uint8_t bank[16384],uint8_t e[2],uint8_t draw_actor[32])
{
 e[0]=e[1]=0;memset(draw_actor,0,32);
 if(g[0x4e]>=2 && g[0x4e]!=2 && g[0x4e]!=4)return -1;
 switch(a[2]){
 case 0:
  e[0]=8;a[2]=2;a[10]=0x60;a[11]=0;a[13]=8;
  a[6]=0x31;a[7]=0x8a;a[8]=bank[0xa31]&127;
  break;
 case 2:
  memcpy(draw_actor,a,32);
  e[1]=1;a[13]=(uint8_t)(a[13]+16);
  if(a[13]>=0x48){a[13]=0x48;a[2]=4;a[0x13]=30;e[0]=0x1a;}
  break;
 case 4:
  memcpy(draw_actor,a,32);
  e[1]=1;
  if(!--a[0x13]){
   a[0]=a[2]=a[0x1e]=0;g[0x41]=(lives1?2:0)|(lives2?4:0);e[1]|=2;
   if(g[0x4e]>=2){
    a[10]=0x58;a[11]=0;a[13]=0x60;a[6]=0xcd;a[7]=0x8a;a[8]=bank[0xacd]&127;
    e[1]|=12; /* 84F5: owner score5C, then second draw at850F. */
   }
  }
  break;
 default:return -1;
 }
 return 0;
}
int spang_round_clear_sprite(const uint8_t a[32],const uint8_t bank[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining)
{
 unsigned animation=word(a+6);
 if(animation<0x8000 || animation+3>=0xc000)return -1;
 const uint8_t *frame=bank+(animation-0x8000);
 unsigned count=frame[1],deltas=word(frame+2);
 if(count>*remaining)return 0;
 if(!count || (*cursor&31) || *cursor+count*32>0xfe0 ||
    animation+4+count*2>0xc000 || deltas<0x8000 || deltas+count*2>0xc000)return -1;
 uint16_t x=word(a+10);uint8_t y=a[13];
 for(unsigned i=0;i<count;++i){
  uint8_t dx=bank[deltas-0x8000+2*i],dy=bank[deltas-0x8000+2*i+1];
  x=(uint16_t)(x+(dx<128?(int)dx:(int)dx-256));y=(uint8_t)(y+dy);
  uint8_t *out=objects+*cursor+i*32;
  out[0]=frame[4+i*2];out[3]=(uint8_t)x;
  out[1]=frame[5+i*2]|((x>>4)&16);out[2]=y;
 }
 *cursor=(uint16_t)(*cursor+count*32);*remaining=(uint8_t)(*remaining-count);
 return (int)count;
}
