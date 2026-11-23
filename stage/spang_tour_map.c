#include "spang_tour_map.h"
#include <string.h>
static unsigned word(const uint8_t *b){return b[0]|((unsigned)b[1]<<8);}
static void put(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
int spang_tour_spawn(uint8_t a[320],const uint8_t b[16384])
{
 unsigned ptr=word(b+0x1e02);
 if(ptr<0x8000 || ptr>=0xc000)return -1;
 unsigned p=ptr-0x8000,n=b[p++];
 if(n!=8 || p+2*n>16384)return -1;
 for(unsigned i=0;i<n;++i){unsigned kind=b[p+2*i],id=b[p+2*i+1];
  if((i==0 && kind!=10) || (i==7 && kind!=8) || (i>0 && i<7 && kind!=0) || id>10 || (id&1))return -1;
 }
 memset(a,0,320);for(unsigned i=0;i<8;++i)a[i*40+16]=4;
 for(unsigned i=0;i<n;++i){a[i*40]=1;a[i*40+17]=b[p++];a[i*40+15]=b[p++];}
 return 0;
}
/* 21AD, used for the selected-city marker. Ordered writes match ROM. */
static int selected(const uint8_t *a,uint8_t *g,const uint8_t *b,spang_tour_write w,void *ctx)
{
 if(g[20]<4)return 0;
 unsigned pos=word(g+16),x=(word(a+10)-16)&65535;
 if(pos<0xd000 || pos>0xdf60)return -1;
 unsigned descriptor=a[2]==6?0x192:0x194;
 uint8_t c=b[descriptor],at=b[descriptor+1]|((x>>4)&16),ar=at|(((x+16)>>4)&16);
 uint8_t o[16]={0,32,64,96,1,2,3,35,34,33,65,66,67,99,98,97};
 uint8_t v[16]={c,(uint8_t)(c+8),(uint8_t)(c+9),(uint8_t)(c+1),at,(uint8_t)(a[13]-16),(uint8_t)x,(uint8_t)x,a[13],at,ar,a[13],(uint8_t)(x+16),(uint8_t)(x+16),(uint8_t)(a[13]-16),ar};
 for(unsigned i=0;i<16;++i)w(ctx,(uint16_t)(pos+o[i]),v[i]);
 g[20]-=4;put(g+16,pos+128);return 0;
}
int spang_tour_marker_step(uint8_t a[40],uint8_t g[128],const uint8_t b[16384],spang_tour_write w,void *ctx)
{
 if(g[23]>2 || a[15]>10 || (a[15]&1))return -1;
 if(!a[0])return 0;
 if(!a[2]){
  if(a[15]==10){
   a[0]=0;if(g[23]!=2)return 0;
   a[0]=1;a[2]=6;put(a+10,0x122);a[13]=0x77;put(a+6,0x818f);a[8]=b[0x18f]&127;return 0;
  }
  a[2]=4;
  unsigned p=word(b+0xa1+g[23]*2)-0x8000+a[15]*2;
  put(a+10,word(b+p));a[13]=b[p+2];
  p=word(b+0xe3+g[23]*2)-0x8000+a[15];unsigned animation=word(b+p);
  put(a+6,animation);a[8]=b[animation-0x8000]&127;
  p=word(b+0x8c+g[23]*2)-0x8000+(a[15]>>1);
  if(b[p]>g[24])a[2]=2;
  return 0;
 }
 if(a[2]==6)return selected(a,g,b,w,ctx);
 if(a[2]!=2 && a[2]!=4)return -1;
 if(a[2]==4 && selected(a,g,b,w,ctx))return -1;
 unsigned p=word(a+6)-0x8000,n=b[p+1],delta=word(b+p+2)-0x8000,attr=b[p+4],dest=word(g+16);
 if(n>g[20])return 0;
 if(p+5+n>16384 || delta+2*n>16384 || dest<0xd000 || dest+n*32>0xdfe0)return -1;
 unsigned x=word(a+10);uint8_t y=a[13];g[20]-=(uint8_t)n;
 for(unsigned i=0;i<n;++i){
  x=(x+(int8_t)b[delta+2*i])&65535;y=(uint8_t)(y+b[delta+2*i+1]);
  w(ctx,(uint16_t)dest,b[p+5+i]);w(ctx,(uint16_t)(dest+3),(uint8_t)x);
  w(ctx,(uint16_t)(dest+1),(uint8_t)(attr|((x>>4)&16)));w(ctx,(uint16_t)(dest+2),y);dest+=32;
 }
 put(g+16,dest);return 0;
}
static void attributes(uint8_t *attrs,unsigned address,unsigned count,uint8_t value,spang_tour_write w,void *ctx)
{for(unsigned i=0;i<count;++i){attrs[address-0xc800+i]=value;w(ctx,(uint16_t)(address+i),value);}}
void spang_tour_timer_step(uint8_t a[40],uint8_t attrs[2048],const uint8_t b[16384],spang_tour_write w,void *ctx)
{
 /* 823C/829A/82AB, original one-player world-1 label palette sweep.
  * Initial queued labels are already expanded by the asset builder. */
 if(!a[0])return;
 if(!a[2]){a[2]=2;a[19]=10;return;}
 if(a[2]==2){if(!--a[19]){a[2]=4;a[19]=a[30]=7;}return;}
 if(a[2]!=4)return;
 if(!--a[30]){
  a[30]=7;
  if(attrs[0x61b]!=0x76){
   uint8_t value=(uint8_t)(attrs[0x61b]+1);
   for(unsigned i=0;i<17;++i)attributes(attrs,0xce1b-i,1,value,w,ctx);
   value=(uint8_t)(attrs[0x624]-1);attributes(attrs,0xce24,17,value,w,ctx);
  }
 }
 if(!--a[19]){
  a[19]=7;++a[31];uint8_t value=b[0x2f1+a[31]];
  attributes(attrs,0xcddd,6,value,w,ctx);attributes(attrs,0xce1d,6,value,w,ctx);attributes(attrs,0xce5d,6,value,w,ctx);
  if(a[31]==7)a[0]=a[2]=0;
 }
}
