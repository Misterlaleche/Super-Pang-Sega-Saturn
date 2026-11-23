#include "spang_round_bonus_mask.h"
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
int spang_round_bonus_mask(uint8_t g[128],const uint8_t objects[4096],
 const uint8_t b[16384],spang_video_write write,void *context)
{
 if(!g || !objects || !b || !write)return -1;
 unsigned selector=g[0x41],count,tail,cursor,appended;
 if(selector>6 || (selector&1) || g[0x16]>127 || g[0x14]>127)return -1;
 count=127-g[0x16];tail=word(g+0x12);cursor=word(g+0x10);
 appended=selector==6?2:1;if(appended>g[0x14])appended=g[0x14];
 if((tail&31) || tail<0xd000 || tail+count*32>0xdfe0 ||
    (appended && ((cursor&31) || cursor<0xd000 || cursor+appended*32>0xdfe0)))return -1;
 for(unsigned pass=0;pass<(selector==6?2u:1u);++pass){
  unsigned p2=selector==4 || (selector==6 && pass==1);
  for(unsigned i=0;i<count;++i){
   unsigned address=tail+i*32;const uint8_t *cell=objects+(address-0xd000);
   unsigned x=cell[3]+((cell[1]&16)<<4),threshold=(cell[4]?256:240)+p2;
   if((p2?x>=threshold:x<threshold) && cell[2]>=105 && cell[2]<168)
    write(context,(uint16_t)(address+2),248);
  }
  if(g[0x14]){
   --g[0x14];cursor=word(g+0x10);
   for(unsigned i=0;i<4;++i)write(context,(uint16_t)(cursor+i),b[(p2?0x9cc:0x9c8)+i]);
   put(g+0x10,cursor+32);
  }
 }
 return 0;
}
