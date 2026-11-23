#include "fauna_internal.h"
#include "spang_items.h"

/* Native BANK4:A976..AF4B. Links are original FA00 addresses, not host
 * pointers. A leader remains reserved after leaving until its flock retires. */
static uint8_t *linked(fauna_context *c,unsigned address)
{if(address<0xfa00 || address>=0xfb40 || (address-0xfa00)%40)return 0;return c->actors+(address-0xfa00);}
static uint8_t *leader(fauna_context *c,uint8_t *a)
{return a[36]?a:linked(c,fa_word(a+27));}
static int unlink_child(fauna_context *c,uint8_t *a)
{uint8_t *p=linked(c,fa_word(a+27));if(!p || a[29]<27 || a[29]>31)return -731;fa_put(p+a[29],0);return 0;}
static int transfer_contact(fauna_context *c,uint8_t *a)
{
 if(!a[37])return 0;
 uint8_t *p=leader(c,a);if(!p || a[34]>3)return -731;a[37]=0;
 for(unsigned i=1;i<=4;++i){unsigned n=(a[34]+i)&3;
  if(!n){if(p[0]!=9){p[37]=1;return 0;}continue;}
  unsigned address=fa_word(p+25+n*2);if(!address)continue;
  uint8_t *next=linked(c,address);if(!next)return -731;next[37]=1;return 0;
 }
 return -732; /* A positive flock count must have a remaining member. */
}
static int segment(uint8_t *a,const uint8_t *b)
{
 unsigned p=((unsigned)a[4]<<8)|a[5];if(p<0x8000 || p>0xbffe)return -733;
 if(!b[p-0x8000])return 9;
 a[19]=b[p-0x8000];a[18]=b[p+1-0x8000];p+=2;a[4]=(uint8_t)(p>>8);a[5]=(uint8_t)p;return 0;
}
static int velocity(uint8_t *a,const uint8_t *b)
{
 if(a[18]>=16)return -734;
 unsigned p=0xb06c+a[18]*4;fa_put(a+20,fa_bw(b,p));fa_put(a+22,fa_bw(b,p+2));return 0;
}
static int flight_pose(uint8_t *a,const uint8_t *b)
{int r=velocity(a,b);if(r)return r;fa_animation(a,b,fa_bw(b,0xaf4c+a[26])+5*a[18]);return 0;}
static void slow_pose(uint8_t *a,const uint8_t *b)
{fa_animation(a,b,fa_bw(b,(a[18]<8?0xaf50:0xaf54)+a[26]));}
static void slow_start(uint8_t *a,const uint8_t *b)
{a[39]=44;a[35]=8;slow_pose(a,b);}
static void exit_start(uint8_t *a,const uint8_t *b)
{a[0]=2;a[35]=4;if(a[38])fa_animation(a,b,fa_bw(b,0xaf5c+a[26]));}
static void move(uint8_t *a,int wide_y)
{
 fa_move(a+9,(int16_t)fa_word(a+20));
 if(wide_y)fa_move(a+12,(int16_t)fa_word(a+22));else fa_put(a+12,fa_word(a+12)+(int16_t)fa_word(a+22));
}
static int ball_contact(fauna_context *c,uint8_t *a)
{
 if(!a[37])return 0;
 static const uint8_t rx[5]={6,8,12,16,20},ry[5]={13,17,25,33,41};
 unsigned x=fa_word(a+10)>>1;
 for(unsigned slot=c->globals[9]&3;slot<32;slot+=4){uint8_t *ball=c->balls+slot*32;if(!ball[27])continue;
  unsigned k=ball[28]>=1 && ball[28]<=4?ball[28]-1:4;
  if((uint8_t)(ball[27]+rx[k]-x)>=2*rx[k]+1 || (uint8_t)(ball[13]+ry[k]-a[13])>=2*ry[k]+1)continue;
  unsigned type=ball[15];if(type>36 || (type&1))return -735;
  unsigned rule=fa_word(c->fixed+0x170b+type),dir;
  if((rule&255)==9)dir=(7-ball[18])&7;else dir=(ball[18]+(rule&255))&(rule>>8);
  ball[18]=(uint8_t)dir;fa_put(ball+2,fa_bw(c->bank1,0x8200+type*4+dir*2));
  if(c->sound)c->sound(c->sound_context,7);
  a[3]=6;slow_start(a,c->bank4);return 1;
 }
 return 0;
}
static int killed(fauna_context *c,uint8_t *a,uint8_t *draw)
{
 a[19]=24;a[0]=2;a[2]=4;fa_animation(a,c->bank4,0xb044);memcpy(draw,a,40);
 if(a[26])spang_items_spawn(c->items,c->bank4[0x3208+c->globals[0x19]],fa_word(a+10),a[13]);
 uint8_t *p=leader(c,a);if(!p)return -731;
 if(a[36])a[0]=9;
 --p[33];a[39]=p[33];
 if(p[33]){int r=transfer_contact(c,a);if(r)return r;if(!a[36]){r=unlink_child(c,a);if(r)return r;}}
 if(c->sound)c->sound(c->sound_context,14);
 return 1;
}
int fauna_bird_step(fauna_context *c,unsigned slot,uint8_t draw[40])
{
 uint8_t *a=c->actors+slot*40;const uint8_t *b=c->bank4;int r;
 if(a[26]!=0 && a[26]!=2)return -736;
 switch(a[2]){
 case 0:{
  if(a[15]>=15)a[15]=0;
  a[2]=2;a[25]=1;unsigned p=fa_bw(b,0xb0b8+a[15]);
  fa_put(a+10,fa_bw(b,p));fa_put(a+13,fa_bw(b,p+2));p+=4;a[5]=(uint8_t)p;a[4]=(uint8_t)(p>>8);a[38]=(uint8_t)(a[14]==255);return 0;
 }
 case 2:
  if((a[3]&4) && !a[25])return killed(c,a,draw);
  if(c->globals[0x2b]){if(!a[3])return 0;memcpy(draw,a,40);return 1;}
  switch(a[3]){
  case 0:
   if(--a[19])return 0;
   a[3]=2;r=segment(a,b);if(r<0)return r;a[1]=1;a[35]=4;
   if(a[38]){r=velocity(a,b);if(r)return r;fa_animation(a,b,fa_bw(b,0xaf58+a[26]));return 0;}
   return flight_pose(a,b);
  case 2:
   move(a,1);memcpy(draw,a,40);
   if(!--a[35]){a[0]=1;a[3]=4;r=flight_pose(a,b);if(r)return r;}return 1;
  case 4:
   memcpy(draw,a,40);r=ball_contact(c,a);if(r<0)return r;if(r)return 1;
   move(a,0);
   if(!--a[19]){r=segment(a,b);if(r<0)return r;if(r==9){a[3]=8;exit_start(a,b);}else{r=flight_pose(a,b);if(r)return r;}}return 1;
  case 6:
   r=fauna_animate(a,b);if(r)return r;memcpy(draw,a,40);
   if(!--a[39]){a[3]=4;r=transfer_contact(c,a);if(r)return r;r=flight_pose(a,b);if(r)return r;return 1;}
   if(--a[35])return 1;
   a[35]=8;move(a,0);
   if(!--a[19]){r=segment(a,b);if(r<0)return r;if(r==9){a[3]=8;exit_start(a,b);}else{r=velocity(a,b);if(r)return r;slow_pose(a,b);}}return 1;
  case 8:{
   move(a,1);memcpy(draw,a,40);
   if(--a[35])return 1;
   uint8_t *p=leader(c,a);if(!p)return -731;--p[33];
   if(a[36])a[0]=9;else{r=unlink_child(c,a);if(r)return r;}
   a[2]=6;return 1;
  }
  default:return -737;
  }
 case 4:
  r=fauna_animate(a,b);if(r)return r;memcpy(draw,a,40);
  if(!--a[19]){
   a[2]=6;if(a[39]>3)return -738;
   fauna_score(c,a,b[0x30b4+a[39]]);unsigned award=fa_bw(b,0xb0ac+2*a[39]);
   r=fauna_queue(c,(award>>8)+a[24],award&255);if(r)return r;
  }
  return 1;
 case 6:
  if(a[36]){if(a[33])return 0;--c->scheduler[28];}
  memset(a,0,6);++c->globals[0x1e];return 0;
 default:return -739;
 }
}
