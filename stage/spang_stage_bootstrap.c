#include "spang_stage_bootstrap.h"
#include <string.h>

static unsigned word(const uint8_t *p)
{return p[0] | ((unsigned)p[1] << 8);}

void spang_stage_pool_clear(uint8_t a[1024])
{memset(a,0,1024);for(unsigned i=0;i<32;++i)a[i*32+16]=3;}

void spang_stage_grid_clear(uint8_t g[2048])
{
 /* 835D: only the playfield rectangle, leaving unrelated RAM untouched. */
 memset(g+0x48,255,48);memset(g+0x688,254,48);
 for(unsigned row=2;row<26;++row){g[row*64+8]=g[row*64+55]=253;memset(g+row*64+9,0,46);}
}

/* BANK5:84F9 dispatch, 8563..8599; allocators FIXED:1591..15B1.
 * This describes creation only. Equal allocation classes do not imply equal
 * motion, split rules or scores in the runtime ball controller. */
static int allocation(unsigned type,const uint8_t *bank,unsigned *start,unsigned *weight)
{
 if(type>36 || (type&1))return -1;
 switch(word(bank+0x4f9+type)){
 case 0x8563:*start=0; *weight=1; break;
 case 0x856a:*start=16;*weight=2; break;
 case 0x8571:*start=16;*weight=4; break;
 case 0x8578:*start=24;*weight=4; break;
 case 0x857f:*start=24;*weight=10;break;
 case 0x8586:*start=28;*weight=8; break;
 case 0x858d:*start=28;*weight=22;break;
 case 0x8594:*start=30;*weight=16;break;
 default:return -1;
 }
 return 0;
}

int spang_stage_balls(unsigned stage,uint8_t a[1024],const uint8_t b[16384],uint8_t *weight)
{
 if(stage>=SPANG_STAGE_TABLE_COUNT || !a || !b || !weight)return -1;
 unsigned p=word(b+0xb82+stage*2);
 if(p<0x8000 || p>=0xc000)return -1;
 p-=0x8000;unsigned count=b[p++];
 if(count>32 || p+count*5>16384)return -1;
 unsigned total=0,used[32],slots[32];
 for(unsigned i=0;i<32;++i)used[i]=a[i*32]!=0;
 /* Validate the complete allocation plan before publishing any actor. */
 for(unsigned i=0;i<count;++i){
  unsigned start,w;
  unsigned type=b[p+5*i];
  unsigned directions=(type==4 || type==16 || type==28)?8u:4u;
  if(allocation(type,b,&start,&w) || b[p+5*i+1]>=directions)return -1;
  unsigned slot=start;while(slot<32 && used[slot])++slot;
  if(slot==32)return -1;
  used[slot]=1;slots[i]=slot;total+=w;
 }
 if(total>32)return -1;
 for(unsigned i=0;i<count;++i,p+=5){
  uint8_t *v=a+slots[i]*32;
  v[15]=b[p];v[18]=b[p+1];v[0]=2;v[10]=b[p+2];v[11]=b[p+3];
  v[13]=v[14]=b[p+4];v[2]=0xb0;v[3]=0x82;
 }
 *weight=(uint8_t)total;return 0;
}

enum {PERMANENT,DESTRUCTIBLE,HIDDEN,SPECIAL};
typedef struct {unsigned records[2],count[2],kind;} layer;

/* Each table points to H-count/H-records then V-count/V-records, except
 * 9C2D which has only horizontal special surfaces. Platform counts are not
 * actor counts: Tour phase 36 really contains 186 permanent H records. */
static int layer_validate(unsigned stage,const uint8_t *bank,unsigned table,
                          unsigned kind,layer *out)
{
 unsigned ptr=word(bank+table+stage*2);
 if(ptr<0x8000 || ptr>=0xc000)return -1;
 unsigned p=ptr-0x8000;out->kind=kind;
 out->count[0]=out->count[1]=0;
 for(unsigned axis=0;axis<(kind==SPECIAL?1u:2u);++axis){
  if(p>=16384)return -1;
  unsigned count=bank[p++];out->records[axis]=p;out->count[axis]=count;
  if(p+count*3>16384)return -1;
  for(unsigned i=0;i<count;++i,p+=3){
   unsigned off=word(bank+p),encoded=bank[p+2];
   unsigned n=(kind==DESTRUCTIBLE || kind==HIDDEN)?encoded&7:encoded;
   if(!n || off>=2048)return -1;
   if(axis){
    if(off+(n-1)*64>=2048)return -1;
    if(kind!=HIDDEN && n<2)return -1;
   }else{
    /* Original INC L does not carry. Valid records must not cross either
     * the collision-byte page or the tile-word page. */
    if(off+n>2048 || (off&255)+n>256)return -1;
    if(kind!=HIDDEN && (off&127)+n>128)return -1;
   }
  }
 }
 return 0;
}

static void tile(uint8_t *chars,unsigned off,unsigned code)
{chars[off*2]=(uint8_t)code;chars[off*2+1]=(uint8_t)(code>>8);}

static unsigned horizontal_tile(unsigned kind,unsigned n,unsigned i)
{
 if(kind==PERMANENT){
  if(n==1)return 0x706;
  return i==0?0x707:i==n-1?0x708:0x709;
 }
 if(kind==DESTRUCTIBLE){
  if(n==1)return 0xaa0;
  if(n<4)return i==0?0xaa1:i==n-1?0xaa2:0xab3;
  return i==0?0xab2:i==1?0xab3:i==n-2?0xab4:i==n-1?0xab5:0xaa3;
 }
 /* 887E: the odd tail is tile A88 in the ROM, not AB8. */
 if(n==1)return 0xaa5;
 if(i==0)return 0xaa8;
 if(i==n-1)return 0xaa9;
 if(n>=5 && (n&1) && i==n-2)return 0xa88;
 return (i&1)?0xab8:0xab9;
}

static unsigned vertical_tile(unsigned kind,unsigned n,unsigned i,unsigned under)
{
 if(kind==PERMANENT){
  if(i==0)return under==0x707?0x70b:under==0x708?0x717:0x70a;
  if(i==n-1)return under==0x707?0x716:under==0x708?0x719:0x71a;
  return 0x718;
 }
 if(n<4)return i==0?0xab0:i==n-1?0xab1:(i&1)?0xab6:0xab7;
 return i==0?0xaa6:i==1?0xab6:i==n-2?0xaa7:i==n-1?0xab7:0xaa4;
}

static void layer_apply(const layer *current_layer,const uint8_t *bank,uint8_t *grid,
                        uint8_t *attrs,uint8_t *chars)
{
 unsigned kind=current_layer->kind;
 for(unsigned axis=0;axis<2;++axis){
  unsigned p=current_layer->records[axis];
  for(unsigned record=0;record<current_layer->count[axis];++record,p+=3){
   unsigned off=word(bank+p),encoded=bank[p+2];
   unsigned n=(kind==DESTRUCTIBLE || kind==HIDDEN)?encoded&7:encoded;
   for(unsigned i=0;i<n;++i){
    unsigned at=off+i*(axis?64u:1u);
    if(kind==PERMANENT)grid[at]=255;
    else if(kind==SPECIAL)grid[at]=252;
    else grid[at]=(uint8_t)(axis?(encoded|192):i?i:(encoded|128));
    if(kind==HIDDEN)continue;
    attrs[at]=(uint8_t)(kind==SPECIAL?19:13);
    unsigned code=axis?vertical_tile(kind,n,i,word(chars+at*2)):horizontal_tile(kind,n,i);
    tile(chars,at,code);
   }
  }
 }
}

int spang_stage_platforms(unsigned stage,uint8_t g[2048],uint8_t attrs[2048],uint8_t chars[4096],const uint8_t b[16384])
{
 if(stage>=SPANG_STAGE_TABLE_COUNT || !g || !attrs || !chars || !b)return -1;
 static const unsigned tables[4]={0x11ac,0x17b0,0x1b29,0x1c2d};
 layer layers[4]={0};
 /* Preflight every layer, including the last one, before changing memory. */
 unsigned kinds=stage<SPANG_TOUR_STAGE_COUNT?4:2; /* FIXED3DF5/3DF8: tutorial has only these layers. */
 for(unsigned kind=0;kind<kinds;++kind)
  if(layer_validate(stage,b,tables[kind],kind,layers+kind))return -1;
 for(unsigned kind=0;kind<kinds;++kind)layer_apply(layers+kind,b,g,attrs,chars);
 return 0;
}

int spang_stage_ladders(unsigned stage,uint8_t a[128],const uint8_t b[16384])
{
 if(stage>=SPANG_STAGE_TABLE_COUNT || !a || !b)return -1;
 unsigned ptr=word(b+0x1cac+stage*2);
 if(ptr<0x8000 || ptr>=0xc000)return -1;
 unsigned p=ptr-0x8000,n=b[p++];
 if(n>8 || p+n*5>16384)return -1;
 for(unsigned i=0;i<n;++i,p+=5){
  uint8_t *v=a+i*16;
  v[0]=1;v[10]=b[p];v[11]=b[p+1];v[13]=b[p+2];v[12]=b[p+3];
  v[15]=b[p+4];v[1]=(uint8_t)(12+i*16);
 }
 return 0;
}

void spang_stage_player(uint8_t p[128],unsigned stage,const uint8_t b[16384])
{
 if(stage>=SPANG_TOUR_STAGE_COUNT || !p || !b || p[0x11]>1)return;
 /* 36F4 + bank0 A557/B51D/ADEB. Retain the session fields. */
 p[0]=p[1]=1;p[2]=2;p[3]=p[4]=p[5]=p[0x2f]=p[0x58]=0;
 p[0x54]=p[0x20]=p[0x14]=p[0x15]=p[0x59]=p[0x5a]=0;
 p[0x4f]=p[0x52]=255;p[0x50]=p[0x53]=p[0x23]=0;
 p[0x57]=(uint8_t)(p[0x11]*8);p[0x21]=p[0x11];
 unsigned pos=0x33da+stage*6+p[0x11]*3;
 p[9]=p[12]=0;p[10]=b[pos];p[11]=b[pos+1];p[13]=b[pos+2];p[14]=0;
 unsigned anim=word(b+0x2df8+p[0x11]*4+p[0x21]*2);p[6]=(uint8_t)anim;p[7]=(uint8_t)(anim>>8);p[8]=b[anim-0x8000]&127;
}
