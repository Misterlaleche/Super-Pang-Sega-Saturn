#include "crocodile.h"
#include "spang_actor_motion.h"
#include "spang_items.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static unsigned bw(const uint8_t *b,unsigned a){return word(b+(a-0x8000));}
static void move(uint8_t *p,int delta)
{
 uint32_t v=p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);
 v=(v+(uint32_t)delta)&0xffffff;p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);
}
static void animation(uint8_t *a,const uint8_t *b,unsigned address)
{put(a+6,address);a[8]=b[address-0x8000]&127;}
static void pose(uint8_t *a,const uint8_t *b,unsigned table)
{animation(a,b,bw(b,table+a[33]));}
static int animate(uint8_t *a,const uint8_t *b)
{
 if(--a[8])return 0;
 unsigned old=word(a+6);if(old<0xa8b5 || old>0xa974)return -601;
 unsigned next=old+5;if(b[old-0x8000]&128)next=bw(b,next);
 if(next<0xa8b5 || next>0xa974)return -601;
 animation(a,b,next);return 0;
}
void crocodile_init(uint8_t s[32],uint8_t a[320],uint8_t g[128])
{
 memset(s,0,32);put(s+2,0x8008);memset(a,0,320);
 for(unsigned i=0;i<8;++i)a[i*40+16]=4;
 g[0x1e]=8;
}
int crocodile_schedule(uint8_t s[32],uint8_t actors[320],uint8_t g[128],const uint8_t b[16384])
{
 unsigned pc=word(s+2),stage=g[0x19],rank=g[0x52];
 if(stage>2 || rank>=32)return -602;
 if(pc==0x8008){
  put(s+8,bw(b,0x826a+b[0x242+stage]));put(s+18,bw(b,0x88d6+rank*2));
  put(s+30,0x978d+stage*16);s[29]=0;g[0x5e]=b[0x936+stage];
  if(g[0x5e])return -603;
  put(s+2,0x805c);return 0;
 }
 if(pc==0x80f5)return 0;
 if(pc==0x805c){
  if(g[0x2b])return 0;
  unsigned old=word(s+18);put(s+18,old-1);if(old)return 0;
  unsigned p=word(s+8),delay=bw(b,p);put(s+18,delay);s[7]=2;
  /*805C overwrites80C1's end marker with808E even for a zero first delay.*/
  put(s+2,0x808e);return 0;
 }
 if(pc!=0x808e)return -604;
 unsigned delay=(word(s+18)-1)&65535;put(s+18,delay);if(delay)return 0;
 unsigned p=word(s+8);put(s+8,p+6);unsigned next=bw(b,p+6);put(s+18,next);
 if(!next)put(s+2,0x80f5);
 if(s[28]>=b[0x916+rank])return 0;
 if(b[p+2-0x8000]!=2)return -605;
 if(g[0x21]<5 || !g[0x1e])return 0;
 --g[0x1e];unsigned slot=0;while(slot<8 && actors[slot*40])++slot;
 if(slot==8)return -606;
 uint8_t *a=actors+slot*40;++s[28];a[0]=2;a[17]=3;a[15]=b[p+3-0x8000];
 if(!a[15]){put(a+10,bw(b,p+4));put(a+13,0xfff8);}
 else put(a+13,bw(b,p+4));
 return 1;
}
static unsigned tile_at(unsigned x,unsigned y){return ((y&255)>>3)*64+((x&511)>>3);}
static unsigned lowadd(unsigned p,int delta){return (p&0x700)|((p+delta)&255);}
static int floor_contact(const uint8_t *a,const uint8_t *grid)
{
 unsigned at=tile_at(word(a+10)+(a[33]?-12:11),(uint8_t)(a[13]+16));
 for(unsigned i=0;i<3;++i)if(grid[lowadd(at,a[33]?(int)i:-(int)i)])return 1;
 return 0;
}
/* BANK4:A793/A7DC. Ladders retain the original eight 16-byte E800
 * descriptors. Carry from the 16-bit interval calculation is significant
 * at both horizontal boundaries; only A793 snaps Y to the ladder top. */
static const uint8_t *ladder_top(uint8_t *a,const uint8_t *ladders,int ahead)
{
 if(!ladders)return 0;
 unsigned right=(word(a+10)+(ahead?(a[33]?1:24):24))&65535;
 unsigned width=ahead?24:48;
 for(unsigned i=0;i<8;++i){const uint8_t *l=ladders+i*16;
  if(!l[0])break;
  if((uint16_t)(word(l+10)-right)<65536-width)continue;
  unsigned feet=(uint8_t)(a[13]+17);
  if(feet<l[13] || feet-l[13]>=8)continue;
  if(!ahead)a[13]=(uint8_t)(l[13]-16);
  return l;
 }
 return 0;
}
/* BANK4:A81D consumes RNG even when the broad support match is not
 * horizontally aligned closely enough to enter the ladder. */
static void descend(uint8_t *a,const uint8_t *l,const uint8_t *g,
 spang_round_frame *r,const uint8_t *b)
{
 unsigned threshold=b[0x287d+((g[0x52]>>3)&3)];
 if((spang_items_random(&r->seed)&31)>=threshold)return;
 if((uint16_t)(word(l+10)-word(a+10)-1)<65534)return;
 if((uint8_t)(a[13]+16)!=l[13])return;
 a[37]=l[1];put(a+10,word(l+10));a[3]=3;a[4]=0;
}
static void turn(uint8_t *a,const uint8_t *b)
{a[33]^=2;a[4]=a[33]?3:0;pose(a,b,0xa8a5);}
static void exit_edge(uint8_t *a)
{
 unsigned x=word(a+10);
 if((!a[33] && x>=436) || (a[33] && x<84)){a[0]=4;a[3]=9;}
}
/* A67B/A6EA, including the third-wall escape and single8px step. */
static int side(uint8_t *a,const uint8_t *grid,const uint8_t *b,int left)
{
 unsigned x=(word(a+10)+(left?-12:11))&65535;
 unsigned at=tile_at(x,(uint8_t)(a[13]-9));int wall=0;
 for(unsigned i=0;i<3;++i)if(grid[(at+i*64)&2047]){wall=1;break;}
 if(!wall){
  at=(at+192)&2047;if(!grid[at])return 0;
  unsigned adjacent=lowadd(at,left?1:-1);
  if(!grid[adjacent])return 1;
  if(!grid[(adjacent+(left?0xfeff:0xff01))&2047])return 0;
 }
 if(a[38]==3){exit_edge(a);return 0;}
 put(a+10,word(a+10)+(left?(int)((~x)&7)+1:-(int)((x&7)+1)));
 if(++a[38]>=3){a[38]=3;a[39]=2;exit_edge(a);return 0;}
 turn(a,b);return 0;
}
static const uint8_t sampled[4][8]={{0,4,8,12,16,20,24,28},{1,5,9,13,17,21,25,29},{2,6,10,14,18,22,26,30},{3,7,11,16,19,23,27,31}};
static void eat_small(uint8_t *a,uint8_t *balls,uint8_t *g,int delta,crocodile_sound_fn sound,void *ctx)
{
 unsigned x=((word(a+10)+delta)&65535)>>1;uint8_t y=(uint8_t)(a[13]-8);
 for(unsigned i=0;i<8;++i){uint8_t *ball=balls+sampled[g[9]&3][i]*32;
  if(ball[28]!=1 || !ball[27])continue;
  if((uint8_t)(ball[27]+3-x)>=7 || (uint8_t)(ball[13]+7-y)>=15)continue;
  ball[0]=ball[1]=ball[27]=0;--g[0x20];if(sound)sound(ctx,0x0c);
 }
}
static void touch_balls(uint8_t *a,uint8_t *balls,const uint8_t *g,int delta)
{
 if(a[36]){--a[36];return;}
 unsigned x=((word(a+10)+delta)&65535)>>1;
 static const uint8_t rx[5]={4,5,8,10,14},ry[5]={19,21,27,31,39};
 for(unsigned i=0;i<8;++i){uint8_t *ball=balls+sampled[g[9]&3][i]*32;
  if(!ball[27])continue;
  unsigned k=ball[28]>=1 && ball[28]<=4?ball[28]-1:4;
  if((uint8_t)(ball[27]+rx[k]-x)>=2*rx[k]+1 || (uint8_t)(ball[13]+ry[k]-a[13])>=2*ry[k]+1)continue;
  a[36]=60;ball[0]=2;put(ball+2,0x913f);ball[27]=0;return;
 }
}
static int explosion(uint8_t *a,uint8_t *balls,const uint8_t *g,spang_round_frame *r,const uint8_t *b)
{
 unsigned ptr=bw(b,0xa542+2*(g[9]&3)),x=word(a+10)>>1;
 if(ptr<0xea00 || ptr>=0xee00)return -607;
 for(unsigned i=0;i<8;++i){unsigned slot=(ptr-0xea00+i*128)&1023;uint8_t *ball=balls+slot;
  if(ball[28]!=1 || !ball[27])continue;
  unsigned dx=(uint8_t)(ball[27]+15-x);if(dx>=31 || b[0x2881+dx]>=ball[13])continue;
  ball[0]=2;ball[27]=0;put(ball+2,0x912e);
  if(r->queue_length>62)return -608;
  r->queue[r->queue_length++]=(uint8_t)(10+a[24]);r->queue[r->queue_length++]=0x24;
  for(unsigned n=0;n<8;++n){uint8_t *score=r->scores+n*16;if(score[0])continue;
   score[0]=2;score[10]=ball[10];score[11]=ball[11];score[13]=ball[13];score[1]=a[24];score[15]=0x22;break;
  }
 }
 return 0;
}
static int fall_stunned(uint8_t *a,const uint8_t *g,const uint8_t *grid,const uint8_t *ladders)
{
 if(g[9]&1)return 0;
 if(ladder_top(a,ladders,0))return 0;
 if(floor_contact(a,grid))return 0;
 a[0]=2;a[3]=6;a[4]=0;return 1;
}
static unsigned player_side(const uint8_t *p1,const uint8_t *p2)
{
 if(!p1[0])return p2[0]?p2[11]:0;
 if(!p2[0] || p1[11]+p2[11]!=1)return p1[11];
 unsigned b=p1[11]?p1[10]:(uint8_t)(-p1[10]),c=p2[11]?p2[10]:(uint8_t)(-p2[10]);
 return b<c?p2[11]:p1[11];
}
static int hurt(uint8_t *a,const uint8_t *g,uint8_t *balls,const uint8_t *grid,spang_round_frame *r,const uint8_t *b,const uint8_t *ladders)
{
 switch(a[3]){
 case 0:case 9:
  if(a[4]==0){a[4]=3;a[19]=40;pose(a,b,0xa8a9);}
  else if(a[4]==3){if(fall_stunned(a,g,grid,ladders))break;if(animate(a,b))return -601;if(!--a[19]){put(a+30,120);a[4]=6;a[0]=3;}}
  else if(a[4]==6){
   if(fall_stunned(a,g,grid,ladders))break;
   if(animate(a,b))return -601;
   put(a+30,word(a+30)-1);
   if(!word(a+30)){a[0]=1;a[25]=1;a[2]=3;a[3]=0;a[4]=a[33]?3:0;a[38]=a[39]=0;pose(a,b,0xa8a5);}
  }else return -609;
  break;
 case 3:case 6:
  if(a[4]==0){a[4]=3;a[19]=120;a[22]=a[23]=0;pose(a,b,0xa8a9);}
  else if(a[4]==3){
   ++a[19];spang_actor_step_y(a,0x20);
   if(!ladder_top(a,ladders,0)){
    if(!floor_contact(a,grid))break;
    a[13]=(uint8_t)((a[13]+16)&0xf8)-16;
   }
   a[3]=a[4]=0;
  }
  else return -609;
  break;
 case 12:
  if(a[4]==0){a[4]=3;put(a+22,0xfc00);pose(a,b,0xa8ad);break;}
  if(a[4]==3){unsigned sum=word(a+22)+0x28;if(sum>65535){a[4]=6;sum=0;}put(a+22,sum);move(a+12,(int16_t)word(a+22));}
  else if(a[4]==6){spang_actor_step_y(a,0x28);if(!a[14] && a[13]>=200){a[3]=15;a[4]=0;a[13]=200;break;}}
  else return -609;
  move(a+9,a[18]?-0x300:0x300);
  if((!a[18] && word(a+10)>=428) || (a[18] && word(a+10)<84)){a[18]^=2;a[33]^=2;pose(a,b,0xa8ad);}
  break;
 case 15:
  if(a[4]==0){a[4]=3;a[19]=12;animation(a,b,0xa94b);r->sounds[4]=0x14;r->sounds[5]=0x20;}
  else if(a[4]==3){if(animate(a,b))return -601;if(!--a[19]){a[4]=6;a[19]=84;}}
  else if(a[4]==6){if(animate(a,b))return -601;int result=explosion(a,balls,g,r,b);if(result)return result;if(!--a[19])a[2]=9;}
  else return -609;
  break;
 default:return -611;
 }
 return 0;
}
int crocodile_step_with_ladders(uint8_t a[40],uint8_t s[32],uint8_t g[128],const uint8_t p1[128],const uint8_t p2[128],
 uint8_t balls[1024],const uint8_t grid[2048],spang_round_frame *r,const uint8_t b[16384],uint8_t draw[40],crocodile_sound_fn sound,void *ctx,const uint8_t ladders[128])
{
 if(a[17]!=3 || a[33]>2 || (a[33]&1))return -612;
 if(a[2]==9){--s[28];memset(a,0,6);++g[0x1e];return 0;}
 if(a[2]==0){
  if(a[3]==0){
   a[25]=a[1]=1;a[22]=a[23]=a[35]=a[36]=a[37]=a[38]=a[39]=a[18]=a[33]=0;
   if(!a[15]){put(a+13,0xfff8);a[3]=3;pose(a,b,0xa8a1);}
   else{a[3]=6;a[18]=a[33]=(uint8_t)((player_side(p1,p2)^1)*2);put(a+20,bw(b,0x9aad+a[33]));put(a+10,bw(b,0x9ab1+a[33]));pose(a,b,0xa8a5);}
   return 0;
  }
  if(a[3]==3){
   if(!g[0x2b])spang_actor_step_y(a,0x20);
   memcpy(draw,a,40);
   if(!g[0x2b] && (uint16_t)(word(a+13)+16)>=24){a[0]=1;a[2]=3;a[3]=6;pose(a,b,0xa8b1);}
   return 1;
  }
  if(a[3]!=6)return -613;
  if(animate(a,b))return -601;
  move(a+9,(int16_t)word(a+20));memcpy(draw,a,40);
  if((uint16_t)(word(a+10)-84)<344){
   a[0]=1;a[2]=3;a[3]=0;a[4]=a[33]?3:0;
   if(p1[0] && p2[0])a[34]=(uint8_t)((uint16_t)(word(p1+10)-word(a+10))<(uint16_t)(word(p2+10)-word(a+10))?(a[33]^2):a[33]);
   else a[34]=p1[0]?0:2;
  }
  return 1;
 }
 if(a[2]==3){
  if(!a[25]){a[0]=2;a[2]=6;a[4]=a[5]=0;}
  else if(!g[0x2b]){
   if(a[3]==0){
    if(a[4]==0 || a[4]==3){
     int left=a[4]==3;if(animate(a,b))return -601;move(a+9,left?-192:192);
     eat_small(a,balls,g,left?-8:8,sound,ctx);touch_balls(a,balls,g,left?8:-8);
     if(side(a,grid,b,left)){a[4]=left?9:6;animation(a,b,left?0xa926:0xa921);}
     else if(a[0]!=3){
      switch(g[9]&3){
      case 0:{unsigned at=tile_at(word(a+10)+(a[33]?-12:11),(uint8_t)(a[13]+16));if(a[39]!=2 && !grid[at] && !ladder_top(a,ladders,1)){++a[39];turn(a,b);}break;}
      case 2:if(!a[35]){const uint8_t *p=a[34]?p2:p1;if((uint16_t)(word(a+10)+71-word(p+10))<143){++a[35];turn(a,b);}}break;
      default:{const uint8_t *l=ladder_top(a,ladders,0);
       if(l)descend(a,l,g,r,b);
       else if(!floor_contact(a,grid)){a[3]=6;a[4]=a[22]=a[23]=0;}
       break;}
      }
     }
    }else if(a[4]==6 || a[4]==9){touch_balls(a,balls,g,a[33]?8:-8);if(!--a[8]){a[13]-=8;a[4]-=6;pose(a,b,0xa8a5);}}
    else return -609;
   }else if(a[3]==3){
    if(a[4]==0){a[4]=3;animation(a,b,0xa935);}
    else if(a[4]==3){
     if(!ladders || a[37]>=128 || (a[37]&15)!=12)return -610;
     if(animate(a,b))return -601;
     ++a[13];touch_balls(a,balls,g,0);
     if((uint8_t)(a[13]+17)>=ladders[a[37]])a[3]=a[4]=a[38]=a[39]=0;
    }else return -609;
   }else if(a[3]==6){
    if(a[4]==0){a[4]=3;a[19]=0;pose(a,b,0xa8b1);}
    else if(a[4]==3){
     ++a[19];spang_actor_step_y(a,0x20);
     int supported=ladder_top(a,ladders,0)!=0;
     if(supported || floor_contact(a,grid)){
      if(!supported)a[13]=(uint8_t)(((uint8_t)(a[13]+16)&0xf8)-16);
      if(a[19]>=12)a[38]=a[39]=0;
      a[3]=0;a[4]=a[33]?3:0;a[22]=a[23]=0;pose(a,b,0xa8a5);
     }
    }else return -609;
   }else if(a[3]==9){
    if(a[4]!=0 && a[4]!=3)return -609;
    if(animate(a,b))return -601;
    int left=a[4]==3;move(a+9,left?-192:192);
    if((left && word(a+10)<48) || (!left && word(a+10)>=464))a[2]=9;
   }else return -610;
  }
 }
 if(a[2]==6){int result=hurt(a,g,balls,grid,r,b,ladders);if(result)return result;}
 else if(a[2]!=3 && a[2]!=9)return -614;
 memcpy(draw,a,40);return 1;
}
int crocodile_step(uint8_t a[40],uint8_t s[32],uint8_t g[128],const uint8_t p1[128],const uint8_t p2[128],
 uint8_t balls[1024],const uint8_t grid[2048],spang_round_frame *r,const uint8_t b[16384],uint8_t draw[40],crocodile_sound_fn sound,void *ctx)
{return crocodile_step_with_ladders(a,s,g,p1,p2,balls,grid,r,b,draw,sound,ctx,0);}
int crocodile_sprite(const uint8_t a[40],const uint8_t b[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *room)
{
 unsigned animation_ptr=word(a+6);if(animation_ptr<0xa8b5 || animation_ptr>0xa974)return -601;
 unsigned command=bw(b,animation_ptr+1),cols=command==0x2635?4:2;
 if(command!=0x21ad && command!=0x2702 && command!=0x2635)return -615;
 unsigned count=cols*2;if(*room<count)return 0;
 if((*cursor&31) || *cursor+count*32>0xfe0)return -616;
 unsigned x=(word(a+10)-cols*8)&65535;uint8_t code=b[animation_ptr+3-0x8000],attr=b[animation_ptr+4-0x8000];
 for(unsigned col=0;col<cols;++col){
  unsigned xx=(x+col*16)&65535;attr|=(xx>>4)&16;
  for(unsigned n=0;n<2;++n){unsigned row=(col&1)?1-n:n,at=*cursor+(col*2+n)*32;
   uint16_t yy=(uint16_t)(word(a+13)-8+row*16);
   objects[at]=(uint8_t)(code+col+row*8);objects[at+1]=attr;
   objects[at+2]=command==0x2702?(yy&0x100?248:(uint8_t)(yy-8)):(uint8_t)(a[13]-16+row*16);objects[at+3]=(uint8_t)xx;
  }
 }
 *cursor+=(uint16_t)(count*32);*room-=(uint8_t)count;return (int)count;
}
static void weapon_hit(uint8_t *a,uint8_t *weapon,unsigned retire)
{a[24]=weapon[17];weapon[0]=2;put(weapon+2,retire);if(a[25])--a[25];}
int crocodile_rope_contact(uint8_t animals[320],uint8_t w[32],uint8_t frame)
{
 for(unsigned slot=frame&3;slot<8;slot+=4){uint8_t *a=animals+slot*40;if(a[0]!=1 || a[17]!=3)continue;
  if((uint8_t)((word(a+10)>>1)+7-w[27])>=15 || (uint8_t)(a[13]+12)<w[13] || (uint8_t)(a[13]-13)>=w[14])continue;
  weapon_hit(a,w,0x8234);return 1;
 }
 return 0;
}
int crocodile_bullet_contact(uint8_t animals[320],uint8_t w[24],uint8_t frame)
{
 for(unsigned slot=frame&3;slot<8;slot+=4){uint8_t *a=animals+slot*40;if(a[0]!=1 || a[17]!=3)continue;
  if((uint8_t)((word(a+10)>>1)+7-w[5])>=15 || (uint8_t)(a[13]+12-w[13])>=25)continue;
  weapon_hit(a,w,0x86cd);return 1;
 }
 return 0;
}
void crocodile_player_contact(uint8_t animals[320],const uint8_t p[128],uint8_t frame)
{
 for(unsigned slot=frame&3;slot<8;slot+=4){uint8_t *a=animals+slot*40;if(a[0]!=3 || a[17]!=3)continue;
  unsigned dx=(uint16_t)(word(a+10)+17-word(p+10));if(dx>=35 || (uint8_t)(p[13]+17-a[13])>=35)continue;
  a[18]=dx>=17?0:2;a[0]=4;a[3]=12;a[4]=a[5]=0;a[24]=(uint8_t)(p[17]*2);
 }
}
