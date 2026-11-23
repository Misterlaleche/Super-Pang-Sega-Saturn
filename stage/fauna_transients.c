/* Native BANK3:9603..962B and BANK2:9D39..9D68. */
#include "fauna_transients.h"
#include "fauna_internal.h"

int fauna_transient_step(uint8_t a[16],const uint8_t b[16384],
 unsigned kind,int advance,int stopped,uint8_t draw[16])
{
 if(kind>1)return -750;
 if(!a[0])return 0;
 if(!advance){if(a[0]!=1)return 0;memcpy(draw,a,16);return 1;}
 unsigned init=kind?0x9d39:0x9603,active=kind?0x9d4d:0x9617;
 unsigned end=kind?0x9d64:0x9627,pc=fa_word(a+2);
 if(pc==init){
  a[0]=1;a[5]=kind?20:8;fa_animation(a,b,kind?0x9d69:0x962c);
  fa_put(a+2,active);return 0;
 }
 if(pc==end){a[0]=0;return 0;}
 if(pc!=active)return -751;
 if(kind && stopped){memcpy(draw,a,16);return 1;}
 if(!--a[8]){
  unsigned old=fa_word(a+6);if(old<0x8000 || old>0xbff9)return -752;
  unsigned next=old+5;if(b[old-0x8000]&128)next=fa_bw(b,next);
  if(next<0x8000 || next>0xbffb)return -752;
  fa_animation(a,b,next);
 }
 memcpy(draw,a,16);
 if(!--a[5])fa_put(a+2,end);
 return 1;
}

int fauna_transient_sprite(const uint8_t a[16],const uint8_t b[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining)
{
 unsigned ptr=fa_word(a+6);if(ptr<0x8000 || ptr>0xbffb)return -752;
 if(fa_bw(b,ptr+1)!=0x2112)return -753;
 if(!*remaining)return 0;
 if((*cursor&31) || *cursor+32>0xfe0)return -754;
 unsigned x=(fa_word(a+10)-8)&65535,at=*cursor;
 objects[at]=b[ptr+3-0x8000];
 objects[at+1]=(uint8_t)(b[ptr+4-0x8000]|((x>>4)&16));
 objects[at+2]=(uint8_t)(a[13]-8);objects[at+3]=(uint8_t)x;
 *cursor+=32;--*remaining;return 1;
}
