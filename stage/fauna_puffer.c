/* Original Super Pang BANK4:8AB0..9495, translated to native C states.
 * Actor layout, arithmetic width, random calls and draw order are retained.
 * Tables below are read from original bank data; no code interpreter is used. */
#include "fauna_internal.h"
#include "spang_items.h"

static uint8_t byte(const fauna_context *c,unsigned address)
{return c->bank4[address-0x8000];}

static void move(uint8_t *a)
{
 /* FIXED134D wraps X at 24 bits, but Y at 16 bits (a[14] is untouched). */
 fa_move(a+9,(int16_t)fa_word(a+20));
 fa_put(a+12,fa_word(a+12)+fa_word(a+22));
}

/* BANK4:904F selects one of the original horizontal regions. */
static unsigned region(const uint8_t *a,unsigned mode)
{
 if(!a[11])return a[10]<0x9f?0:1;
 return (!mode?1u:0u)+(a[10]<0x60?1u:2u);
}

static unsigned choice(fauna_context *c,unsigned table,unsigned group)
{return byte(c,table+group*16+(spang_items_random(&c->round->seed)&15));}

static void velocity(fauna_context *c,uint8_t *a,unsigned table)
{
 unsigned p=table+a[37]*4;
 fa_put(a+20,fa_bw(c->bank4,p));fa_put(a+22,fa_bw(c->bank4,p+2));
}

static void hurt_pose(fauna_context *c,uint8_t *a)
{
 a[31]=a[37]<8?0:3;
 fa_animation(a,c->bank4,a[37]<8?0x9717:0x9701);
}

static void rise(fauna_context *c,uint8_t *a)
{
 a[3]=6;a[18]=2;a[37]=6;a[38]=a[39]=0;a[19]=3;
 a[36]=byte(c,0x972d+a[37]);velocity(c,a,0x95d3);
 fa_animation(a,c->bank4,a[11]>=1?0x967d:0x9693);
}

static void turn(uint8_t *a,unsigned left)
{
 a[19]=a[left?38:36];a[18]=(uint8_t)left;
 fa_put(a+20,0);a[3]=(uint8_t)(left*2);
}

/* BANK4:8B15 is also reached after the stationary attack wait. It deliberately
 * does not reset current HP, the primary state, or substate bytes first. */
static void swimming(fauna_context *c,uint8_t *a)
{
 a[0]=1;a[5]=0;
 if(byte(c,0x9647+a[26])<a[13]){rise(c,a);return;}
 a[4]=0;fa_put(a+22,0);
 a[15]=(uint8_t)choice(c,0x9520,region(a,1));
 memcpy(a+36,c->bank4+0x1514+a[15]*4,4);
 unsigned left=a[11]>=1;turn(a,left);
 fa_animation(a,c->bank4,left?0x94fa:0x94ff);
}

static void waiting(fauna_context *c,uint8_t *a)
{
 a[3]=10;a[4]=0;a[5]=1;a[19]=100;a[18]=6;
 fa_animation(a,c->bank4,a[31]<2?0x96eb:0x96d5);
}

/* FIXED163C / FIXED164C have independent allocation ranges. They do not
 * initialize unrelated bytes in a reused transient-effect record. */
static uint8_t *effect(fauna_context *c,unsigned first,unsigned count)
{
 for(unsigned i=first;i<first+count;++i){uint8_t *p=c->transients+i*16;if(!p[0])return p;}
 return 0;
}

static void bubble(fauna_context *c,const uint8_t *a)
{
 if((spang_items_random(&c->round->seed)&15)<13)return;
 uint8_t *p=effect(c,8,8);if(!p)return;
 p[0]=2;p[10]=a[10];p[11]=a[11];p[13]=a[13];fa_put(p+2,0x9d39);
}

static void impact(fauna_context *c,const uint8_t *a,const uint8_t *ball,unsigned side)
{
 uint8_t *p=effect(c,0,4);if(!p)return;
 if(side<8){
  unsigned t=0x965d+side*4;
  fa_put(p+10,fa_word(a+10)+fa_bw(c->bank4,t));
  p[13]=(uint8_t)(a[13]+byte(c,t+2));
 }else{
  const uint8_t *source=side==9?a:ball;
  p[10]=source[10];p[11]=source[11];p[13]=source[13];
 }
 p[14]=0;p[0]=2;fa_put(p+2,0x9603);
}

/* BANK4:90F3..9495 visits one quarter of the 32-ball pool and stops at the
 * first contact. Coordinate differences are unsigned bytes, as in the ROM. */
static void balls(fauna_context *c,uint8_t *a)
{
 if(!a[25] || a[3]>=10)return;
 a[24]=4;
 for(unsigned i=c->globals[9]&3;i<32;i+=4){
  uint8_t *p=c->balls+i*32;if(!p[27])continue;
  unsigned size=p[28],rx,ry,dx,dy,side,damage;
  if(size==1){rx=6;ry=13;damage=3;}
  else if(size==2){rx=8;ry=17;damage=5;}
  else if(size==3){rx=12;ry=25;damage=6;}
  else if(size==4){rx=16;ry=33;damage=8;}
  else{rx=20;ry=41;damage=10;}
  dx=(uint8_t)(p[27]+rx-(fa_word(a+10)>>1));
  if(dx>=rx*2+1)continue;
  dy=(uint8_t)(p[13]+ry-a[13]);if(dy>=ry*2+1)continue;
  if(size==1)side=8;
  else{
   unsigned lo=size==2?5:size==3?5:size==4?7:9;
   unsigned hi=size==2?13:size==3?21:size==4?27:33;
   unsigned top=size==2?10:size==3?10:size==4?14:18;
   unsigned bottom=size==2?26:size==3?42:size==4?54:66;
   side=dx<lo?5:dx>=hi?2:0;
   if(dy<top){if(side)--side;}
   else if(dy>=bottom)side=side?side+1:7;
   else if(!side)side=9;
  }
  a[25]=(uint8_t)(a[25]>=damage?a[25]-damage:0);
  p[0]=2;p[27]=0;fa_put(p+2,0x913f);
  impact(c,a,p,side);return;
 }
}

static int health(fauna_context *c,uint8_t *a)
{
 if(a[25]==a[26])return 0;
 if(a[25]){
  a[36]=byte(c,0x963c+(uint8_t)(a[26]-a[25]));
  a[37]=byte(c,0x9633+a[18]);a[3]=14;velocity(c,a,0x95f3);
  a[19]=10;a[0]=2;a[5]=1;a[26]=a[25];hurt_pose(c,a);return 0;
 }
 a[19]=48;a[0]=2;a[2]=4;a[5]=1;fa_animation(a,c->bank4,0x9735);
 if(c->sound)c->sound(c->sound_context,0x0e);
 return a[24]<3?fauna_queue(c,0x0a+a[24],0x0a):0;
}

static void attack(fauna_context *c,uint8_t *a)
{
 a[18]=4;a[3]=8;a[4]=a[5]=0;
 unsigned group=region(a,0);a[31]=(uint8_t)group;
 a[37]=(uint8_t)choice(c,0x9593,group);
 a[36]=a[19]=byte(c,0x9550+a[37]);
 fa_animation(a,c->bank4,group<2?0x96bf:0x96a9);
 unsigned entry=0x9577+a[37]*4,vx=fa_bw(c->bank4,entry);
 if(group<2)fa_put(a+20,vx);
 else{
  /* Original 90A7 adjusts the X fraction once, then uses an integral
   * negative velocity. Negating the entire 8.8 value would change its path. */
  a[20]=0;a[21]=(uint8_t)(-(int)(vx>>8));
  unsigned old=a[9],fraction=vx&255;a[9]=(uint8_t)(old-fraction);
  if(old<fraction)fa_put(a+10,fa_word(a+10)-1);
 }
 fa_put(a+22,0);
 a[30]=(uint8_t)(byte(c,entry+2)-byte(c,0x9652+a[26]));
}

static void bounce(fauna_context *c,uint8_t *a)
{
 unsigned angle=a[37];
 if(a[13]<=32 || a[13]>192){a[13]=a[13]<=32?32:192;angle=(angle+8)&15;}
 else if(!a[11]){if(a[10]>88)return;a[10]=88;}
 else{if(a[10]<=168)return;a[10]=168;}
 a[37]=(uint8_t)((16-angle)&15);velocity(c,a,0x95f3);
}

static int active(fauna_context *c,uint8_t *a)
{
 int r;
 switch(a[3]){
 case 0:case 2:{
  unsigned left=a[3]==2;move(a);
  if(!a[4]){
   fa_put(a+20,fa_word(a+20)+(left?-64:64));fa_put(a+22,fa_word(a+22)+32);
   if(!--a[19]){a[4]=2;a[19]=a[left?39:37];}
  }else if(a[4]==2){
   fa_put(a+20,fa_word(a+20)+(left?128:-128));fa_put(a+22,fa_word(a+22)-96);
   if(!--a[19]){a[4]=0;a[3]=4;a[19]=2;}
  }else return -741;
  return 0;
 }
 case 4:
  if(!--a[19]){
   fa_put(a+22,0);
   if(byte(c,0x9647+a[26])<a[13])rise(c,a);else turn(a,a[18]==0);
  }
  return 0;
 case 6:
  r=fauna_animate(a,c->bank4);if(r)return r;
  if(!a[38] && ++a[39]>=180)a[38]=1;
  if(--a[19])return 0;
  a[19]=3;move(a);if(--a[36])return 0;
  if(a[37]==5 && a[38]){attack(c,a);return 0;}
  a[37]=(a[37]+1)&7;
  if(a[37]==6)a[18]=2;else if(a[37]==2)a[18]=3;
  a[36]=byte(c,0x972d+a[37]);velocity(c,a,0x95d3);return 0;
 case 8:
  r=fauna_animate(a,c->bank4);if(r)return r;move(a);
  if(!a[4]){
   fa_put(a+22,fa_word(a+22)-a[30]);
   if(!--a[19]){a[18]=5;a[4]=2;a[19]=a[36];}
  }else if(a[4]==2){
   fa_put(a+22,fa_word(a+22)+a[30]);
   if(!--a[19])waiting(c,a);
  }else return -742;
  return 0;
 case 10:
  r=fauna_animate(a,c->bank4);if(r)return r;
  if(--a[19])return 0;
  if((spang_items_random(&c->round->seed)&15)>=byte(c,0x9504+((c->globals[0x52]>>1)&15))){swimming(c,a);return 0;}
  a[3]=12;a[4]=0;a[5]=1;a[18]=7;fa_put(a+20,0x300);fa_put(a+22,0);
  a[31]=(uint8_t)(a[11]*2);
  if(a[31]>=2){a[18]=8;a[21]=0xfd;}
  fa_animation(a,c->bank4,a[31]<2?0x96bf:0x96a9);return 0;
 case 12:
  move(a);r=fauna_animate(a,c->bank4);if(r)return r;
  if(a[31]<2){if(a[11] && a[10]>=207)a[2]=6;}
  else if(!a[11] && a[10]<47)a[2]=6;
  return 0;
 case 14:
  move(a);r=fauna_animate(a,c->bank4);if(r)return r;bounce(c,a);bubble(c,a);
  if(--a[19])return 0;
  if(!--a[36]){a[0]=1;waiting(c,a);return 0;}
  a[19]=10;
  {unsigned n=spang_items_random(&c->round->seed)&15;
   if(a[13]<156)a[37]=(a[37]+byte(c,0x9557+n))&15;
   else{unsigned delta=byte(c,0x9567+n);a[37]=(a[37]+(a[37]<8?-delta:delta))&15;}}
  velocity(c,a,0x95f3);hurt_pose(c,a);return 0;
 default:return -743;
 }
}

int fauna_puffer_step(fauna_context *c,unsigned slot,uint8_t draw[40])
{
 if(slot>=8 || !c->transients)return -740;
 uint8_t *a=c->actors+slot*40;int r;
 switch(a[2]){
 case 0:
  if(!a[3]){
   unsigned index=spang_items_random(&c->round->seed)&15;
   unsigned pos=0x975d+byte(c,fa_word(c->scheduler+30)+index);
   fa_put(a+10,fa_bw(c->bank4,pos));a[13]=byte(c,pos+2);
   a[5]=0;a[19]=70;a[3]=2;a[1]=1;
   fa_animation(a,c->bank4,region(a,0)<2?0x94c8:0x9496);
  }else if(a[3]!=2)return -744;
  /* Appearance draws before advancing animation or making the actor active. */
  memcpy(draw,a,40);r=fauna_animate(a,c->bank4);if(r)return r;
  if(!--a[19]){a[25]=a[26]=10;a[2]=2;swimming(c,a);}
  return 1;
 case 2:
  if(c->globals[0x2b]){
   if(a[3]==14 && !--a[19]){a[19]=10;if(!--a[36]){a[0]=1;waiting(c,a);}}
  }else{
   r=active(c,a);if(r)return r;balls(c,a);
  }
  r=health(c,a);if(r)return r;memcpy(draw,a,40);return 1;
 case 4:
  r=fauna_animate(a,c->bank4);if(r)return r;memcpy(draw,a,40);
  if(!--a[19])a[2]=6;
  return 1;
 case 6:
  --c->scheduler[28];--c->scheduler[29];memset(a,0,6);++c->globals[0x1e];return 0;
 default:return -745;
 }
}
