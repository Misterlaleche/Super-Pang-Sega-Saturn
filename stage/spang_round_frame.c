#include "spang_round_frame.h"
#include "spang_round_ball.h"
#include "spang_items.h"
#include "spang_sprite_overlay.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned bw(const uint8_t *bank,unsigned a){return word(bank+(a-0x8000));}
typedef struct {
 spang_round_frame *s;uint8_t *balls,*items,*g,*p[2],*objects;
 const uint8_t *bank,*grid;uint16_t *cursor;uint8_t *remaining;
 uint8_t frame;int advance,fault;
 spang_panic *panic;const uint8_t *fixed;
} view;
static uint8_t read_bus(void *ctx,uint16_t a)
{
 view *v=ctx;
 if(a>=0x8000 && a<0xc000)return v->bank[a-0x8000];
 if(a>=0xe080 && a<0xe100)return v->g[a-0xe080];
 if(a>=0xf000 && a<0xf800)return v->grid[a-0xf000];
 v->fault=-101;return 0;
}
static int animate(uint8_t *a,const uint8_t *bank)
{
 if(--a[8])return 0;
 unsigned old=word(a+6),next=old+5;
 if(old<0x8000 || old>0xbff9)return -102;
 if(bank[old-0x8000]&128)next=bw(bank,next);
 if(next<0x8000 || next>0xbffb)return -102;
 put(a+6,next);a[8]=bank[next-0x8000]&127;return 0;
}
static int draw(view *v,uint8_t *a,int native)
{
 uint8_t copy[32];uint8_t *p=a;
 if(!v->advance){memcpy(copy,a,32);p=copy;}
 /* The sprite routine can advance animation while drawing. Save the route
  * actually used for these cells before it mutates the actor. */
 unsigned anim=word(p+6),route=0;
 if(v->panic && !native && anim>=0x8000 && anim+4<0xc000)route=bw(v->bank,anim+1);
 unsigned first=*v->cursor;
 int count=spang_round_ball_sprite(p,v->bank,v->objects,v->cursor,v->remaining);
 if(count<0)return -103;
 unsigned width=count==16?4:(count==9 || count==10)?3:(count==4 || count==5)?2:1;
 unsigned origin=(word(a+10)-width*8)&65535;
 for(unsigned at=first;at<*v->cursor;at+=32){
  v->objects[at+4]=native?SPANG_SPRITE_NATIVE_TAG:SPANG_SPRITE_ANCHOR_TAG;
  put(v->objects+at+5,native?word(a+10):origin);
 }
 if(v->panic && !native && count){
  unsigned icon=count==5 || count==10;
  int clipped=route==0x26f8 || route==0x2702 || route==0x2847 || route==0x2a42 || route==0x278e || route==0x2931;
  /* Unclipped sprite routes publish only the low byte of Y. */
  int y=a[13];
  if(clipped){
   /* These original routines clip against bit 8 of the actual 16-bit Y.
    * Actor byte 0 is also an effect countdown, so it cannot identify signed
    * Y. Map the producer's 9-bit position to the nearby visible window;
    * retain enough of the lower edge for partially visible large sprites. */
   y=word(a+13)&511;
   if(y>=512-(int)width*8)y-=512;
  }
  for(unsigned n=0;n<(unsigned)count;++n){unsigned at=first+n*32,is_icon=icon && !n;
   v->objects[at+18]=(uint8_t)(0x80|(is_icon?1:width));v->objects[at+19]=(uint8_t)(is_icon?0:n-icon);
   put(v->objects+at+20,word(a+10)-(is_icon?8:width*8));put(v->objects+at+22,(unsigned)(y-(int)(is_icon?8:width*8)));
  }
 }
 return 0;
}
static int emit(view *v,unsigned command,unsigned parameter)
{
 if(v->s->queue_length>62)return -104;
 unsigned at=v->s->queue_length;v->s->queue[at]=(uint8_t)command;
 v->s->queue[at+1]=(uint8_t)parameter;v->s->queue_length+=2;return 0;
}
static void popped(uint8_t *p)
{
 unsigned n=p[0x58]+1,adjust=0;
 if((p[0x58]&15)==15 || (n&15)>9)adjust=6;
 if(n>0x99)adjust+=0x60;
 p[0x58]=(uint8_t)(n+adjust);
}
static int score(view *v,uint8_t *a)
{
 unsigned type=a[15],combo=a[29],owner=a[24];
 if(type>36 || (type&1) || combo>6 || (combo&1) || (owner!=0 && owner!=2))return -105;
 if(combo)for(unsigned i=0;i<8;++i){uint8_t *f=v->s->scores+i*16;if(f[0])continue;
  f[0]=2;f[15]=v->bank[0x172e +((type*4+combo)>>1)];
  f[10]=a[10];f[11]=a[11];f[13]=a[13];f[1]=a[24];break;
 }
 unsigned at=0x1696+type*4+combo;
 if(emit(v,v->bank[at+1]+owner,v->bank[at]))return -104;
 popped(v->p[owner/2]);return 0;
}
static void effect_spawn(view *v,const uint8_t *a)
{
 unsigned type=a[15],first=type<20?0:type<32?8:type<36?12:14;
 unsigned effect=type==16?8:type==28?10:type<20?0:type<32?2:type<36?4:6;
 for(unsigned i=first;i<16;++i){uint8_t *f=v->s->fx+i*16;if(f[0])continue;
  /*94E9 preserves the free slot's other bytes, including itsAA3F handler.*/
  f[0]=2;f[15]=(uint8_t)effect;f[10]=a[10];f[11]=a[11];f[13]=a[13];f[14]=a[14];break;
 }
}
static int effects(view *v,unsigned first,unsigned end)
{
 for(unsigned i=first;i<end;++i){uint8_t *f=v->s->fx+i*16;
  if(!f[0])continue;
  unsigned pc=word(f+2);uint8_t a[32]={0};memcpy(a,f,16);
  if(v->advance){
   if(pc==0xaa3f){
    if(a[15]>30 || (a[15]&1))return -106;
    put(a+2,0xaa59);a[0]=12;unsigned anim=bw(v->bank,0xaa6a+a[15]);
    if(anim<0x8000 || anim>0xbffb)return -106;
    put(a+6,anim);a[8]=v->bank[anim-0x8000]&127;
    v->s->sounds[6]=0x12;v->s->sounds[7]=0x10;memcpy(f,a,16);continue;
   }
   if(pc!=0xaa59)return -106;
   if(!--a[0]){put(a+2,0xaa3f);memcpy(f,a,16);continue;}
   if(animate(a,v->bank))return -102;
  }else if(pc==0xaa3f)continue;
  if(draw(v,a,0))return -103;
  if(v->advance)memcpy(f,a,16);
 }
 return 0;
}
static void floating(view *v,const uint8_t *a,unsigned kind,unsigned owner);
static int panic_special(view *v,uint8_t *a,unsigned pc)
{
 if(pc==0xa80b){
  v->s->sounds[4]=0x19;v->s->sounds[5]=0x20;v->panic->scheduler[27]=0;
  a[0]=a[1]=a[27]=a[22]=a[21]=0;
  if(a[5]==2){v->s->stop[17]=v->s->stop[0]=8;put(v->s->stop+2,0x9380);v->g[0x2b]=1;v->g[0x2c]=0;}
  else if(a[5]==4){v->panic->scheduler[27]=1;a[0]=2;a[29]=0;a[19]=10;a[18]=5;put(a+2,0xa8d7);}
  else if(a[5])return -115;
  unsigned first=a[15]==42?12:8,kind=(a[15]==42?26:20)+a[5];
  for(unsigned i=first;i<16;++i){uint8_t *f=v->s->fx+i*16;if(f[0])continue;f[0]=2;f[15]=(uint8_t)kind;f[10]=a[10];f[11]=a[11];f[13]=a[13];break;}
  if(a[5]!=2){unsigned bonus=bw(v->bank,0xa97b+a[29]);if(emit(v,10+a[24],bonus&255))return -104;floating(v,a,bonus>>8,a[24]);}
  return 0;
 }
 if(--a[19])return 0;
 if(pc==0xa8d7){
  a[19]=1;put(a+2,0xa91b);unsigned count=0;
  for(unsigned i=0;i<32;++i)if(v->balls[i*32]==1 || v->balls[i*32]==3)count+=2;
  if(v->s->direct_length>=8 || (a[24]!=0 && a[24]!=2))return -113;
  unsigned bcd=((count/10)<<4)|(count%10),at=v->s->direct_length++*3;
  v->s->direct[at]=a[24];put(v->s->direct+at+1,bcd);return 0;
 }
 for(unsigned i=0;i<32;++i){uint8_t *b=v->balls+i*32;if(b[0]!=1 && b[0]!=3)continue;
  put(b+2,b[0]==1?0x9150:0x93eb);b[0]=2;b[27]=b[26]=0;b[24]=a[24];
 }
 a[19]=30;
 if(!--a[18]){a[0]=0;for(unsigned i=0;i<2;++i)if(v->p[i][0]){v->p[i][0]=1;v->panic->scheduler[27]=0;v->panic->scheduler[30]=v->panic->scheduler[31]=1;}}
 return 0;
}
static void tiny_fade(view *v,uint8_t *a)
{
 unsigned anim=a[15]==4?0x978e:0x977a;
 a[0]=2;put(a+6,anim);a[8]=v->bank[anim-0x8000]&127;put(a+2,0x91ec);
 --v->g[0x20];a[19]=12;
 if(a[26]){v->s->stop[17]=v->s->stop[0]=8;put(v->s->stop+2,0x937d);v->g[0x2b]=1;v->g[0x2c]=0;a[26]=0;}
 v->s->sounds[6]=0x12;v->s->sounds[7]=0x10;
}
static int panic_incoming_retire(view *v,uint8_t *a)
{
 if(spang_panic_progress(v->panic,v->g,v->fixed)<0)return -115;
 a[0]=a[1]=a[22]=0;
 if(a[15]<8){tiny_fade(v,a);return 0;}
 unsigned type=a[15],first=(type<20)?0:(type<32 || type==38 || type==40)?8:type==36?14:12;
 unsigned kind=type==16?8:type==28?18:type<20?0:(type<32 || type==38 || type==40)?12:type==36?16:14;
 for(unsigned i=first;i<16;++i){uint8_t *f=v->s->fx+i*16;if(f[0])continue;
  f[0]=2;f[15]=(uint8_t)kind;f[10]=a[10];f[11]=a[11];f[13]=a[13];f[14]=a[14];break;
 }
 return 0;
}
static int ball(view *v,unsigned slot)
{
 uint8_t *a=v->balls+slot*32;if(!a[0])return 0;
 /* FIXED09EC..0C81: frozen drawing owns visibility byte1, not active byte0. */
 if(!v->advance && a[1]!=1)return 0;
 unsigned pc=word(a+2);int retire=pc==0x9155 || pc==0x915d || (v->panic && pc==0x9150);
 int moving=pc!=0x82b0 && pc!=0x90c1 && pc!=0x912e && pc!=0x913f && !retire && pc!=0x91ec;
 if(v->advance){
  v->g[0x21]=(uint8_t)(v->g[0x21]+a[22]); /*097E, before this handler.*/
  if(v->panic){
   if(pc==0xa785)return spang_panic_enter(a,v->bank,v->g[0x5b]);
   if(pc==0xa7c3){if(spang_panic_fall(a,v->g[0x5b],v->g[0x2b])<0)return -115;return v->g[0x2b] && v->g[0x2c]?0:draw(v,a,0);}
   if(pc==0xa80b || pc==0xa8d7 || pc==0xa91b)return panic_special(v,a,pc);
   if(pc==0x93eb)return panic_incoming_retire(v,a);
  }
  if(pc==0x82b0){if(spang_round_ball_init(a,v->g[0x1a],v->bank))return -107;}
  else if(pc==0x90c1){put(a+2,0x9155);if(v->panic && spang_panic_progress(v->panic,v->g,v->fixed)<0)return -115;if(score(v,a))return -105;}
  else if(pc==0x912e || pc==0x913f){
   if(a[28]!=1)(void)spang_items_drop_rng(v->items,v->g,a,v->bank,&v->s->seed,1);
   put(a+2,0x915d);if(pc==0x912e)popped(v->p[a[24]?1:0]);
  }else if(retire){
   if(pc==0x9150){if(spang_panic_progress(v->panic,v->g,v->fixed)<0)return -115;}
   else{
    if(pc==0x9155 && a[28]!=1)(void)spang_items_drop_rng(v->items,v->g,a,v->bank,&v->s->seed,0);
    if(v->g[0x40] && !--v->g[0x40])v->s->lucky[0]=2;
   }
  }else if(pc==0x91ec){if(animate(a,v->bank))return -102;}
  else if(!v->g[0x2b]){
   unsigned old_mode=a[4];
   if(spang_round_ball_update_full(a,(uint16_t)word(v->g+0x22),(uint16_t)word(v->g+0x24),0,read_bus,v,v->g+0x2a,v->g+0x26))return -108;
   if(v->panic && a[15]>=38 && old_mode!=a[4]){v->s->sounds[2]=0x1a;v->s->sounds[3]=0x40;}
  }
 }
 if(!(moving && v->g[0x2b] && v->g[0x2c]))if(draw(v,a,0))return -103;
 if(v->advance && pc==0x91ec){if(!--a[19])a[0]=0;}
 if(v->advance && retire){
  a[0]=a[1]=a[22]=0;
  if(a[15]<8)tiny_fade(v,a);
  else{
   effect_spawn(v,a);
   uint8_t random=(a[15]==22 || a[15]==26 || a[15]==34)?spang_items_random(&v->s->seed):0;
   if(spang_round_ball_split_ex(a,v->balls,v->bank,random,v->g+0x2a)<0)return -109;
  }
 }
 return v->fault;
}
static void floating(view *v,const uint8_t *a,unsigned kind,unsigned owner)
{
 for(unsigned i=0;i<8;++i){uint8_t *f=v->s->scores+i*16;if(f[0])continue;
  f[0]=2;f[15]=(uint8_t)kind;f[10]=a[10];f[11]=a[11];f[13]=a[13];f[1]=(uint8_t)owner;break;
 }
}
static void life_hud_scratch(view *v,unsigned owner)
{put(v->g+0x26,v->p[owner!=0][17]?0x724:0x6e4);}
static int lucky_floor(view *v,const uint8_t *a,unsigned *foot)
{
 *foot=(uint8_t)(a[13]+a[14]);
 return v->grid[((*foot>>3)*64+((word(a+10)&511)>>3))&2047]!=0;
}
static int lucky(view *v)
{
 for(unsigned i=0;i<2;++i){uint8_t a[32]={0};uint8_t *f=v->s->lucky+i*16;memcpy(a,f,16);if(!a[0])continue;
  unsigned pc=word(a+2),foot;int draw_now=0;
  if(!v->advance){if(pc!=0xabea && pc!=0xabd4 && pc!=0xabca && pc!=0xabcb && pc!=0xad0f && pc!=0xac97)if(draw(v,a,1))return -112;continue;}
  if(pc==0xad0f || ((pc==0xabea || pc==0xabd4) && v->g[pc==0xabea?0x47:0x48])){a[0]=0;put(a+2,0xabea);}
  else if(pc==0xabea || pc==0xabd4 || pc==0xabca || pc==0xabcb){
   unsigned stage=v->g[0x19];if(stage>=42 || (stage>=40 && pc==0xabea))return -112;
   if(pc==0xabea){++v->g[0x47];a[15]=(uint8_t)(stage*2);put(a+10,bw(v->bank,0xad66+stage*3));a[13]=v->bank[0x2d68+stage*3];}
   else{if(pc==0xabd4)++v->g[0x48];a[15]=v->bank[0x2d3c+stage];}
   if(a[15]>78 || (a[15]&1))return -112;
   a[0]=1;a[14]=v->bank[0x2dde +(a[15]>>1)];unsigned anim=bw(v->bank,0xae06+a[15]);
   if(anim<0x8000 || anim>0xbffb)return -112;
   put(a+6,anim);a[8]=v->bank[anim-0x8000]&127;put(a+2,(pc==0xabca || pc==0xabcb)?0xac5a:0xac29);
  }else if(pc==0xac29){
   put(a+12,word(a+12)+0x140);
   if(lucky_floor(v,a,&foot)){a[13]=(uint8_t)(foot-((foot&7)+a[14]));put(a+2,0xac5d);put(a+4,0x258);}
   draw_now=1;
  }else if(pc==0xac5a)draw_now=1;
  else if(pc==0xac5d || pc==0xac7c){
   draw_now=pc==0xac5d || !(v->frame&4);
   if(!lucky_floor(v,a,&foot))put(a+2,0xac29);
   else if(!--a[4]){
    if(pc==0xac5d){if(!a[5]--){a[4]=0xb4;put(a+2,0xac7c);}}
    else{a[0]=2;put(a+2,0xad0f);}
   }
  }else if(pc==0xac97){
   unsigned kind=a[15],owner=a[1];if(kind>78 || (kind&1) || (owner!=0 && owner!=2))return -112;
   put(a+2,0xad0f);
   if(kind==78){uint8_t *p=v->p[owner/2];unsigned n=p[0x60]+1,adjust=0;if((p[0x60]&15)==15 || (n&15)>9)adjust=6;if(n>0x99)adjust+=0x60;p[0x60]=(uint8_t)(n+adjust);life_hud_scratch(v,owner);}
   if(emit(v,10+owner,v->bank[0x2eec+kind]))return -104;
   floating(v,a,v->bank[0x2eed+kind],owner);
  }else return -112;
  if(draw_now && draw(v,a,1))return -103;
  memcpy(f,a,16);
 }
 return 0;
}
static int items(view *v)
{
 for(unsigned i=0;i<8;++i){uint8_t *a=v->items+i*32;
  if(!a[0])continue;
  if(!v->advance){if(a[0]==1 && word(a+2)!=0xb069 && draw(v,a,1))return -103;continue;}
  int event=spang_items_tick(a,v->g,v->grid,v->balls,v->bank,v->frame);
  if(event<0)return -110;
  v->s->events[i]=(uint8_t)event;
  if(event&SPANG_ITEM_SPECIAL){unsigned count=0;
   for(unsigned n=0;n<32;++n)if(v->balls[n*32]==1 && v->balls[n*32+15]<8)++count;
   if(count>=a[28]){if(v->s->direct_length>=8 || (a[24]!=0 && a[24]!=2))return -113;
    unsigned at=v->s->direct_length++*3,bcd=spang_items_special_bonus(a,v->balls,v->bank);
    v->s->direct[at]=a[24];put(v->s->direct+at+1,bcd);
   }
  }
  if(event&SPANG_ITEM_SCORE){
   unsigned param=a[15]<8?v->bank[0x3180+a[29]]:2;
   if(emit(v,10+a[24],param))return -104;
  }
  if(event&SPANG_ITEM_SLOWDOWN){if(!v->s->slow[0])v->s->slow[0]=v->g[0x22];put(v->s->slow+2,0x956b);}
  if(event&SPANG_ITEM_LIFE){floating(v,a,0x5e,a[24]);life_hud_scratch(v,a[24]);}
  if(event&SPANG_ITEM_TIMESTOP){v->s->stop[0]=v->s->stop[17]=8;put(v->s->stop+2,0x9384);}
  if((event&SPANG_ITEM_DRAW) && draw(v,a,1))return -103;
 }
 return 0;
}
void spang_round_frame_init(spang_round_frame *s,uint16_t seed)
{
 if(!s)return;
 memset(s,0,sizeof *s);s->seed=seed;
 for(unsigned i=0;i<16;++i)put(s->fx+i*16+2,0xaa3f);
 for(unsigned i=0;i<8;++i)put(s->scores+i*16+2,0xa102);
 put(s->lucky+2,0xabea);put(s->lucky+18,0xabd4);put(s->slow+2,0x956b);
}
static int frame_step(spang_round_frame *s,uint8_t balls[1024],
 uint8_t item_pool[256],uint8_t g[128],uint8_t p1[128],uint8_t p2[128],
 const uint8_t bank[16384],const uint8_t grid[2048],uint8_t objects[4096],
 uint16_t *cursor,uint8_t *remaining,uint8_t frame,int advance,
 spang_panic *panic,const uint8_t *fixed)
{
 if(!s || !balls || !item_pool || !g || !p1 || !p2 || !bank || !grid || !objects || !cursor || !remaining)return -100;
 if(g[0x1a]!=(panic?2:0) || (*cursor&31) || *cursor>0xfe0)return -100;
 view v={s,balls,item_pool,g,{p1,p2},objects,bank,grid,cursor,remaining,frame,advance,0,panic,fixed};
 if(advance){g[0x21]=0;s->queue_length=0;s->direct_length=0;memset(s->queue,255,64);memset(s->events,0,8);}
 int result=effects(&v,0,8);if(result)return result;
 for(unsigned i=0;i<16;++i){result=ball(&v,i);if(result)return result;}
 result=effects(&v,8,12);if(result)return result;
 for(unsigned i=16;i<24;++i){result=ball(&v,i);if(result)return result;}
 result=effects(&v,12,16);if(result)return result;
 result=lucky(&v);if(result)return result;
 result=items(&v);if(result)return result;
 for(unsigned i=24;i<32;++i){result=ball(&v,i);if(result)return result;}
 return v.fault;
}
int spang_round_frame_step(spang_round_frame *s,uint8_t balls[1024],
 uint8_t items[256],uint8_t g[128],uint8_t p1[128],uint8_t p2[128],
 const uint8_t b[16384],const uint8_t grid[2048],uint8_t objects[4096],
 uint16_t *cursor,uint8_t *remaining,uint8_t frame,int advance)
{return frame_step(s,balls,items,g,p1,p2,b,grid,objects,cursor,remaining,frame,advance,0,0);}

int spang_round_lucky_contact(spang_round_frame *s,const uint8_t p[128],uint8_t frame)
{
 if(!s || !p || p[17]>1)return -114;
 unsigned slot=(frame+p[17]*2)&3;if(slot>=2)return 0;
 uint8_t *a=s->lucky+(slot?0:16);if(a[0]!=1)return 0;
 unsigned sum=word(a+10)+17,dx=((sum&65535)-word(p+10)-(sum>>16))&65535;
 if(dx>=35 || (uint8_t)(p[13]+17-a[13])>=35)return 0;
 a[1]=(uint8_t)(p[17]*2);a[0]=2;put(a+2,0xac97);return 1;
}
int spang_round_frame_panic_step(spang_round_frame *s,uint8_t balls[1024],
 uint8_t items[256],uint8_t g[128],uint8_t p1[128],uint8_t p2[128],
 const uint8_t b[16384],const uint8_t grid[2048],uint8_t objects[4096],
 uint16_t *cursor,uint8_t *remaining,uint8_t frame,int advance,
 spang_panic *panic,const uint8_t fixed[32768])
{
 if(!panic || !fixed)return -100;
 return frame_step(s,balls,items,g,p1,p2,b,grid,objects,cursor,remaining,frame,advance,panic,fixed);
}
