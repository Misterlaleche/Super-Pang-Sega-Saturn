#include "spang_weapons.h"
#include "spang_harpoon.h"
#include "spang_geometry.h"
#include "spang_panic.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void state(uint8_t *a,unsigned n){put(a+2,n);}
static int animation(uint8_t *a,const uint8_t *b,unsigned n,int rope)
{if(n<0x8000 || n+4>=0xc000)return -1;put(a+6,n);a[rope?19:8]=b[n-0x8000]&127;if(rope)a[8]=b[n+1-0x8000];return 0;}
void spang_weapons_init(spang_weapons *w){memset(w,0,sizeof *w);}
int spang_weapons_fire(spang_weapons *w,uint8_t p[128],const uint8_t b[16384])
{
 unsigned type=p[0x57]&7,owner=p[0x11]*2;
 if(p[0x11]>1 || p[0x57]>14 || (p[0x57]>>3)!=p[0x11] || (type&1))return -1;
 if(type==6){
  unsigned count=0;
  for(unsigned n=0;n<3;++n){unsigned slot=0;while(slot<12 && w->bullets[slot][0])++slot;if(slot==12)break;
   uint8_t *a=w->bullets[slot];memset(a,0,24);a[0]=2;a[15]=(uint8_t)(n*2+p[0x11]*6);a[17]=(uint8_t)owner;
   put(a+10,word(p+10)+n*8-8);a[13]=(uint8_t)(p[13]-12);state(a,0x8635);++count;
  }
  if(count)p[0x55]=4;
  return (int)count;
 }
 unsigned used=(w->harpoons[0][0]!=0)+(w->harpoons[1][0]!=0);
 if(used==2 || (type!=2 && used))return 0;
 uint8_t *a=w->harpoons[w->harpoons[0][0]?1:0];memset(a,0,32);a[0]=2;a[15]=type==4?2:0;a[17]=(uint8_t)owner;
 put(a+10,word(p+10));a[13]=(uint8_t)(p[13]-16);state(a,0x8055);
 (void)b;return 1;
}
static int hit(uint8_t *a,uint8_t *ball,uint8_t *p,int bullet,uint8_t *g,uint8_t *p2)
{
 if(g)return spang_panic_hit(a,ball,p,p2,g,bullet);
 if(ball[21])return -1;
 if(!bullet){if(spang_harpoon_hit(a,ball,p)<0)return -1;return 0;}
 /* 9007 bypasses the harpoon accuracy increment at 8FE9. */
 a[0]=2;state(a,0x86cd);ball[0]=2;ball[27]=0;state(ball,0x90c1);ball[24]=a[17];
 unsigned combo=p[0x59]==ball[28]?p[0x5a]+2:0;if(combo>6)combo=6;
 p[0x59]=ball[28];p[0x5a]=ball[29]=(uint8_t)combo;return 0;
}
static int rope_break(uint8_t *a,uint8_t *grid,spang_weapon_tile_fn cb,void *ctx)
{
 unsigned address=word(a+29);int vertical=(a[31]&64)!=0;
 if(!vertical && a[31]<128)address-=a[31]&7;
 if(address<0xf000 || address>=0xf800)return -1;
 unsigned cell=grid[address-0xf000],count=cell&7;
 if(cell && !count)return -1;
 for(unsigned n=0;n<count;++n){unsigned at=vertical?address-64*n:(address&0xff00)|((address+n)&255);if(at<0xf000 || at>=0xf800)return -1;}
 put(a+29,address);a[31]=(uint8_t)cell;state(a,0x8234);
 for(unsigned n=0;n<count;++n){unsigned at=(vertical?address-64*n:(address&0xff00)|((address+n)&255))-0xf000;grid[at]=0;}
 if(count && cb)cb(ctx,a,(int)count);
 return 0;
}
static void lucky_contact(uint8_t *w,uint8_t *pool,uint8_t frame,int bullet)
{
 /* BANK2:89AB..8A29 / 8E13..8E8B. This pool contains fruit/lucky
  * rewards; F900 power-ups retain their original player-body contact. */
 unsigned slot=frame&3;if(!pool || slot>=2)return;
 uint8_t *a=pool+(slot?0:16);if(a[0]!=1)return;
 unsigned radius=bullet?11:4,sum=word(a+10)+radius;
 unsigned dx=((sum&65535)-word(w+10)-(sum>>16))&65535;
 if(dx>=radius*2+1)return;
 if(bullet){if((uint8_t)(a[13]+11-w[13])>=23)return;}
 else if((uint8_t)(a[13]+7)<w[13] || (uint8_t)(a[13]-8)>=w[14])return;
 w[0]=2;state(w,bullet?0x86cd:0x8234);
 a[0]=2;a[1]=w[17];state(a,0xac97);
 if(a[15]>=2)a[15]-=2;
}
static int weapons_step(spang_weapons *w,uint8_t frame,int edge,int advance,const uint8_t b[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t p[128],spang_weapon_tile_fn cb,void *ctx,uint8_t *g,uint8_t *p2,unsigned pass,uint8_t *lucky,uint8_t contact_frame,spang_weapon_contact_fn contact,spang_weapon_sound_fn sound)
{
 if(!advance)return 0;
 if(pass&1)w->harpoon_visible=0;
 if(pass&2)w->bullet_visible=0;
 if(pass&1)for(unsigned i=0;i<2;++i){uint8_t *a=w->harpoons[i];if(!a[0])continue;unsigned s=word(a+2);int draw=0,collide=0;
  if(s==0x8055){if(spang_harpoon_init(a,b))return -1;uint8_t *owner=g && a[17]?p2:p;put(owner+0x3c,word(owner+0x3c)+1);}
  else if(s==0x80cc || s==0x8152){if(spang_harpoon_flight_draw(a,frame,b,grid,w->harpoon_draw[i]))return -2;draw=collide=1;}
  else if(s==0x8176 || s==0x818f || s==0x81d5){
   if(s==0x8176){state(a,0x818f);a[28]=100;if(animation(a,b,0x8380,1))return -3;}
   memcpy(w->harpoon_draw[i],a,32);draw=collide=1;
  }else if(s==0x81e8){if(rope_break(a,grid,cb,ctx))return -4;}
  else if(s==0x8234){a[0]=0;state(a,0x8055);}
  else return -5;
  if(draw)w->harpoon_visible|=(uint8_t)(1u<<i);
  if(collide){int struck=0;
   for(unsigned n=0;n<32;++n){uint8_t *ball=balls+n*32;if((g || ball[0]==1) && ball[27] && spang_harpoon_overlap(ball[28],a[27],ball[27],a[14],a[13],ball[13])){if(hit(a,ball,p,0,g,p2))return -6;struck=1;break;}}
   if(!struck){lucky_contact(a,lucky,contact_frame,0);if(contact)struck=contact(ctx,a,0,contact_frame);}
   if(!struck && (s==0x8176 || s==0x818f)){
    if((edge && a[28]<=35) || (edge && !(a[28]=(uint8_t)(a[28]-35))) || !--a[28]){
     a[28]=30;if(animation(a,b,0x8388,1))return -7;state(a,0x81d5);
    }
   }else if(!struck && s==0x81d5 && !--a[28])state(a,0x8234);
  }
 }
 if(pass&2)for(unsigned i=0;i<12;++i){uint8_t *a=w->bullets[i];if(!a[0])continue;unsigned s=word(a+2);int draw=0;
  if(s==0x8635){a[0]=1;if(a[15]>10 || (a[15]&1))return -8;if(animation(a,b,word(b+0x71b+a[15]),0))return -8;a[5]=(uint8_t)(word(a+10)>>1);state(a,word(b+0x659+a[15]));}
  else if(s==0x8665 || s==0x866a || s==0x868c){
   if(s!=0x868c){uint32_t x=a[9]|((uint32_t)a[10]<<8)|((uint32_t)a[11]<<16);x=(x+(s==0x8665?-192:192))&0xffffff;a[9]=(uint8_t)x;a[10]=(uint8_t)(x>>8);a[11]=(uint8_t)(x>>16);a[5]=(uint8_t)(word(a+10)>>1);}
   a[13]-=6;memcpy(w->bullet_draw[i],a,24);draw=1;
   unsigned at=(((uint8_t)(a[13]-4)>>3)<<6)|((word(a+10)&511)>>3);
   if(grid[at]){
    state(a,0x86cd);
    /* BANK2:86C1..86CA requests F02 before the next projectile task. */
    if(sound)sound(ctx,2);
   }
   else{int struck=0;for(unsigned n=0;n<32;++n){uint8_t *ball=balls+n*32;if((!g && ball[0]!=1) || !ball[27])continue;
    unsigned type=ball[28],rx=type==1?3:type==2?5:type==3?9:type==4?13:17,ry=type==1?7:type==2?11:type==3?19:type==4?27:32;
    if((uint8_t)(ball[27]+rx-a[5])<rx*2+1 && (uint8_t)(ball[13]+ry-a[13])<ry*2+1){if(hit(a,ball,p,1,g,p2))return -9;struck=1;break;}
   }
   if(!struck){lucky_contact(a,lucky,contact_frame,1);if(contact)contact(ctx,a,1,contact_frame);}
   }
  }else if(s==0x86cd || s==0x86d7){
   if(s==0x86cd){a[5]=4;state(a,0x86d7);}
   if(!--a[8]){unsigned ptr=word(a+6),next=ptr+5;
    if(ptr<0x8000||ptr>0xbffb)return -10;
    if(b[ptr-0x8000]&128){if(next>0xbffe)return -10;next=word(b+(next-0x8000));}
    if(animation(a,b,next,0))return -10;
   }
   memcpy(w->bullet_draw[i],a,24);draw=1;if(!--a[5])state(a,0x86e7);
  }else if(s==0x86e7){a[0]=0;state(a,0x8635);}else return -11;
  if(draw)w->bullet_visible|=(uint16_t)(1u<<i);
 }
 return 0;
}
int spang_weapons_step(spang_weapons *w,uint8_t frame,int edge,int advance,const uint8_t b[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t p[128],spang_weapon_tile_fn cb,void *ctx)
{return weapons_step(w,frame,edge,advance,b,grid,balls,p,cb,ctx,0,0,3,0,frame,0,0);}
int spang_weapons_panic_step(spang_weapons *w,uint8_t frame,int edge,int advance,const uint8_t b[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t p[128],spang_weapon_tile_fn cb,void *ctx,uint8_t g[128],uint8_t p2[128])
{if(!g || !p2 || g[0x1a]!=2)return -12;return weapons_step(w,frame,edge,advance,b,grid,balls,p,cb,ctx,g,p2,3,0,frame,0,0);}
int spang_weapons_round_step_audio(spang_weapons *w1,spang_weapons *w2,uint8_t frame,const int edges[2],int advance,const uint8_t b[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t p1[128],uint8_t p2[128],uint8_t g[128],uint8_t lucky[32],spang_weapon_tile_fn cb,spang_weapon_contact_fn contact,spang_weapon_sound_fn sound,void *ctx)
{
 if(!g || (g[0x1a]!=0 && g[0x1a]!=2) || !p1 || !p2 || !lucky)return -12;
 for(unsigned pass=1;pass<=2;++pass)for(unsigned owner=0;owner<2;++owner){
  uint8_t *p=g[0x1a]?p1:owner?p2:p1;
  int r=weapons_step(owner?w2:w1,frame,edges[owner],advance,b,grid,balls,p,cb,ctx,g[0x1a]?g:0,p2,pass,lucky,g[9],contact,sound);
  if(r)return r;
 }
 return 0;
}
int spang_weapons_round_step(spang_weapons *w1,spang_weapons *w2,uint8_t frame,const int edges[2],int advance,const uint8_t b[16384],uint8_t grid[2048],uint8_t balls[1024],uint8_t p1[128],uint8_t p2[128],uint8_t g[128],uint8_t lucky[32],spang_weapon_tile_fn cb,spang_weapon_contact_fn contact,void *ctx)
{return spang_weapons_round_step_audio(w1,w2,frame,edges,advance,b,grid,balls,p1,p2,g,lucky,cb,contact,0,ctx);}
int spang_weapons_draw(const spang_weapons *w,const uint8_t b[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining)
{
 for(unsigned i=0;i<2;++i)if(w->harpoon_visible&(1u<<i)){int n=spang_harpoon_sprite(w->harpoon_draw[i],b,objects,cursor,remaining);if(n<0)return -1;}
 for(unsigned i=0;i<12;++i)if(w->bullet_visible&(1u<<i)){
  if(!*remaining)break;
  if(*cursor+32>0xfe0)return -2;
  const uint8_t *a=w->bullet_draw[i];unsigned ptr=word(a+6);if(ptr<0x8000 || ptr+4>=0xc000)return -3;
  unsigned x=(word(a+10)-8)&511;uint8_t *o=objects+*cursor;memset(o,0,32);
  o[0]=b[ptr+3-0x8000];o[1]=b[ptr+4-0x8000]|((x>>4)&16);o[2]=(uint8_t)(a[13]-8);o[3]=(uint8_t)x;
  *cursor+=32;--*remaining;
 }
 return 0;
}
