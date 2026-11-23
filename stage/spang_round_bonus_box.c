#include "spang_round_bonus_box.h"
#include "spang_round_clock.h"
static void tile(spang_video_write write,void *context,uint16_t address,uint16_t code)
{write(context,address,(uint8_t)code);write(context,(uint16_t)(address+1),(uint8_t)(code>>8));}
static void glyph(spang_video_write write,void *context,uint16_t address,uint16_t code)
{
 for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
  tile(write,context,(uint16_t)(address+y*128+x*2),(uint16_t)((code&0xff00)|((code+y*16+x)&255)));
 uint16_t attr=(uint16_t)(0xc800+(address-0xd000)/2);
 write(context,attr,0x10);write(context,(uint16_t)(attr+1),0x10);
 write(context,(uint16_t)(attr+64),0x10);write(context,(uint16_t)(attr+65),0x10);
}
int spang_round_bonus_box_player(unsigned player,uint8_t counter,const uint8_t table[20],spang_video_write write,void *context,uint8_t queue[2])
{
 if(player>1 || (counter>>4)>9 || (counter&15)>9)return -1;
 unsigned shift=player*0x2e;
 queue[0]=8;queue[1]=(uint8_t)(0x10+player*2);
 for(unsigned y=0;y<6;++y){
  uint16_t edge=y==0?0x0a8e:y==5?0x0a9e:0x0b76;
  uint16_t middle=y==0?0x0a8f:y==5?0x0a9f:0x0020;
  for(unsigned x=0;x<21;++x)tile(write,context,(uint16_t)(0xd794+shift+y*128+x*2),(x==0 || x==20)?edge:middle);
 }
 for(unsigned y=0;y<6;++y)for(unsigned x=0;x<21;++x)
  write(context,(uint16_t)(0xcbca+shift/2+y*64+x),x==20?0xe5:0x65);
 for(unsigned i=0;i<2;++i){
  unsigned d=i?(counter&15):(counter>>4);
  uint16_t code=table[2*d]|((uint16_t)table[2*d+1]<<8);
  glyph(write,context,(uint16_t)(0xd81e + shift+i*4),code);
 }
 return 0;
}
int spang_round_bonus_box(uint8_t counter,const uint8_t table[20],spang_video_write write,void *context,uint8_t queue[2])
{return spang_round_bonus_box_player(0,counter,table,write,context,queue);}
int spang_round_bonus_p1(uint8_t a[32],uint8_t t[32],uint8_t retries,uint8_t group,const uint8_t bank[16384],const uint8_t tables[113],const uint8_t digits[20],spang_video_write write,void *context,uint8_t award[3])
{
 award[0]=award[1]=award[2]=0;
 if(a[0x1e])return 0;
 if(group>=15 || (t[1]&15)>9 || (t[1]>>4)>9 || t[2]>1)return -1;
 const uint8_t *label=bank+0xa03;unsigned count=label[0];
 if(!count || count>64)return -1;
 a[0x1e]=2;a[0x12]=0;a[0x1d]=0;
 a[1]=bank[0x54e];a[0x13]=bank[0x54f];a[0x12]=2;
 for(unsigned y=0;y<2;++y)for(unsigned x=0;x<count;++x){
  uint16_t dest=(uint16_t)(0xd916+y*128+x*2);
  tile(write,context,dest,(uint16_t)(label[3+y*count+x]|((uint16_t)label[2]<<8)));
  write(context,(uint16_t)(0xcc8b+y*64+x),label[1]);
 }
 uint32_t value=spang_round_time_bonus((uint16_t)(t[1]|((uint16_t)t[2]<<8)),retries,group,tables);
 value&=0xfffff0u; /* 8624: discard units before drawing and scoring. */
 t[0]=(uint8_t)value&0xf0;t[1]=(uint8_t)(value>>8);t[2]=(uint8_t)(value>>16);
 a[0x1f]=0;
 for(unsigned i=0;i<6;++i){
  unsigned d=(value>>(20-4*i))&15;
  if(!d && !a[0x1f] && i!=5)continue;
  ++a[0x1f];
  glyph(write,context,(uint16_t)(0xd924+i*4),(uint16_t)(digits[d*2]|((uint16_t)digits[d*2+1]<<8)));
 }
 award[0]=t[0];award[1]=t[1];award[2]=t[2];
 return 1;
}

static int label(const uint8_t *bank,unsigned address,unsigned dest,spang_video_write write,void *context)
{
 if(address<0x8000 || address>0xbffb)return -1;
 const uint8_t *s=bank+(address-0x8000);unsigned count=s[0];
 if(!count || count>64 || address+3+count*2>0xc000)return -1;
 for(unsigned y=0;y<2;++y)for(unsigned x=0;x<count;++x){
  tile(write,context,(uint16_t)(dest+y*128+x*2),(uint16_t)(s[3+y*count+x]|((uint16_t)s[2]<<8)));
  write(context,(uint16_t)(0xc800+(dest-0xd000)/2+y*64+x),s[1]);
 }
 return 0;
}
int spang_round_bonus_players(uint8_t a[32],uint8_t t[32],uint8_t retries,uint8_t group,uint8_t eligible,uint8_t pops1,uint8_t pops2,const uint8_t bank[16384],const uint8_t tables[113],const uint8_t digits[20],spang_video_write write,void *context,uint8_t awards[6],uint8_t *wins)
{
 for(unsigned i=0;i<6;++i)awards[i]=0;
 *wins=0;
 if(a[0x1e]){
  /* 8564: only WIN blinks; ties use the RET dispatcher at8598. */
  if((a[0x1d]==2 || a[0x1d]==4) && !--a[0x13]){
   unsigned at=0x54e + a[0x12];a[1]=bank[at];a[0x13]=bank[at+1];a[0x12]=(uint8_t)((a[0x12]+2)%10);
   unsigned dest=a[0x1d]==2?0xcc17:0xcc2e;
   for(unsigned y=0;y<2;++y)for(unsigned x=0;x<7;++x)write(context,(uint16_t)(dest+y*64+x),a[1]);
  }
  return 0;
 }
 if((eligible&~6u) || group>=15 || (t[1]&15)>9 || (t[1]>>4)>9 || t[2]>1)return -1;
 unsigned award=eligible==4?2:1;
 if(eligible==6)award=pops1==pops2?3:pops1>pops2?1:2;
 a[0x1e]=2;a[0x12]=2;a[0x1d]=(uint8_t)(eligible==6?award*2:0);
 a[1]=bank[0x54e];a[0x13]=bank[0x54f];
 if(eligible==6)for(unsigned n=0;n<2;++n){
  unsigned pointer=award==3?0x89f2:award==(1u<<n)?0x89d0:0x89e1;
  if(label(bank,pointer,0xd82e + n*0x2e,write,context))return -1;
  if(!(award&(1u<<n)) && label(bank,0x8a14,0xd91c+n*0x2e,write,context))return -1;
 }
 uint32_t value=spang_round_time_bonus((uint16_t)(t[1]|((uint16_t)t[2]<<8)),retries,group,tables);
 if(award==3){
  /* 86DD..86F7 shifts one BCD digit, then applies the original *5. */
  unsigned decimal=0,multiple=1;for(unsigned v=value>>4;v;v>>=4){decimal+=(v&15)*multiple;multiple*=10;}
  decimal*=5;value=0;for(unsigned shift=0;shift<24;shift+=4){value|=(decimal%10)<<shift;decimal/=10;}
 }else *wins=(uint8_t)award;
 value&=0xfffff0u;t[0]=(uint8_t)value;t[1]=(uint8_t)(value>>8);t[2]=(uint8_t)(value>>16);
 for(unsigned n=0;n<2;++n)if(award&(1u<<n)){
  if(label(bank,0x8a03,0xd916+n*0x2e,write,context))return -1;
  a[0x1f]=0;
  for(unsigned i=0;i<6;++i){unsigned d=(value>>(20-4*i))&15;
   if(!d && !a[0x1f] && i!=5)continue;
   ++a[0x1f];glyph(write,context,(uint16_t)(0xd924+n*0x2e + i*4),(uint16_t)(digits[d*2]|((uint16_t)digits[d*2+1]<<8)));
  }
  awards[n*3]=t[0];awards[n*3+1]=t[1];awards[n*3+2]=t[2];
 }
 return (int)award;
}
