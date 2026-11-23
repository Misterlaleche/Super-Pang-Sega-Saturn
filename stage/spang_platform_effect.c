#include "spang_platform_effect.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
int spang_platform_effect_spawn(uint8_t pool[128],const uint8_t h[32],const uint8_t bank[16384])
{
 unsigned cell=h[31],count=cell&7,address=word(h+29);
 if(!count || count>6 || address<0xf000 || address>=0xf800)return -1;
 for(unsigned i=0;i<4;++i){uint8_t *a=pool+i*32;if(a[0])continue;
  memset(a,0,32);a[0]=2;a[1]=h[17];put(a+2,0x9253);
  put(a+10,(address&63)*8);a[13]=(uint8_t)((address>>6)*8);
  a[14]=(uint8_t)cell;a[15]=bank[0x29a+((cell&64)?7:0)+count];return 1;
 }return 0;
}
int spang_platform_effect_step(uint8_t pool[128],const uint8_t bank[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance,uint8_t drops[16])
{
 memset(drops,0,16);
 for(unsigned i=0;i<4;++i){uint8_t *a=pool+i*32;unsigned state=word(a+2),v=a[15];
  if(!a[0])continue;
  if(v>20 || (v&1))return -1;
  if(state==0x9253){if(!advance)continue;
   a[0]=1;a[5]=16;put(a+2,0x9303);
   unsigned p=word(bank+0x13b5+v);if(p<0x8000 || p>=0xc000)return -1;
   put(a+6,p);a[8]=bank[p-0x8000];
   int dx=0,dy=0;
   if(v==0){dx=-4;dy=-4;}
   else if(v<12){if(v==2 || v==4 || v==8)dx=-8;dy=-4;}
   else {dx=-4;if(v!=12)dy=-8;}
   put(a+10,word(a+10)+dx);a[13]=(uint8_t)(a[13]+dy);continue;
  }
  if(state==0x9375){if(advance){a[0]=a[11]=0;put(a+2,0x9253);}continue;}
  if(state!=0x9303)return -1;
  if(advance && !--a[5]){
   a[0]=2;put(a+2,0x9375);unsigned item=(a[14]&0x38)>>2;
   if(item){unsigned x=word(a+10)+bank[0x1382+v];
    drops[4*i]=(uint8_t)(item==2?2:1);drops[4*i+1]=(uint8_t)(item==2?2:(item-4)/2);
    drops[4*i+2]=(uint8_t)x;drops[4*i+3]=(uint8_t)(a[13]+bank[0x1383+v]);
   }continue;
  }
  unsigned p=word(a+6);
  if(advance && !--a[8]){if(p<0x8000 || p+1>=0xc000)return -1;p+=bank[p-0x8000+1]+3;put(a+6,p);if(p>=0xc000)return -1;a[8]=bank[p-0x8000];}
  if(p<0x8000 || p+2>=0xc000)return -1;
  unsigned count=bank[p-0x8000+1],attr=bank[p-0x8000+2],x=word(a+10),y=a[13];
  if(p+2+count>=0xc000)return -1;
  if(*remaining<count)continue;
  if((*cursor&31) || *cursor+count*32>0xfe0)return -1;
  *remaining-=(uint8_t)count;
  for(unsigned j=0;j<count;++j){uint8_t *o=objects+*cursor;
   o[0]=bank[p-0x8000+3+j];o[1]=(uint8_t)(attr|((x>>4)&16));o[2]=(uint8_t)y;o[3]=(uint8_t)x;
   *cursor+=32;if(v<12)x+=16;else y=(y-16)&255;
  }
 }return 0;
}
