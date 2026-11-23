#include "spang_tour_region.h"
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void queued(spang_tour_queue_fn fn,void *ctx,unsigned value)
{fn(ctx,value>>8,value&255);}
int spang_tour_region_step(uint8_t actors[320],uint8_t g[128],
 const uint8_t in[6],const uint8_t b[16384],
 spang_tour_queue_fn queue,void *context,uint8_t events[2])
{
 if(!actors || !g || !in || !b || !queue || !events ||
    g[0x17]>1 || g[0x18]!=4+g[0x17]*5)return -1;
 events[0]=events[1]=0;uint8_t *a=actors;
 if(!a[0])return 0;
 if(!a[2]){a[2]=b[0xb30+g[0x18]];return a[2]==8?0:-1;}
 if(a[2]!=8 || a[3]>28 || (a[3]&1))return -1;
 if(!a[3]){
  a[3]=2;a[19]=120;
  if(in[4])queued(queue,context,word(b+0xc7e + g[0x17]*2));
  if(in[5])queued(queue,context,word(b+0xc82+g[0x17]*2));
  events[0]=0x3b;return 0;
 }
 if(a[3]<=10){
  if((in[4] && (in[0]&(uint8_t)~in[1]&8)) ||
     (in[5] && (in[2]&(uint8_t)~in[3]&8))){
   a[3]=26;for(unsigned i=2;i<7;++i)actors[i*40]=0;return 0;
  }
  if(--a[19])return 0;
  a[19]=60;queued(queue,context,0x800+b[0xc74+g[0x17]]+a[3]);
  unsigned target=word(b+0xc84+a[3]);
  if(target<0xfa00 || target>=0xfb40 || (target-0xfa00)%40)return -1;
  actors[target-0xfa00]=0;a[3]+=2;events[0]=0x11;return 0;
 }
 if(a[3]==12){a[19]=20;a[3]=14;}
 if(a[3]<=24){
  if(--a[19])return 0;
  a[19]=20;a[3]+=2;
  queued(queue,context,a[3]&2?0x840:word(b+0xcba+g[0x17]*2));return 0;
 }
 if(a[3]==26){a[3]=28;a[19]=120;queued(queue,context,0x83e);return 0;}
 if(--a[19])return 0;
 a[0]=a[2]=g[0x0e]=0;events[1]=1;return 0;
}
