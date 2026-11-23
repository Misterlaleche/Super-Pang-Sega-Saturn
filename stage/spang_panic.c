#include "spang_panic.h"
#include "spang_items.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned bw(const uint8_t *b,unsigned a){return word(b+(a-0x8000));}
int spang_panic_hit(uint8_t a[32],uint8_t ball[32],uint8_t p1[128],uint8_t p2[128],uint8_t g[128],int bullet)
{
 if(!a || !ball || !p1 || !p2 || !g || (a[17]!=0 && a[17]!=2))return -150;
 uint8_t *p=a[17]?p2:p1;
 if(!bullet){put(g+0x6e,1800);put(p+0x3e,word(p+0x3e)+1);}
 put(a+2,bullet?0x86cd:0x8234);a[0]=2;
 /*901D consumes the projectile but leaves an entering balloon intact.*/
 if(ball[0]!=1)return 0;
 ball[0]=2;ball[27]=0;ball[24]=a[17];
 if(!ball[21]){
  put(ball+2,0x90c1);unsigned combo=p[0x59]==ball[28]?p[0x5a]+2:0;
  if(combo>6)combo=6;
  p[0x59]=ball[28];p[0x5a]=ball[29]=(uint8_t)combo;
 }else{
  put(ball+2,0xa80b);unsigned combo=g[0x66]==ball[5]?g[0x67]+2:0;
  if(combo>8)combo=8;
  g[0x66]=ball[5];g[0x67]=ball[29]=(uint8_t)combo;
  if(ball[5]==4){uint8_t *players[2]={p1,p2};for(unsigned i=0;i<2;++i)if(players[i][0]){players[i][0]=5;players[i][1]=1;players[i][35]=0;}}
 }
 return 0;
}
static int event(spang_panic *s,unsigned kind,unsigned command,unsigned parameter)
{
 if(s->event_count>=16)return -151;
 unsigned n=s->event_count++*3;s->events[n]=(uint8_t)kind;
 s->events[n+1]=(uint8_t)command;s->events[n+2]=(uint8_t)parameter;return 0;
}
void spang_panic_events_reset(spang_panic *s){if(s)s->event_count=0;}
static int speed(uint8_t *g,const uint8_t *f,unsigned value)
{
 if(value>40)return -150;
 g[0x22]=(uint8_t)value;put(g+0x24,word(f+0x2f65+2*value));
 memcpy(g+0x30,f+0x2fb7+8*value,8);return 0;
}
static void gauge(uint8_t *g,const uint8_t *f)
{unsigned n=word(f+0x33c8+2*g[0x55]);put(g+0x50,n);put(g+0x58,384-n);}
int spang_panic_level(uint8_t g[128],const uint8_t f[32768],unsigned difficulty)
{
 if(!g || !f || difficulty>3 || !g[0x53] || g[0x53]>99 || g[0x55]>96)return -150;
 unsigned level=g[0x53]<32?g[0x53]:31,rank=f[0x3348+4*level+difficulty];
 if(rank>15)return -150;
 g[0x54]=(uint8_t)rank;g[0x5a]=f[0x3328+2*rank];
 if(g[0x22]!=40){g[0x28]=f[0x3329+2*rank];if(speed(g,f,g[0x28]))return -150;}
 put(g+0x3a,0x9876+32*rank);
 /* FIXED3279's preceding ADD is discarded by327A. */
 put(g+0x38,0x9a76);gauge(g,f);return 0;
}
int spang_panic_progress(spang_panic *s,uint8_t g[128],const uint8_t f[32768])
{
 if(!s || !g || !f)return -150;
 if(g[0x53]>=100){put(s->scheduler+2,0x95f6);g[0x56]=255;return 2;}
 if(s->difficulty>3 || !g[0x53] || g[0x55]>96)return -150;
 unsigned n=g[0x55]+6;
 if(n<97){g[0x55]=(uint8_t)n;gauge(g,f);return 0;}
 if(++g[0x53]==100)return 2;
 if(!(g[0x53]&3))g[0x56]=f[0x348a+(g[0x53]>>2)];
 g[0x55]=0;return spang_panic_level(g,f,s->difficulty)<0?-150:1;
}
int spang_panic_enter(uint8_t a[32],const uint8_t b[16384],uint8_t delay)
{
 if(!a || !b || a[15]>42 || (a[15]&1))return -150;
 unsigned anim=bw(b,0xa985+a[15]);if(anim<0x8000 || anim>0xbffb)return -150;
 a[0]=3;a[1]=1;a[4]=0;a[5]=2;a[19]=delay;put(a+2,0xa7c3);
 a[27]=(uint8_t)(word(a+10)>>1);a[28]=b[0x315+a[15]];a[22]=b[0x316+a[15]];
 put(a+6,anim);a[8]=b[anim-0x8000]&127;return 0;
}
int spang_panic_fall(uint8_t a[32],uint8_t delay,uint8_t blocked)
{
 if(!a)return -150;
 if(blocked)return 0;
 if(delay<a[19])a[19]=delay;
 if(--a[19])return 0;
 a[19]=delay;put(a+13,word(a+13)+8);
 if(--a[23])return 0;
 a[30]=a[31]=0;a[14]=a[13];put(a+2,0x82b0);return 1;
}
int spang_panic_services(spang_panic *s,uint8_t g[128],const uint8_t p1[128],const uint8_t p2[128],const uint8_t f[32768])
{
 if(!s || !g || !p1 || !p2 || !f)return -150;
 unsigned pc=word(g+0x60),music=0;
 if(pc==0x9e10){put(g+0x60,g[0x0a]?0x9e28:0x9e4f);if(g[0x0a])music=0x38;}
 else if(pc==0x9e28){if(g[0x21]>=35){put(g+0x60,0x9e3f);put(g+0x62,600);music=0x39;}}
 else if(pc==0x9e3f || pc==0x9e67){unsigned timer=(word(g+0x62)-1)&65535;put(g+0x62,timer);if(!timer)put(g+0x60,pc==0x9e3f?0x9e50:0x9e28);}
 else if(pc==0x9e50){if(g[0x21]<31){put(g+0x60,0x9e67);put(g+0x62,600);music=0x38;}}
 else if(pc!=0x9e4f)return -150;
 if(music && event(s,SPANG_PANIC_FM,music,0))return -151;
 unsigned timer=(word(g+0x6e)-1)&65535;put(g+0x6e,timer);
 if(!timer){
  if(speed(g,f,40))return -150;
  if(event(s,SPANG_PANIC_PALETTE,13,20))return -151;
  if(p1[0x60] && event(s,SPANG_PANIC_QUEUE,10,0x5c))return -151;
  if(p2[0x60] && event(s,SPANG_PANIC_QUEUE,12,0x5c))return -151;
 }
 return 0;
}
int spang_panic_death_reset(uint8_t g[128],const uint8_t f[32768])
{if(!g || !f)return -150;put(g+0x6e,1800);return speed(g,f,g[0x28]);}
int spang_panic_restart(spang_panic *s,uint8_t g[128],const uint8_t f[32768])
{
 if(!s || !g || !f || !g[0x53] || g[0x53]>100)return -150;
 memset(s->scheduler,0,32);put(s->scheduler+2,0x95e2);s->event_count=0;
 g[0x56]=g[0x55]=g[0x67]=0;g[0x66]=255;gauge(g,f);
 if(g[0x53]==100)--g[0x53];
 put(g+0x6e,1800);return 0;
}
int spang_panic_init(spang_panic *s,uint8_t g[128],const uint8_t f[32768],unsigned difficulty)
{
 if(!s || !g || !f || difficulty>3)return -150;
 memset(s,0,sizeof *s);s->difficulty=(uint8_t)difficulty;
 g[0x1a]=2;g[0x53]=1;g[0x52]=g[0x54]=g[0x55]=g[0x56]=0;
 if(spang_panic_level(g,f,difficulty))return -150;
 return spang_panic_restart(s,g,f);
}
int spang_panic_spawn(spang_panic *s,uint8_t g[128],uint8_t pool[1024],
 const uint8_t b[16384],unsigned selector,uint16_t *seed)
{
 static const uint8_t types[14]={0,2,8,12,20,24,32,36,4,16,28,38,40,42};
 static const uint8_t starts[14]={0,0,16,16,24,24,28,30,0,16,24,24,24,28};
 if(!s || !g || !pool || !b || !seed || selector>26 || (selector&1))return -150;
 unsigned i=selector/2,slot=starts[i];while(slot<32 && pool[slot*32])++slot;
 if(slot==32)return -2;
 uint8_t *a=pool+slot*32;unsigned type=types[i];
 a[0]=3;a[18]=(uint8_t)(i>=8 && i<=10?4:2);a[26]=2;a[15]=(uint8_t)type;
 if(type>=38){a[21]=2;a[26]=0;}
 unsigned x=spang_items_random(seed)&31;
 put(a+10,bw(b,0x97f4+2*x));put(a+13,bw(b,0x9834+type));
 a[23]=b[0x1860+type/2];put(a+2,0xa785);
 if(g[0x4b]){
  if(event(s,SPANG_PANIC_FM,0x1a,0))return -151;
  if((g[0x4b]&1) && event(s,SPANG_PANIC_QUEUE,10,10))return -151;
  if((g[0x4b]&2) && event(s,SPANG_PANIC_QUEUE,12,10))return -151;
 }
 return (int)slot;
}
static int special(spang_panic *s,uint8_t *g,uint8_t *balls,const uint8_t *b,uint16_t *seed)
{
 uint8_t *a=s->scheduler;unsigned rank=g[0x54];if(rank>=16)return -150;
 if(!a[26]){
  unsigned timer=word(a+28);
  if(timer){put(a+28,timer-1);return 0;}
  if(g[0x21]<b[0x17e4+rank])return 0;
 }
 unsigned choice=b[0x17d4+(spang_items_random(seed)&15)];
 int slot=spang_panic_spawn(s,g,balls,b,choice,seed);
 if(slot==-2)return 0;
 if(slot<0)return slot;
 a[27]=1;
 if(a[26])a[26]=0;
 else put(a+28,bw(b,0x9c76+rank*2));
 return 0;
}
static void fast_drop(uint8_t *g,uint8_t *a,const uint8_t *p,uint8_t input)
{
 if(!p[0] || (input&0xf0)!=0x40)return;
 g[0x4b]|=(uint8_t)(p[17]+1);g[0x5b]=10;a[25]=150;
}
int spang_panic_schedule(spang_panic *s,uint8_t g[128],uint8_t balls[1024],
 const uint8_t p1[128],const uint8_t p2[128],uint8_t input1,uint8_t input2,
 const uint8_t bank2[16384],uint16_t *seed)
{
 if(!s || !g || !balls || !p1 || !p2 || !bank2 || !seed || g[0x1a]!=2)return -150;
 uint8_t *a=s->scheduler;unsigned pc=word(a+2);
 if(pc==0x95e2){put(a+2,0x960a);put(a+28,1200);a[30]=a[31]=1;return 0;}
 if(pc==0x95f6)return 0;
 if(pc!=0x960a)return -150;
 if(!a[27]){int r=special(s,g,balls,bank2,seed);if(r)return r;}
 g[0x4b]=0;
 if(a[25]){--a[25];return 0;}
 fast_drop(g,a,p1,input1);fast_drop(g,a,p2,input2);
 if(!g[0x4b]){
  g[0x5b]=g[0x5a];
  if(--a[30])return 0;
  a[30]=30;if(--a[31])return 0;
 }
 unsigned ptr=word(g+0x38);if(ptr<0x8000 || ptr>0xbfe0)return -150;
 a[31]=bank2[ptr-0x8000+(spang_items_random(seed)&31)];
 ptr=word(g+0x3a);if(ptr<0x8000 || ptr>0xbfe0)return -150;
 unsigned selector=bank2[ptr-0x8000+(spang_items_random(seed)&31)];
 int r=spang_panic_spawn(s,g,balls,bank2,selector,seed);return r>=0 || r==-2?0:r;
}
