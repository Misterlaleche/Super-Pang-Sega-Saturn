#include "spang_tour_plane.h"

static uint16_t word(const uint8_t *p){return p[0]|((uint16_t)p[1]<<8);}
static void put(uint8_t *p,uint16_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}

static int sprite(const uint8_t a[40],uint8_t g[128],const uint8_t b[16384],
 spang_tour_write write,void *context)
{
 unsigned animation=word(a+6),cursor=word(g+0x10);
 if(animation<0x8000 || animation>0xbffb || word(b+(animation-0x8000+1))!=0x21ad)return -1;
 if(g[0x14]<4)return 0;
 if((cursor&31) || cursor<0xd000 || cursor>0xdf60 || !write)return -1;
 uint8_t code=b[animation-0x8000+3],attr=b[animation-0x8000+4];
 uint16_t x=(uint16_t)(word(a+10)-16);
 uint8_t left=(uint8_t)(attr|((x>>4)&16));
 uint8_t right=(uint8_t)(left|(((uint16_t)(x+16)>>4)&16));
 const uint8_t offsets[16]={0,32,64,96,1,2,3,35,34,33,65,66,67,99,98,97};
 const uint8_t values[16]={code,(uint8_t)(code+8),(uint8_t)(code+9),(uint8_t)(code+1),
  left,(uint8_t)(a[13]-16),(uint8_t)x,(uint8_t)x,a[13],left,
  right,a[13],(uint8_t)(x+16),(uint8_t)(x+16),(uint8_t)(a[13]-16),right};
 g[0x14]-=4;
 for(unsigned i=0;i<16;++i)write(context,(uint16_t)(cursor+offsets[i]),values[i]);
 put(g+0x10,(uint16_t)(cursor+128));
 return 0;
}

int spang_tour_plane_step(uint8_t a[40],uint8_t g[128],const uint8_t in[6],
 const uint8_t b[16384],spang_tour_write write,void *context,uint8_t events[2])
{
 if(!a||!g||!in||!b||!events)return -1;
 events[0]=events[1]=0;
 if(!a[0])return 0;
 /* BANK3:8B30 worlds0..3/5..8 use this flight; worlds4/9 use tally8BF0. */
 if(g[0x18]>13 || g[0x18]==4 || g[0x18]==9 || (a[2]!=0 && a[2]!=2 && a[2]!=4 && a[2]!=6))return -1;
 if(a[2]==0){
  unsigned route=0xcf8+b[0xce9+g[0x18]]; /* 8CF8 + route-table byte */
  if(route+7>=16384)return -1;
  unsigned animation_index=b[route+3];
  if(0xd58+animation_index+1>=16384)return -1;
  unsigned animation=word(b+0xd58+animation_index);
  if(animation<0x8000 || animation>=0xc000 || b[0xb30+g[0x18]]!=2)return -1;
  a[2]=b[0xb30+g[0x18]];a[10]=b[route];a[11]=b[route+1];a[13]=b[route+2];
  put(a+6,(uint16_t)animation);a[8]=b[animation-0x8000]&127;
  for(unsigned i=0;i<4;++i)a[0x14+i]=b[route+4+i];
  a[0x13]=20;events[0]=0x11;return 0;
 }
 if(a[2]==4){
  if((in[4] && (in[0]&(uint8_t)~in[1]&8)) ||
     (in[5] && (in[2]&(uint8_t)~in[3]&8)))goto complete;
  uint32_t x=(uint32_t)a[9]|((uint32_t)a[10]<<8)|((uint32_t)a[11]<<16);
  int32_t delta=word(a+0x14);if(delta>=32768)delta-=65536;
  x=(x+(uint32_t)delta)&0xffffff;
  a[9]=(uint8_t)x;a[10]=(uint8_t)(x>>8);a[11]=(uint8_t)(x>>16);
  put(a+12,(uint16_t)(word(a+12)+word(a+0x16)));
 }
 if(sprite(a,g,b,write,context))return -1;
 if(--a[0x13])return 0;
 if(a[2]==2){a[2]=4;a[0x13]=60;return 0;}
 if(a[2]==4){a[2]=6;a[0x13]=g[0x0c]==3?80:20;return 0;}
complete:
 a[0]=a[2]=0;g[0x0e]=0;events[1]=1;
 return 0;
}
