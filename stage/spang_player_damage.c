#include "spang_player_damage.h"
static uint16_t word(const uint8_t *p){return p[0]|((uint16_t)p[1]<<8);}
static void put(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void move(uint8_t *p,uint16_t velocity)
{
 uint32_t v=p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);
 v=(v+(velocity&0x8000?(0xffff0000u|velocity):velocity))&0xffffff;
 p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);
}
static void animation(uint8_t *p,const uint8_t *bank)
{
 unsigned table=0xae30+(uint8_t)((p[0x11]*2+p[0x21])*2);
 uint16_t a=word(bank+(table-0x8000));put(p+6,a);p[8]=bank[a-0x8000]&127;
}
int spang_player_contact(uint8_t p[128],const uint8_t balls[1024],
 uint8_t frame,uint8_t blocked,uint8_t *dead_flags)
{
 if(p[0]>=3 || blocked)return 0;
 if(p[0x54])return -1;
 static const uint8_t radius[5]={8,10,15,22,28};
 static const uint8_t offset[5]={12,14,18,26,30};
 static const uint8_t height[5]={28,32,40,56,64};
 for(unsigned n=0;n<16;++n){
  unsigned slot=(n%8)*4+(n/8)*2+(frame&1);const uint8_t *b=balls+slot*32;
  if(b[0]!=1)continue;
  unsigned selector=b[28];if(selector<1 || selector>5)return -1;
  unsigned r=radius[selector-1];uint16_t x=(uint16_t)(word(b+10)+r-word(p+10));
  if(x>=r*2+1)continue;
  if((uint8_t)(b[13]+offset[selector-1]-p[13])>=height[selector-1])continue;
  p[0x21]=x>=r;*dead_flags=(uint8_t)(*dead_flags+p[0x11]+1);
  p[0]=5;p[2]=4;p[3]=p[4]=p[5]=0;return 1;
 }
 return 0;
}
int spang_player_death(uint8_t p[128],const uint8_t bank[16384],
 uint8_t *sound,uint8_t *palette)
{
 *sound=*palette=255;
 if(p[0]!=5 || p[2]!=4 || p[0x11]>1 || p[0x21]>1)return -1;
 switch(p[3]){
 case 0:
  p[3]=2;p[0x2f]=2;p[0x13]=30;p[0x25]=7;p[0x22]=1;
  if((!p[0x21] && word(p+10)>=384) || (p[0x21] && word(p+10)<128))p[0x22]=2;
  break;
 case 2:
  if(p[0x25])--p[0x25];
  if(!--p[0x13]){
   *sound=0x1e;p[3]=4;
   unsigned table=0xaa05+p[0x21]*4;put(p+0x14,word(bank+(table-0x8000)));put(p+0x16,word(bank+(table+2-0x8000)));animation(p,bank);
  }break;
 case 4:
  move(p+9,word(p+0x14));move(p+12,word(p+0x16));
  if((!p[0x21] && word(p+10)>=432) || (p[0x21] && word(p+10)<80)){
   p[0x21]^=1;
   if(--p[0x22]){
    unsigned table=0xaa05+p[0x21]*4;put(p+0x14,word(bank+(table-0x8000)));put(p+0x16,word(bank+(table+2-0x8000)));
   }else{p[3]=6;put(p+0x14,word(bank+0x2a5b+p[0x21]*2));put(p+0x16,0xfc00);}
   animation(p,bank);
  }break;
 case 6:
  move(p+9,word(p+0x14));put(p+0x16,(uint16_t)(word(p+0x16)+0x40));move(p+12,word(p+0x16));
  if((uint16_t)(word(p+13)+0x180)>=0x290){p[3]=8;p[0x13]=10;*sound=0x1d;}break;
 case 8:
  if(!--p[0x13]){p[3]=10;p[0x30]=0;p[0x31]=8;}break;
 case 10:
  if(!--p[0x31]){
   p[0x31]=8;uint8_t phase=(uint8_t)(p[0x30]+2);
   if(phase==8){p[0x5f]=3;p[2]=6;}
   else{p[0x30]=phase;*palette=phase;}
  }break;
 default:return -1;
 }
 return 0;
}

static uint8_t death_y(uint16_t y){return y&0x100?0xf8:(uint8_t)(y-8);}
int spang_player_death_sprite(const uint8_t p[128],uint8_t code,uint8_t attr,
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining)
{
 if(*remaining<4)return 0;
 if((*cursor&31) || *cursor>0xf80)return -1;
 uint16_t y=(uint16_t)(word(p+13)-8),x=(uint16_t)(word(p+10)-16);
 uint8_t top=death_y(y),bottom=death_y((uint16_t)(y+16));
 uint8_t left=attr|((x>>4)&16),right=left|(((uint16_t)(x+16)>>4)&16);
 const uint8_t codes[4]={code,(uint8_t)(code+8),(uint8_t)(code+9),(uint8_t)(code+1)};
 for(unsigned i=0;i<4;++i){unsigned dest=*cursor+i*32;
  objects[dest]=codes[i];objects[dest+1]=i<2?left:right;
  objects[dest+2]=(i==0 || i==3)?top:bottom;objects[dest+3]=(uint8_t)(x+(i>=2?16:0));
 }
 *cursor+=128;*remaining-=4;return 4;
}
uint8_t spang_player_life_end(uint8_t p[128])
{
 p[0x2f]=4;p[0]=p[2]=p[3]=p[4]=p[5]=p[0x62]=0;p[0x5f]=2;
 uint8_t old=p[0x60],v=(uint8_t)(old-1),adjust=((old&15)==0 || (v&15)>9)?6:0;
 if(old==0 || v>0x99)adjust+=0x60;
 return p[0x60]=(uint8_t)(v-adjust);
}
