#include "spang_panic_video.h"
#include "spang_panic_video_data.h"

static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static unsigned byte_set(uint8_t *destination,uint8_t value)
{if(*destination==value)return 0;*destination=value;return 1;}
static unsigned tile(uint8_t *chars,unsigned row,unsigned col,unsigned code)
{
 unsigned at=(row*64+col)*2;
 return byte_set(chars+at,(uint8_t)code)|byte_set(chars+at+1,(uint8_t)(code>>8));
}
static unsigned attributes(uint8_t *attrs,unsigned row,unsigned col,unsigned value)
{return byte_set(attrs+row*64+col,(uint8_t)value);}
static unsigned palette_copy(uint8_t *palette,unsigned offset,const uint8_t *source,unsigned size)
{
 unsigned changed=0;
 for(unsigned i=0;i<size;i++)changed|=byte_set(palette+offset+i,source[i]);
 return changed?SPANG_PANIC_VIDEO_PALETTE:0;
}
static unsigned digit(uint8_t *chars,unsigned col,unsigned number)
{
 unsigned code=le16(panic_digits+number*2),changed=0;
 for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)
  changed|=tile(chars,27+y,col+x,(code&0xff00)|((code+y*16+x)&255));
 return changed;
}
unsigned spang_panic_video_update(uint8_t g[128],uint8_t attrs[2048],
                                 uint8_t chars[4096],uint8_t palette[4096])
{
 unsigned dirty=0,head=le16(g+0x50),tail=le16(g+0x58);
 (void)attrs;
 /* FIXED3298 returns at100 before330E: retain99 for completion. */
 if(g[0x53]<100)
  dirty|=digit(chars,32,g[0x53]/10)|digit(chars,34,g[0x53]%10);
 /* FIXED1ED7: the descending copy starts at C77E (not C77F). Its one-byte
  * overlap and untouched final byte are retained exactly. A zero count here
  * means no pending ISR transfer, avoiding Z80 LDIR's zero-as65536 behavior. */
 if(head<=384 && tail<=383){
  if(g[0x55] && tail){
   for(unsigned i=0;i<tail;i++)if(byte_set(palette+0x77e - i,panic_gauge[382-i]))
    dirty|=SPANG_PANIC_VIDEO_PALETTE;
   g[0x58]=g[0x59]=0;
  }
  if(head){dirty|=palette_copy(palette,0x600,panic_gauge+(g[0x55]?384:0),head);g[0x50]=g[0x51]=0;}
 }
 return dirty;
}
unsigned spang_panic_video_init(uint8_t g[128],uint8_t attrs[2048],
                               uint8_t chars[4096],uint8_t palette[4096])
{
 unsigned dirty=0;
 /* FIXED1CB4 selects BANK13:800C for Panic instead of the Tour actor palette.
  * FIXED1C95 installs the gold Panic frame palette at8D2C. READY later replaces
  * bank0 through request22; backgrounds already came from the phase's scene. */
 dirty|=palette_copy(palette,0,panic_actor_palette,512);
 dirty|=palette_copy(palette,0x3e0,panic_flash_palette+64,32);
 /* The native scene loader supplies Tour score/lives plus background. Original
  * Panic leaves its weapon/time/world/mode lanes empty; retain only score/lives.
  * This adaptation is verified against normal-boot original Panic RAM. */
 for(unsigned row=27;row<31;row++)for(unsigned col=20;col<56;col++){
  dirty|=tile(chars,row,col,0x20);
  if(col<39)dirty|=attributes(attrs,row,col,0x10);
 }
 /* FIXED1864/18CA, omitting17DB because the exact original background is loaded. */
 for(unsigned col=8;col<56;col++){
  dirty|=attributes(attrs,1,col,0x1f)|attributes(attrs,26,col,0x1f);
  dirty|=tile(chars,1,col,col==8?0x0d02:col==55?0x0d03:0x0d05);
  dirty|=tile(chars,26,col,col==8?0x0d12:col==55?0x0d13:0x0d15);
 }
 for(unsigned row=2;row<26;row++){
  dirty|=attributes(attrs,row,8,0x1f)|attributes(attrs,row,55,0x1f);
  dirty|=tile(chars,row,8,0x0d04)|tile(chars,row,55,0x0d14);
 }
 /* FIXED34A6/34D4 gauge frame and its twelve palette segments. */
 for(unsigned row=0;row<2;row++)for(unsigned col=0;col<14;col++){
  unsigned code=0x6c5+row*16+(col==0?0:col==13?2:1);
  dirty|=tile(chars,29+row,25+col,code);
  dirty|=attributes(attrs,29+row,25+col,col==0 || col==13?0x30:0x2f+col);
 }
 /* BANK5:809B, two five-glyph rows sourced at8474. */
 for(unsigned row=0;row<2;row++)for(unsigned col=0;col<5;col++){
  dirty|=tile(chars,27+row,27+col,0x700|panic_label[row*5+col]);
  dirty|=attributes(attrs,27+row,27+col,0x12);
 }
 dirty|=spang_panic_video_update(g,attrs,chars,palette);
 return dirty;
}
unsigned spang_panic_video_palette(unsigned request,unsigned parameter,uint8_t palette[4096])
{
 if(request==20 && parameter<=6 && !(parameter&1)){
  unsigned address=le16(panic_flash_pointer+parameter);
  return palette_copy(palette,0x3e0,panic_flash_palette+(address-0x8cec),32);
 }
 if(request==22 && parameter<40)
  return palette_copy(palette,0,panic_ready_palette+panic_ready_selector[parameter]*16,32);
 return 0;
}
