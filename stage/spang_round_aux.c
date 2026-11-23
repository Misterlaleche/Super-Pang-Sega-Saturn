#include "spang_round_aux.h"
#include "spang_items.h"
#include "spang_sprite_overlay.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned bw(const uint8_t *b,unsigned a){return word(b+(a-0x8000));}
static int space(uint16_t cursor,uint8_t remaining)
{return !(cursor&31) && cursor<=0xfe0 && cursor+(unsigned)remaining*32<=4096;}
int spang_round_stop_step(spang_round_frame *s,uint8_t g[128],const uint8_t b[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance)
{
 if(!s || !g || !b || !objects || !cursor || !remaining || !space(*cursor,*remaining))return -120;
 uint8_t a[32];memcpy(a,s->stop,32);if(!a[0])return 0;
 unsigned pc=word(a+2);int starting=pc==0x9379 || pc==0x937d || pc==0x9380 || pc==0x9384;
 if(!starting && pc!=0x93ba && pc!=0x93dd && pc!=0x93f5)return -121;
 if(starting){
  if(!advance)return 0;
  uint16_t seed=s->seed;
  unsigned offset=pc==0x9379?4:pc==0x937d?0:pc==0x9380?16:b[0x1437+(spang_items_random(&seed)&31)];
  if(offset>16 || (offset&3))return -122;
  unsigned duration=bw(b,0x9459+offset),anim=bw(b,0x9457+offset);
  if(anim<0x8000 || anim>0xbff2)return -122;
  put(a+2,0x93ba);
  /*938E does not restart a longer clock, nor clear an existing blink flag.*/
  if(word(a+30)<duration){put(a+30,duration);put(a+6,anim);a[8]=b[anim-0x8000];put(a+10,216);a[13]=32;s->sounds[2]=0x1c;s->sounds[3]=0x40;}
  memcpy(s->stop,a,32);s->seed=seed;return 0;
 }
 if(!advance)a[8]=2; /*9550 without941E: same visible frame, no ticking copy.*/
 unsigned first=*cursor;
 int result=spang_items_stop_sprite(a,b,objects,cursor,remaining);
 if(result<0)return -123;
 for(unsigned at=first;at<*cursor;at+=32){objects[at+4]=SPANG_SPRITE_NATIVE_TAG;put(objects+at+5,word(a+10));}
 if(!advance)return 0;
 if(pc==0x93ba){
  unsigned duration=(word(a+30)-1)&65535;put(a+30,duration);
  if(!duration){a[19]=4;a[29]=8;g[0x2c]=1;put(a+2,0x93dd);}
 }else if(!--a[19]){
  if(pc==0x93dd){g[0x2c]=0;a[19]=8;put(a+2,0x93f5);}
  else if(!--a[29]){g[0x2b]=g[0x2c]=0;a[0]=0;}
  else{g[0x2c]=1;a[19]=4;put(a+2,0x93dd);}
 }
 memcpy(s->stop,a,32);return 0;
}
/* A263 reserves slot0 until the original delayed music request completes.
 * Direct2BAF requests are exposed as events; 2BE3 keeps its raw mailbox.
 * This owner does not synthesize PCM or claim an audible audio backend. */
static int music_pc(unsigned pc)
{return pc==0xa263 || pc==0xa269 || pc==0xa2aa || pc==0xa2c1;}
static void music_step(spang_round_frame *s,const uint8_t *g,unsigned selection,
 unsigned attract,const uint8_t *b,uint8_t request[2])
{
 uint8_t *a=s->scores;unsigned pc=word(a+2),scene=g[0x18];
 if(pc==0xa263){put(a+2,0xa269);return;}
 if(pc==0xa2c1){
  if(--a[5])return;
  s->sounds[4]=0x0c;s->sounds[5]=0x20;
 }else{
  int send=0;
  if(pc==0xa2aa){if(--a[5])return;scene=2;send=1;}
  else if(g[0x0a]){
   if(scene==2){
    if(g[0x19]==6){request[0]=1;request[1]=0x23;a[5]=0x69;put(a+2,0xa2c1);}
    else{s->sounds[4]=0x0c;s->sounds[5]=0x20;a[5]=60;put(a+2,0xa2aa);}
    return;
   }
   send=1;
  }
  if(send && (g[0x0a] || attract)){request[0]=1;request[1]=b[0x22cc+(selection==12?15:0)+scene];}
 }
 a[0]=0;put(a+2,0xa102);
}
static int scores_step(spang_round_frame *s,const uint8_t *g,unsigned selection,
 unsigned attract,uint8_t request[2],const uint8_t b[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance)

{
 if(!s || !b || !objects || !cursor || !remaining || !space(*cursor,*remaining))return -124;
 /*Validate every record before publishing any changes. Stale free slots are
  * intentional and must not be normalized during allocation or retirement.*/
 for(unsigned i=0;i<8;++i){const uint8_t *a=s->scores+i*16;if(!a[0])continue;
  unsigned pc=word(a+2),anim;
  if(music_pc(pc)){
   if(i || !g || !request || g[0x19]>=40 || g[0x18]>=15)return -125;
   continue;
  }
  if(pc==0xa102){if(a[15]>94 || (a[15]&1))return -125;anim=bw(b,0xa142+a[15]);}
  else if(pc==0xa12d)anim=word(a+6);else return -125;
  if(anim<0x8000 || anim>0xbffc || bw(b,anim+1)!=0x2148)return -126;
 }
 if(advance && request)request[0]=request[1]=0;
 for(unsigned i=0;i<8;++i){uint8_t *a=s->scores+i*16;if(!a[0])continue;
  if(music_pc(word(a+2))){if(advance)music_step(s,g,selection,attract,b,request);continue;}
  if(word(a+2)==0xa102){
   if(advance){put(a+2,0xa12d);a[0]=1;a[1]=a[1]?0x64:0x63;a[5]=a[15]<24?10:30;unsigned anim=bw(b,0xa142+a[15]);put(a+6,anim);a[8]=b[anim-0x8000]&127;}
   continue;
  }
  if(advance && (!--a[13] || !--a[5])){a[0]=0;put(a+2,0xa102);continue;}
  if(*remaining<2)continue;
  unsigned x=(word(a+10)-16)&65535,code=b[word(a+6)-0x8000+3],attr=a[1];
  for(unsigned n=0;n<2;++n){uint8_t *o=objects+*cursor+n*32;unsigned xx=(x+n*16)&65535;
   /*2148 carries the first sprite attribute into the second (218D).*/
   attr|=(xx>>4)&16;o[0]=(uint8_t)(code+n);o[1]=(uint8_t)attr;o[2]=(uint8_t)(a[13]-8);o[3]=(uint8_t)xx;o[4]=SPANG_SPRITE_NATIVE_TAG;put(o+5,word(a+10));
  }
  *cursor+=64;*remaining-=2;
 }
 return 0;
}

int spang_round_scores_step(spang_round_frame *s,const uint8_t b[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance)
{return scores_step(s,0,0,0,0,b,objects,cursor,remaining,advance);}
int spang_round_scores_step_with_music(spang_round_frame *s,const uint8_t g[128],
 uint8_t selection,uint8_t attract,uint8_t request[2],const uint8_t b[16384],
 uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining,int advance)
{
 if(!g || !request)return -124;
 return scores_step(s,g,selection,attract,request,b,objects,cursor,remaining,advance);
}

int spang_round_slow_step(spang_round_frame *s,uint8_t globals[128],const uint8_t fixed[32768],int advance)
{
 if(!s || !globals || !fixed)return -127;
 if(!s->slow[0] || !advance)return 0;
 uint8_t a[32],g[128];memcpy(a,s->slow,32);memcpy(g,globals,128);
 unsigned pc=word(a+2);int apply=0;
 if(g[0x23] || g[0x22]>40 || g[0x28]>40)return -128;
 if(pc==0x956b){
  a[19]=10;g[0x29]=g[0x28]>10?(uint8_t)(g[0x28]-10):1;
  if(g[0x29]>=g[0x22]){apply=1;a[19]=30;put(a+2,0x95ab);}else put(a+2,0x9589);
 }else if(pc==0x9589){
  if(!--a[19]){a[19]=10;--g[0x22];apply=1;
   if(g[0x29]>=g[0x22]){a[19]=30;put(a+2,0x95ab);}
  }
 }else if(pc==0x95ab){if(!--a[19]){a[19]=15;put(a+2,0x95ba);}}
 else if(pc==0x95ba){
  if(!--a[19]){a[19]=15;++g[0x22];apply=1;
   if(g[0x28]<=g[0x22]){g[0x22]=g[0x28];a[0]=0;}
  }
 }else return -129;
 if(apply){
  unsigned speed=word(g+0x22);if(speed>40)return -128;
  memcpy(g+0x24,fixed+0x2f65+speed*2,2);memcpy(g+0x30,fixed+0x2fb7+speed*8,8);
 }
 memcpy(s->slow,a,32);memcpy(globals,g,128);return 0;
}

static int objects_step(spang_round_frame *s,uint8_t balls[1024],
 uint8_t items[256],uint8_t g[128],uint8_t p1[128],uint8_t p2[128],
 const uint8_t b1[16384],const uint8_t b2[16384],const uint8_t b3[16384],
 const uint8_t grid[2048],uint8_t objects[4096],uint16_t *cursor,
 uint8_t *remaining,uint8_t frame,int advance,uint8_t selection,uint8_t attract,uint8_t request[2])
{
 if(!s || !balls || !items || !g || !p1 || !p2 || !b1 || !b2 || !b3 || !grid || !objects || !cursor || !remaining || g[0x1a])return -130;
 int result=spang_round_stop_step(s,g,b3,objects,cursor,remaining,advance);
 if(result)return result;
 result=request?spang_round_scores_step_with_music(s,g,selection,attract,request,b2,objects,cursor,remaining,advance)
               :spang_round_scores_step(s,b2,objects,cursor,remaining,advance);
 if(result)return result;
 return spang_round_frame_step(s,balls,items,g,p1,p2,b1,grid,objects,cursor,remaining,frame,advance);
}

int spang_round_objects_step(spang_round_frame *s,uint8_t balls[1024],
 uint8_t items[256],uint8_t g[128],uint8_t p1[128],uint8_t p2[128],
 const uint8_t b1[16384],const uint8_t b2[16384],const uint8_t b3[16384],
 const uint8_t grid[2048],uint8_t objects[4096],uint16_t *cursor,
 uint8_t *remaining,uint8_t frame,int advance)
{return objects_step(s,balls,items,g,p1,p2,b1,b2,b3,grid,objects,cursor,remaining,frame,advance,0,0,0);}
int spang_round_objects_step_with_music(spang_round_frame *s,uint8_t balls[1024],
 uint8_t items[256],uint8_t g[128],uint8_t p1[128],uint8_t p2[128],
 const uint8_t b1[16384],const uint8_t b2[16384],const uint8_t b3[16384],
 const uint8_t grid[2048],uint8_t objects[4096],uint16_t *cursor,
 uint8_t *remaining,uint8_t frame,int advance,uint8_t selection,uint8_t attract,uint8_t request[2])
{
 if(!request)return -130;
 return objects_step(s,balls,items,g,p1,p2,b1,b2,b3,grid,objects,cursor,remaining,frame,advance,selection,attract,request);
}
