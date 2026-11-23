#include "spang_round_ball.h"
#include <string.h>
/* Even ROM type IDs. Slots 6/18/30 are the upper halves of the eight-way
 * diamond dispatch tables, not independently spawned Tour actors. */
static int canonical_type(unsigned type)
{return type<=36 && !(type&1) && (0x77df7u & (1u<<(type>>1)));}
static int panic_type(unsigned type){return type==38 || type==40 || type==42;}
static unsigned bankword(const uint8_t *b,unsigned a){return b[a-0x8000]|((unsigned)b[a+1-0x8000]<<8);}
/* 1599/15A1/15A9/15B1: a saturated partition deliberately falls back to
 * IX. Do not clear a free slot: the ROM preserves its unrelated bytes. */
static uint8_t *child_slot(uint8_t *parent,uint8_t *pool,unsigned start)
{
 for(unsigned i=start;i<32;++i)if(!pool[i*32])return pool+i*32;
 return parent;
}
static void inherit_special(uint8_t *parent,uint8_t *child)
{uint8_t special=parent[26];parent[26]=0;child[26]=special;}
int spang_round_ball_split_ex(uint8_t a[32],uint8_t pool[1024],
 const uint8_t bank[16384],uint8_t random,uint8_t *flag)
{
 if(!a || !pool || !bank || !flag || !canonical_type(a[15]) || a[15]<8)return -1;
 unsigned type=a[15],start=type<20?0:type<32?16:type<36?24:28;
 if(type==16 || type==28){
  unsigned table=(type==16?0x9597:0x95a7)+(a[18]<4?4:0);
  unsigned direction=a[18]<4?1:0,child_type=type==16?4:16;
  for(unsigned n=0;n<2;++n,table+=8){
   uint8_t *child=child_slot(a,pool,start);
   child[0]=2;child[15]=(uint8_t)child_type;child[18]=(uint8_t)((direction-n*2)&7);
   uint16_t x=(uint16_t)((a[10]|((uint16_t)a[11]<<8))+bankword(bank,table));
   child[10]=(uint8_t)x;child[11]=(uint8_t)(x>>8);
   child[13]=(uint8_t)(a[13]+bank[table+2-0x8000]);
   child[2]=0xb0;child[3]=0x82;inherit_special(a,child);
  }
  return 2;
 }
 unsigned four=type==10 || type==14 || type==22 || type==26 || type==34;
 unsigned choose=type==22 || type==26 || type==34;
 unsigned base_type=type<20?0:type<32?8:type<36?20:32;
 if(type==12)base_type=2;
 if(type==24)base_type=12;
 if(choose)*flag=(uint8_t)((random&3)+1);
 for(unsigned group=0;group<(four?2u:1u);++group){
  uint8_t offset=(uint8_t)(a[13]-a[14]),height;
  uint16_t phase;
  if(group){height=offset;phase=(uint16_t)bankword(bank,0x8000+offset*2u);}
  else{
   offset=(uint8_t)(offset+16);offset&=type<20?0xfc:0xfe;
   height=(uint8_t)(offset>>(type<20?2:1));
   phase=(uint16_t)bankword(bank,0x8000+(type<20?height*2u:offset));
  }
  unsigned table=four?(type<20?0x95bf:0x95d7)+group*8:(type<20?0x95b7:0x95cf);
  for(unsigned n=0;n<2;++n,table+=4){
   uint8_t *child=child_slot(a,pool,start);
   unsigned child_type=base_type+group*(type<20?2:4);
   if(choose && !--*flag)child_type+=2;
   child[0]=2;child[15]=(uint8_t)child_type;
   uint8_t y=(uint8_t)(a[13]+bank[table-0x8000]);child[13]=y;
   uint16_t p=phase;uint8_t base=(uint8_t)(y-height);
   if(y<height){base=a[13];p=0;}
   child[14]=base;child[30]=(uint8_t)p;child[31]=(uint8_t)(p>>8);
   uint16_t x=(uint16_t)((a[10]|((unsigned)a[11]<<8))+bankword(bank,table+1));
   child[10]=(uint8_t)x;child[11]=(uint8_t)(x>>8);child[2]=0xb0;child[3]=0x82;
   child[18]=bank[table+3-0x8000];inherit_special(a,child);
  }
 }
 return four?4:2;
}
int spang_round_ball_split(uint8_t a[32],uint8_t pool[1024],const uint8_t bank[16384])
{
 /* The compatibility API has no RNG owner; never silently pick a black child. */
 if(!a || a[15]==22 || a[15]==26 || a[15]==34)return -1;
 uint8_t flag=0;return spang_round_ball_split_ex(a,pool,bank,0,&flag);
}
int spang_round_ball_init(uint8_t a[32],uint8_t level,const uint8_t bank[16384])
{
 if(!a || !bank || (!canonical_type(a[15]) && !(level==2 && panic_type(a[15]))))return -1;
 unsigned directions=(a[15]==4 || a[15]==16 || a[15]==28)?8u:4u;
 if(a[18]>=directions)return -1;
 /* 8303/8311: small special balls select the alternate animation table. */
 unsigned animation=bankword(bank,(a[15]<8 && a[26]?0x9a46:0x9a1a)+a[15]);
 if(animation<0x8000 || animation+4>=0xc000)return -1;
 unsigned next=bankword(bank,0x8200+(uint8_t)((a[15]*2+a[18])*2));
 a[0]=a[1]=1;a[25]=a[18];a[2]=(uint8_t)next;a[3]=(uint8_t)(next>>8);
 a[27]=(uint8_t)((a[10]|((unsigned)a[11]<<8))>>1);
 a[28]=bank[0x315+(a[15]>>1)];
 a[22]=bank[0x32b+(uint8_t)(((level>>1)|(level<<7))+a[15])];
 a[6]=(uint8_t)animation;a[7]=(uint8_t)(animation>>8);a[8]=bank[animation-0x8000]&0x7f;
 return 0;
}
int spang_round_ball_sprite(uint8_t a[32],const uint8_t bank[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining)
{
 unsigned animation=a[6]|((unsigned)a[7]<<8);
 if(animation<0x8000 || animation+4>=0xc000)return -1;
 unsigned route=bankword(bank,animation+1),width,clipped=0,icon=0;
 if(route==0x2108 || route==0x2112)width=1;
 else if(route==0x21a3 || route==0x21ad)width=2;
 else if(route==0x22b8 || route==0x22c2)width=3;
 else if(route==0x250a)width=4;
 else if(route==0x26f8 || route==0x2702){width=2;clipped=1;}
 else if(route==0x2847){width=3;clipped=1;}
 else if(route==0x2a42){width=4;clipped=1;}
 else if(route==0x2221 || route==0x278e){width=2;icon=1;clipped=route==0x278e;}
 else if(route==0x238a || route==0x2931){width=3;icon=1;clipped=route==0x2931;}
 else return -1;
 unsigned count=width*width+icon;
 if((*cursor&31) || *cursor>0xfe0)return -1;
 if(route==0x2108 || route==0x21a3 || route==0x22b8 || route==0x26f8){
  if(!--a[8]){unsigned next=animation+5;if(bank[animation-0x8000]&0x80)next=bankword(bank,next);
   if(next<0x8000 || next>=0xc000)return -1;
   a[6]=(uint8_t)next;a[7]=(uint8_t)(next>>8);a[8]=bank[next-0x8000]&0x7f;
  }
 }
 if(*remaining<count)return 0;
 if(*cursor+count*32>0xfe0)return -1;
 uint8_t code=bank[animation+3+icon-0x8000],attr=bank[animation+4+2*icon-0x8000];
 uint16_t x=(uint16_t)((a[10]|((unsigned)a[11]<<8))-width*8);
 unsigned slot=0;
 if(icon){
  unsigned dest=*cursor,y=a[13]|((unsigned)a[14]<<8),ix=(a[10]|((unsigned)a[11]<<8))-8;
  objects[dest]=bank[animation+3-0x8000];objects[dest+1]=bank[animation+5-0x8000]|((ix>>4)&16);
  objects[dest+2]=(uint8_t)(clipped && (y&256)?248:y-8);objects[dest+3]=(uint8_t)ix;slot=1;
 }
 for(unsigned col=0;col<width;++col){
  attr|=(uint8_t)(((x+col*16)>>4)&16);
  for(unsigned j=0;j<width;++j){unsigned row=(col&1)?width-1-j:j;unsigned dest=*cursor+slot++*32;
   objects[dest]=(uint8_t)(code+col+row*8);objects[dest+1]=attr;
   unsigned y=(a[13]|((unsigned)a[14]<<8))-width*8+8+row*16;
   objects[dest+2]=(uint8_t)(clipped && (y&256)?248:y-8);objects[dest+3]=(uint8_t)(x+col*16);
  }
 }
 *cursor+=(uint16_t)(count*32);*remaining-=(uint8_t)count;return (int)count;
}
static uint16_t word(spang_ball_read_fn r,void *c,uint16_t a)
{return r(c,a)|((uint16_t)r(c,(uint16_t)(a+1))<<8);}
static void state(uint8_t *a,spang_ball_read_fn r,void *c)
{
 uint8_t index=(uint8_t)((a[15]*2+a[18])*2);
 uint16_t v=word(r,c,0x8200+index);a[2]=(uint8_t)v;a[3]=(uint8_t)(v>>8);
}
/* A004/A05E: ordered multi-point ceiling/floor tests from ROM tables. */
static void vertical(uint8_t *a,uint16_t x,uint16_t table,int up,
 spang_ball_read_fn r,void *c,uint8_t *flag)
{
 unsigned count=r(c,table++);uint8_t base=a[13];
 for(unsigned i=0;i<count;++i,table+=3){
  uint16_t px=(uint16_t)(x+word(r,c,table));uint8_t offset=r(c,table+2),y=(uint8_t)(base+offset);
  if(!r(c,(uint16_t)(0xf000+((y>>3)<<6)+((px>>3)&63))))continue;
  if(up){*flag=(uint8_t)(((~y)&7)+1);a[13]=(uint8_t)(y+*flag-offset);--a[18];}
  else{
   if(y>=0xd0){
    uint16_t t=(uint16_t)(0xa10b+(uint8_t)(a[15]*2));a[30]=r(c,t);a[31]=r(c,t+1);a[14]=r(c,t+2);
    if(panic_type(a[15])){
     a[4]=(uint8_t)((a[4]+2)&2);a[5]=r(c,(uint16_t)(0xa109+(a[4]>>1)));
     unsigned anim=word(r,c,(uint16_t)(0x99da+a[15]*3+a[5]));a[6]=(uint8_t)anim;a[7]=(uint8_t)(anim>>8);a[8]=r(c,(uint16_t)anim)&127;
    }
   }
   *flag=(uint8_t)((y&7)+1);a[13]=(uint8_t)(y-(*flag+offset));++a[18];
  }
  state(a,r,c);return;
 }
}
/* 9EF3/9F5D and the final 9FC3 corner probe. */
static void horizontal(uint8_t *a,uint16_t x,uint16_t table,int left,
 spang_ball_read_fn r,void *c,uint8_t flag)
{
 unsigned count=r(c,table++);uint8_t base=a[13];
 for(unsigned i=0;i<=count;++i,table+=3){
  uint16_t px=(uint16_t)(x+word(r,c,table));uint8_t y=(uint8_t)(base+r(c,table+2));
  if(!r(c,(uint16_t)(0xf000+((y>>3)<<6)+((px>>3)&63))))continue;
  if(i==count){a[18]^=3;state(a,r,c);return;}
  a[18]=(uint8_t)((a[18]+2)&3);state(a,r,c);
  if(!flag){
   if(left){unsigned sum=a[10]+((~px)&7)+1;a[10]=(uint8_t)sum;if(sum>255)a[11]=1;}
   else{unsigned delta=(px&7)+1;if(a[10]<delta)a[11]=0;a[10]=(uint8_t)(a[10]-delta);}
  }
  return;
 }
}
/* Eight-way diamonds use linear 8.8 velocity components E0B0..E0B7,
 * not the parabola phase table. Keep the ROM's probe order: the tiny
 * steep handlers test Y then X; the shallow handlers test X then Y. */
static int occupied(spang_ball_read_fn r,void *c,uint16_t x,uint8_t y)
{return r(c,(uint16_t)(0xf000+((y>>3)<<6)+((x>>3)&63)))!=0;}
static void diamond_tiny_y(uint8_t *a,uint16_t x,int up,spang_ball_read_fn r,void *c)
{
 uint8_t y=(uint8_t)(a[13]+(up?-4:3));
 if(!occupied(r,c,(uint16_t)(x+(up?1:-2)),y))return;
 a[13]=(uint8_t)(up?y+((~y)&7)+5:y-((y&7)+4));
 a[18]=(uint8_t)((3u-a[18])&7);state(a,r,c);
}
static void diamond_tiny_x(uint8_t *a,uint16_t x,int left,int up,spang_ball_read_fn r,void *c)
{
 uint16_t px=(uint16_t)(x+(left?-4:3));
 uint8_t y=(uint8_t)(a[13]+(up?-2:1));
 if(!occupied(r,c,px,y))return;
 a[18]=(uint8_t)((7u-a[18])&7);state(a,r,c);
 unsigned low=px&255;
 if(left){unsigned sum=low+((~low)&7)+5;a[10]=(uint8_t)sum;if(sum>255)a[11]=1;}
 else{unsigned delta=(low&7)+4;a[10]=(uint8_t)(low-delta);if(low<delta)a[11]=0;}
}
/* 9D50/9DAE do not change bounce phase/base. Only the larger diamond's
 * floor probe writes E0AA; the ceiling and tiny probes leave it alone. */
static void diamond_y(uint8_t *a,uint16_t x,uint16_t table,int up,
 spang_ball_read_fn r,void *c,uint8_t *flag)
{
 unsigned n=r(c,table++);uint8_t base=a[13];
 for(unsigned i=0;i<n;++i,table+=3){
  uint16_t px=(uint16_t)(x+word(r,c,table));uint8_t off=r(c,table+2),y=(uint8_t)(base+off);
  if(!occupied(r,c,px,y))continue;
  if(up)a[13]=(uint8_t)(y+((~y)&7)+1-off);
  else{*flag=(uint8_t)((y&7)+1);a[13]=(uint8_t)(y-(*flag+off));}
  a[18]=(uint8_t)((3u-a[18])&7);state(a,r,c);return;
 }
}
/* 9C8B/9CEE: unlike round balls, no extra corner probe follows the table. */
static void diamond_x(uint8_t *a,uint16_t x,uint16_t table,int left,
 spang_ball_read_fn r,void *c)
{
 unsigned n=r(c,table++);uint8_t base=a[13];
 for(unsigned i=0;i<n;++i,table+=3){
  uint16_t px=(uint16_t)(x+word(r,c,table));uint8_t y=(uint8_t)(base+r(c,table+2));
  if(!occupied(r,c,px,y))continue;
  a[18]=(uint8_t)((7u-a[18])&7);state(a,r,c);
  if(left){unsigned sum=a[10]+((~px)&7)+1;a[10]=(uint8_t)sum;if(sum>255)a[11]=1;}
  else{unsigned delta=(px&7)+1;if(a[10]<delta)a[11]=0;a[10]=(uint8_t)(a[10]-delta);}
  return;
 }
}
static int diamond_update(uint8_t actor[32],uint8_t freeze,
 spang_ball_read_fn read,void *context,uint8_t *flag,uint8_t scratch_x[2])
{
 unsigned d=actor[18];if(d>7)return -1;
 if(freeze)return -2;
 uint16_t expected=word(read,context,(uint16_t)(0x8200+((actor[15]*2+d)*2)));
 if((actor[2]|((uint16_t)actor[3]<<8))!=expected)return -1;
 int left=d>=4,up=d<2 || d>5,steep=(d&3)==0 || (d&3)==3;
 uint16_t vx=word(read,context,(uint16_t)(steep?0xe0b0:0xe0b4));
 uint16_t vy=word(read,context,(uint16_t)(steep?0xe0b2:0xe0b6));
 uint8_t a[32],f=*flag;memcpy(a,actor,32);
 uint16_t y=a[12]|((uint16_t)a[13]<<8);y=(uint16_t)(up?y-vy:y+vy);
 a[12]=(uint8_t)y;a[13]=(uint8_t)(y>>8);
 uint32_t x=a[9]|((uint32_t)a[10]<<8)|((uint32_t)a[11]<<16);
 x=(left?x-vx:x+vx)&0xffffff;a[9]=(uint8_t)x;a[10]=(uint8_t)(x>>8);a[11]=(uint8_t)(x>>16);
 uint16_t integer_x=(uint16_t)(x>>8);
 if(a[15]!=4 && scratch_x){scratch_x[0]=(uint8_t)integer_x;scratch_x[1]=(uint8_t)(integer_x>>8);}
 if(a[15]==4){
  if(steep){diamond_tiny_y(a,integer_x,up,read,context);diamond_tiny_x(a,integer_x,left,up,read,context);}
  else{diamond_tiny_x(a,integer_x,left,up,read,context);integer_x=a[10]|((uint16_t)a[11]<<8);diamond_tiny_y(a,integer_x,up,read,context);}
 }else{
  static const uint16_t tables[2][8]={
   {0x97a2,0x97a9,0x97b3,0x97ba,0x97c4,0x97cb,0x97d5,0x97dc},
   {0x97e6,0x97f3,0x9803,0x9810,0x9820,0x982d,0x983d,0x984a}};
  unsigned size=a[15]==16?0:1,column=(left?4:0)+(up?2:0);
  diamond_y(a,integer_x,tables[size][column],up,read,context,&f);
  diamond_x(a,integer_x,tables[size][column+1],left,read,context);
 }
 a[27]=(uint8_t)((a[10]|((uint16_t)a[11]<<8))>>1);
 memcpy(actor,a,32);*flag=f;return 0;
}
int spang_round_ball_update_full(uint8_t actor[32],uint16_t step,uint16_t dx,
 uint8_t freeze,spang_ball_read_fn read,void *context,uint8_t *flag,uint8_t scratch_x[2])
{
 if(!actor || !read || !flag || (!canonical_type(actor[15]) && !panic_type(actor[15])))return -1;
 if(actor[15]==4 || actor[15]==16 || actor[15]==28)return diamond_update(actor,freeze,read,context,flag,scratch_x);
 if(actor[15]==0 || actor[15]==2)return spang_small_ball_update(actor,step,dx,freeze,read,context,flag);
 unsigned type=actor[15];
 unsigned size=(type>=8 && type<=14)?0:((type>=20 && type<=26) || type==38 || type==40)?1:
               (type==32 || type==34 || type==42)?2:type==36?3:4;
 if(size==4 || actor[18]>3)return -1;
 if(freeze)return -2;
 uint16_t expected=word(read,context,(uint16_t)(0x8200+(uint8_t)((actor[15]*2+actor[18])*2)));
 if((actor[2]|((uint16_t)actor[3]<<8))!=expected)return -1;
 static const uint16_t tables[4][8]={
  {0x97a2,0x97a9,0x97b3,0x97ba,0x97c4,0x97cb,0x97d5,0x97dc},
  {0x97e6,0x97f3,0x9803,0x9810,0x9820,0x982d,0x983d,0x984a},
  {0x985a,0x9870,0x988c,0x98a2,0x98be,0x98d4,0x98f0,0x9906},
  {0x9922,0x993e,0x9960,0x997c,0x999e,0x99ba,0x99dc,0x99f8}};
 uint8_t a[32],f=*flag;memcpy(a,actor,32);
 unsigned direction=a[18];int left=direction>=2,up=direction&1;
 uint8_t carry=spang_ball_phase_step(a,step,up,read,context,&f);
 if(carry)up=0;
 uint32_t x=a[9]|((uint32_t)a[10]<<8)|((uint32_t)a[11]<<16);
 x=(left?x-dx:x+dx)&0xffffff;a[9]=(uint8_t)x;a[10]=(uint8_t)(x>>8);a[11]=(uint8_t)(x>>16);
 uint16_t integer_x=(uint16_t)(x>>8);
 if(scratch_x){scratch_x[0]=(uint8_t)integer_x;scratch_x[1]=(uint8_t)(integer_x>>8);}
 unsigned column=(left?4:0)+(up?2:0);
 vertical(a,integer_x,tables[size][column],up,read,context,&f);
 column=(left?4:0)+(a[18]==(left?2:0)?0:2)+1;
 horizontal(a,integer_x,tables[size][column],left,read,context,f);
 a[27]=(uint8_t)((a[10]|((uint16_t)a[11]<<8))>>1);
 memcpy(actor,a,32);*flag=f;return 0;
}

/* Compatibility for existing C01 isolated probes; the full owner also
 * preserves the original E0A6 collision-coordinate scratch word. */
int spang_round_ball_update(uint8_t actor[32],uint16_t step,uint16_t dx,
 uint8_t freeze,spang_ball_read_fn read,void *context,uint8_t *flag)
{return spang_round_ball_update_full(actor,step,dx,freeze,read,context,flag,0);}
