#include <jo/jo.h>
#include "include/boot_diag.h"
/* No CD, BIOS, audio, input or heap dependency. The same NBG1 bitmap that
 * rt_init_art later reserves is acquired early; that reservation is idempotent.
 * This is instrumentation, not a claim that the hardware regression is fixed. */
volatile unsigned boot_diag_phase __attribute__((used,externally_visible));
volatile unsigned boot_diag_count __attribute__((used,externally_visible));
volatile unsigned char boot_diag_trace[64] __attribute__((used,externally_visible));
static unsigned ready,finished;
/* 5x7 loading font; VRAM and internal phase tracing remain independent. */
static const char glyph_names[]="Loading.PlesWt ";
static const unsigned char font[][7]={
 {16,16,16,16,16,16,31},{0,0,14,17,17,17,14},{0,0,14,1,15,17,15},
 {1,1,15,17,17,17,15},{4,0,12,4,4,4,14},{0,0,30,17,17,17,17},
 {0,0,15,17,15,1,14},{0,0,0,0,0,12,12},{30,17,17,30,16,16,16},
 {12,4,4,4,4,4,14},{0,0,14,17,31,16,14},{0,0,15,16,14,1,30},
 {17,17,17,21,21,21,10},{4,4,31,4,4,5,2},{0,0,0,0,0,0,0}
};
static void draw_text(const char *text,int y)
{
 volatile unsigned short *v=(volatile unsigned short *)JO_VDP2_VRAM_A0;
 unsigned length=0;while(text[length])length++;
 int x=(320-(int)length*18)/2;
 for(;*text;text++,x+=18){
  unsigned g=0;while(glyph_names[g] && glyph_names[g]!=*text)g++;
  if(!glyph_names[g])g=14;
  for(unsigned dy=0;dy<21;dy++)for(unsigned dx=0;dx<15;dx++)
   v[(y+dy)*512+x+dx]=(font[g][dy/3]&(16u>>(dx/3)))?0xffff:0x8800;
 }
}
void boot_diag_mark(unsigned phase)
{
 if(finished)return;
 boot_diag_phase=phase;
 if(boot_diag_count<sizeof boot_diag_trace)boot_diag_trace[boot_diag_count++]=(unsigned char)phase;
 if(!ready){
  /* The core has already initialized both VDPs and enabled interrupts. */
  if(!__jo_vdp2_reserve_nbg1_bitmap()){
   jo_set_default_background_color(JO_COLOR_Red);return;
  }
  jo_vdp2_zoom_nbg1f(1.0f,1.0f);jo_vdp2_move_nbg1(0,0);
  jo_set_displayed_screens(JO_NBG1_SCREEN);jo_core_tv_on();ready=1;
 }
 volatile unsigned short *v=(volatile unsigned short *)JO_VDP2_VRAM_A0;
 for(unsigned y=64;y<160;y++)for(unsigned x=32;x<288;x++)v[y*512+x]=0x8800;
 draw_text("Loading...",76);draw_text("Please Wait",112);
}
void boot_diag_finish(void){finished=1;boot_diag_phase=100;}
