#include "spang_round_scene.h"
#include "spang_round_ball.h"
#include "spang_sprite_overlay.h"
#include <string.h>
typedef struct {const uint8_t *bank,*grid,*globals;int fault;} bus_t;
static uint8_t read(void *context,uint16_t a)
{
 bus_t *bus=context;
 if(a>=0x8000 && a<0xc000)return bus->bank[a-0x8000];
 if(a>=0xe080 && a<0xe100)return bus->globals[a-0xe080];
 if(a>=0xf000 && a<0xf800)return bus->grid[a-0xf000];
 bus->fault=1;return 0;
}
int spang_round_scene_step_full(uint8_t actors[1024],uint8_t objects[4096],
 uint8_t *remaining,const uint8_t bank[16384],const uint8_t grid[2048],
 uint8_t g[128],int advance,spang_round_random_fn random,void *context)
{
 if(!actors || !objects || !remaining || !bank || !grid || !g)return -19;
 if(advance && !random)for(unsigned i=0;i<32;++i){
  const uint8_t *a=actors+i*32;unsigned state=a[2]|((unsigned)a[3]<<8);
  if(a[0] && (state==0x9155 || state==0x915d) &&
     (a[15]==22 || a[15]==26 || a[15]==34))return -26;
 }
 uint16_t step=g[0x22]|((uint16_t)g[0x23]<<8),dx=g[0x24]|((uint16_t)g[0x25]<<8);
 int freeze=g[0x2b],hide=g[0x2c];
 bus_t bus={bank,grid,g,0};uint16_t cursor=0;*remaining=127;
 memset(objects,0,4096);
 for(unsigned i=0;i<32;++i){uint8_t *a=actors+i*32;if(!a[0])continue;
  unsigned state=a[2]|((unsigned)a[3]<<8);
  int retiring=advance && (state==0x9155 || state==0x915d);
  int physics=state!=0x82b0 && state!=0x90c1 && state!=0x912e && state!=0x913f && state!=0x9155 && state!=0x915d;
  if(advance){
   if(state==0x82b0){if(spang_round_ball_init(a,g[0x1a],bank))return -20;}
   else if(state==0x90c1){a[2]=0x55;a[3]=0x91;}
   else if(state==0x912e || state==0x913f){a[2]=0x5d;a[3]=0x91;}
   else if(!retiring && !freeze){if(spang_round_ball_update(a,step,dx,0,read,&bus,g+0x2a))return -21;}
  }
  uint16_t first=cursor;
  /* 9458 suppresses only frozen movement handlers, not pending hit/split
   * handlers. Death/pause uses a copy so drawing cannot tick animations. */
  uint8_t frozen[32];uint8_t *draw=a;
  if(!advance){memcpy(frozen,a,32);draw=frozen;}
  int count=(freeze && hide && physics)?0:spang_round_ball_sprite(draw,bank,objects,&cursor,remaining);
  if(count<0)return -22;
  /* Tag after the ROM-exact sprite producer, using the complete ball's
   * left edge. Tags are Saturn renderer metadata, not arcade object bytes. */
  unsigned radius=count==16?32:count==9?24:count==4?16:8;
  uint16_t origin=(uint16_t)((a[10]|((unsigned)a[11]<<8))-radius);
  for(unsigned pos=first;pos<cursor;pos+=32){
   objects[pos+4]=SPANG_SPRITE_ANCHOR_TAG;
   objects[pos+5]=(uint8_t)origin;objects[pos+6]=(uint8_t)(origin>>8);
  }
  if(retiring){a[0]=a[1]=a[22]=0;
   if(a[28]!=1){
    uint8_t value=(a[15]==22 || a[15]==26 || a[15]==34)?random(context):0;
    if(spang_round_ball_split_ex(a,actors,bank,value,g+0x2a)<0)return -23;
   }
  }
  if(bus.fault)return -24;
 }
 return 0;
}

/* Compatibility entry for the original round-only diagnostics. It cannot
 * synthesize diamond speeds or a black-child RNG stream. */
int spang_round_scene_step_ex(uint8_t actors[1024],uint8_t objects[4096],
 uint8_t *remaining,const uint8_t bank[16384],const uint8_t grid[2048],
 uint16_t step,uint16_t dx,int advance,int freeze,int hide)
{
 if(!actors)return -19;
 if(advance && !freeze)for(unsigned i=0;i<32;++i){
  const uint8_t *a=actors+i*32;
  if(a[0] && (a[15]==4 || a[15]==16 || a[15]==28))return -25;
 }
 uint8_t g[128]={0};g[0x22]=(uint8_t)step;g[0x23]=(uint8_t)(step>>8);
 g[0x24]=(uint8_t)dx;g[0x25]=(uint8_t)(dx>>8);g[0x2b]=(uint8_t)freeze;g[0x2c]=(uint8_t)hide;
 return spang_round_scene_step_full(actors,objects,remaining,bank,grid,g,advance,0,0);
}

int spang_round_scene_step(uint8_t actors[1024],uint8_t objects[4096],
 uint8_t *remaining,const uint8_t bank[16384],const uint8_t grid[2048],
 uint16_t step,uint16_t dx,int advance)
{return spang_round_scene_step_ex(actors,objects,remaining,bank,grid,step,dx,advance,0,0);}
