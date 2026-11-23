#include "fauna_internal.h"
#include "spang_items.h"

void fauna_init(uint8_t s[32],uint8_t actors[320],uint8_t g[128])
{crocodile_init(s,actors,g);}

int fauna_frozen_snapshot(const uint8_t a[40],uint8_t draw[40])
{if(!a[1])return 0;memcpy(draw,a,40);return 1;}

int fauna_animate(uint8_t a[40],const uint8_t b[16384])
{
 if(--a[8])return 0;
 unsigned old=fa_word(a+6);if(old<0x8000 || old>0xbff9)return -701;
 unsigned next=old+5;if(b[old-0x8000]&128)next=fa_bw(b,next);
 if(next<0x8000 || next>0xbffb)return -701;
 fa_animation(a,b,next);return 0;
}
int fauna_queue(fauna_context *c,unsigned command,unsigned parameter)
{
 spang_round_frame *r=c->round;if(r->queue_length>62)return -702;
 r->queue[r->queue_length++]=(uint8_t)command;
 r->queue[r->queue_length++]=(uint8_t)parameter;return 0;
}
int fauna_score(fauna_context *c,const uint8_t a[40],unsigned type)
{
 for(unsigned i=0;i<8;++i){uint8_t *p=c->round->scores+i*16;if(p[0])continue;
  p[0]=2;p[10]=a[10];p[11]=a[11];p[13]=a[13];p[1]=a[24];p[15]=(uint8_t)type;return 1;
 }
 return 0;
}
/* FIXED1615. The free count is the original owner, not a second pool scan. */
static int allocate(fauna_context *c)
{
 if(!c->globals[0x1e])return -1;
 --c->globals[0x1e];
 for(unsigned i=0;i<8;++i)if(!c->actors[i*40])return (int)i;
 return -703;
}
static int secret(fauna_context *c,uint8_t previous_system)
{
 uint8_t *g=c->globals,*s=c->scheduler;
 unsigned time=fa_word(c->timer+1),input;
 if(g[0x5e]==2){if((uint16_t)(time-0x63)>=3)return 0;input=8;}
 else if(g[0x5e]==4){if((uint16_t)(time-0x72)>=2)return 0;input=0x48;}
 else if(g[0x5e]==6){
  /* BANK4:89B2 tests both FCC0/FCC2 FIRE bits at exactly BCD0081.
   * Single-player input2 is zero; no alternate trigger is invented. */
  if(time!=0x81 || !(c->input1 & c->input2 & 8))return 0;
  input=0;
 }else if(g[0x5e]==8){
  /* BANK4:89CC: exactly BCD0050 and previous system START1 bit. */
  if(time!=0x50 || !(previous_system&8))return 0;
  input=0;
 }else return -704;
 if(g[0x5e]!=6 && g[0x5e]!=8 && !((c->player1[0] && c->input1==input)||(c->player2[0] && c->input2==input)))return 0;
 int slot=allocate(c);if(slot==-1)return 0;if(slot<0)return slot;
 uint8_t *a=c->actors+slot*40;g[0x5f]=2;a[0]=2;a[17]=9;
 fa_put(s+2,s[7]?0x808e:0x805c);return 1;
}
static void bird_allocate(fauna_context *c,unsigned slot,unsigned parameter)
{
 uint8_t *a=c->actors+slot*40;
 a[0]=2;a[36]=a[37]=a[1]=a[26]=0;a[17]=6;a[15]=(uint8_t)parameter;
}
static int flock(fauna_context *c,unsigned parameter)
{
 uint8_t *g=c->globals;
 if(g[0x21]>=28 || fa_word(c->timer+1)<0x10)return 0;
 int eligible=0;for(unsigned i=0;i<32;++i)if(c->balls[i*32] && c->balls[i*32+15]){eligible=1;break;}
 if(!eligible || !(parameter&2?c->player2[0]:c->player1[0]) || g[0x1e]<4)return 0;
 ++c->scheduler[28];int slot=allocate(c);if(slot<0)return -703;
 unsigned leader_slot=(unsigned)slot,leader_address=0xfa00+leader_slot*40;
 bird_allocate(c,leader_slot,parameter);uint8_t *lead=c->actors+leader_slot*40;
 lead[36]=lead[19]=lead[37]=1;lead[34]=0;
 lead[39]=(uint8_t)((spang_items_random(&c->round->seed)&15)>>2);
 if(!lead[39])lead[26]=2;
 for(unsigned n=1;n<4;++n){
  slot=allocate(c);if(slot<0)return -703;
  bird_allocate(c,(unsigned)slot,parameter);uint8_t *a=c->actors+slot*40;
  fa_put(lead+25+n*2,0xfa00+(unsigned)slot*40);fa_put(a+27,leader_address);
  a[19]=(uint8_t)(1+n*10);a[29]=(uint8_t)(25+n*2);a[34]=(uint8_t)n;
  if(lead[39]==n)a[26]=2;
 }
 lead[33]=4;return 4;
}
int fauna_schedule_system(fauna_context *c,uint8_t previous_system)
{
 uint8_t *s=c->scheduler,*g=c->globals;const uint8_t *b=c->bank4;
 unsigned pc=fa_word(s+2),stage=g[0x19],rank=g[0x52];
 if(stage>=40 || rank>=32)return -705;
 if(pc==0x8008){
  fa_put(s+8,fa_bw(b,0x826a+b[0x242+stage]));fa_put(s+18,fa_bw(b,0x88d6+rank*2));
  fa_put(s+30,0x978d+stage*16);s[29]=0;g[0x5e]=b[0x936+stage];
  fa_put(s+2,g[0x5e] && !g[0x5f]?0x8059:0x805c);return 0;
 }
 if(pc==0x80f5)return 0;
 if(pc==0x8059 || pc==0x808b){int r=secret(c,previous_system);if(r<0)return r;pc=pc==0x8059?0x805c:0x808e;}
 if(pc==0x805c){
  if(g[0x2b])return 0;
  unsigned old=fa_word(s+18);fa_put(s+18,old-1);if(old)return 0;
  unsigned ptr=fa_word(s+8);if(ptr<0x8000 || ptr>0xbffa)return -706;
  fa_put(s+18,fa_bw(b,ptr));s[7]=2;
  fa_put(s+2,g[0x5e] && !g[0x5f]?0x808b:0x808e);return 0;
 }
 if(pc!=0x808e)return -707;
 unsigned delay=(fa_word(s+18)-1)&65535;fa_put(s+18,delay);if(delay)return 0;
 unsigned ptr=fa_word(s+8);if(ptr<0x8000 || ptr>0xbff8)return -706;
 fa_put(s+8,ptr+6);unsigned next=fa_bw(b,ptr+6);fa_put(s+18,next);
 if(!next)fa_put(s+2,0x80f5);
 if(s[28]>=b[0x916+rank])return 0;
 unsigned kind=b[ptr+2-0x8000];
 if(kind==4)return flock(c,b[ptr+3-0x8000]);
 if(kind==0){if(s[29]>=2 || fa_word(c->timer+1)<0x20)return 0;}
 else if(kind==2){if(g[0x21]<5)return 0;}
 else return -708;
 int slot=allocate(c);if(slot==-1)return 0;if(slot<0)return slot;
 uint8_t *a=c->actors+slot*40;++s[28];a[0]=2;
 if(!kind){++s[29];a[17]=a[15]=0;return 1;}
 a[17]=3;a[15]=b[ptr+3-0x8000];
 if(!a[15]){fa_put(a+10,fa_bw(b,ptr+4));fa_put(a+13,0xfff8);}
 else fa_put(a+13,fa_bw(b,ptr+4));
 return 1;
}

int fauna_sprite(const uint8_t a[40],const uint8_t b[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *room)
{
 unsigned ptr=fa_word(a+6);if(ptr<0x8000 || ptr>0xbffb)return -701;
 unsigned command=fa_bw(b,ptr+1),cols,rows;
 if(command==0x2112){cols=rows=1;}
 else if(command==0x21ad || command==0x2702){cols=rows=2;}
 else if(command==0x246f){cols=3;rows=2;}
 else if(command==0x2635){cols=4;rows=2;}
 else return -709;
 unsigned count=cols*rows;if(*room<count)return 0;
 if((*cursor&31) || *cursor+count*32>0xfe0)return -710;
 unsigned x=(fa_word(a+10)-cols*8)&65535;
 uint8_t code=b[ptr+3-0x8000],attr=b[ptr+4-0x8000];
 for(unsigned col=0;col<cols;++col){
  unsigned xx=(x+col*16)&65535;attr|=(xx>>4)&16;
  for(unsigned n=0;n<rows;++n){unsigned row=(col&1)?rows-1-n:n,at=*cursor+(col*rows+n)*32;
   uint16_t yy=(uint16_t)(fa_word(a+13)-8+row*16);
   objects[at]=(uint8_t)(code+col+row*8);objects[at+1]=attr;
   objects[at+2]=command==0x2702?(yy&0x100?248:(uint8_t)(yy-8)):(uint8_t)(a[13]-rows*8+row*16);
   objects[at+3]=(uint8_t)xx;
  }
 }
 *cursor+=(uint16_t)(count*32);*room-=(uint8_t)count;return (int)count;
}
static void weapon_hit(uint8_t *a,uint8_t *weapon,unsigned retire)
{
 a[24]=weapon[17];
 if(a[17]!=6){weapon[0]=2;fa_put(weapon+2,retire);}
 unsigned damage=a[17]==0?5:1;a[25]=(uint8_t)(a[25]>=damage?a[25]-damage:0);
}
int fauna_rope_contact(uint8_t actors[320],uint8_t w[32],uint8_t frame)
{
 for(unsigned slot=frame&3;slot<8;slot+=4){uint8_t *a=actors+slot*40;if(a[0]!=1)continue;
  if((uint8_t)((fa_word(a+10)>>1)+7-w[27])>=15 || (uint8_t)(a[13]+12)<w[13] || (uint8_t)(a[13]-13)>=w[14])continue;
  weapon_hit(a,w,0x8234);return 1;
 }
 return 0;
}
int fauna_bullet_contact(uint8_t actors[320],uint8_t w[24],uint8_t frame)
{
 for(unsigned slot=frame&3;slot<8;slot+=4){uint8_t *a=actors+slot*40;if(a[0]!=1)continue;
  if((uint8_t)((fa_word(a+10)>>1)+7-w[5])>=15 || (uint8_t)(a[13]+12-w[13])>=25)continue;
  weapon_hit(a,w,0x86cd);return 1;
 }
 return 0;
}
void fauna_player_contact(fauna_context *c,uint8_t p[128],uint8_t frame)
{
 for(unsigned slot=frame&3;slot<8;slot+=4){uint8_t *a=c->actors+slot*40;
  if(a[17]==3){
   if(a[0]!=3)continue;
   unsigned dx=(uint16_t)(fa_word(a+10)+17-fa_word(p+10));
   if(dx>=35 || (uint8_t)(p[13]+17-a[13])>=35)continue;
   a[18]=dx>=17?0:2;a[0]=4;a[3]=12;a[4]=a[5]=0;a[24]=(uint8_t)(p[17]*2);continue;
  }
  if((a[17]!=0 && a[17]!=6) || c->globals[0x2b] || p[0]!=1 || a[0]!=1 || (!a[17] && a[5]))continue;
  if((uint16_t)(fa_word(a+10)+17-fa_word(p+10))>=35 || (uint8_t)(p[13]+17-a[13])>=35)continue;
  if(!a[17]){a[24]=(uint8_t)(p[17]*2);a[25]=(uint8_t)(a[25]>=5?a[25]-5:0);}else a[25]=0;
  p[0]=2;p[35]=1;p[36]=180;if(c->sound)c->sound(c->sound_context,0x12);
 }
}
void fauna_protection_tick(uint8_t p[128],uint8_t frame)
{(void)frame;if(p[0]==2 && !--p[36]){p[0]=p[1]=1;p[35]=0;}}

int fauna_schedule(fauna_context *c){return fauna_schedule_system(c,0);}
