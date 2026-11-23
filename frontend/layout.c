#include <stdint.h>
#include "layout.h"
#include "layout_data.h"

static int title(unsigned screen)
{return screen==2 || screen==3 || screen==6 || screen==10;}
static int floor_five_sixths(int value)
{int product=value*5;return product>=0?product/6:-((-product+5)/6);}
static int visible(int x,int y,int width,int height)
{return width>0 && height>0 && x<320 && y<240 && x+width>0 && y+height>0;}
static int text_cell(const frontend_text_placement *table,unsigned count,unsigned index,int *x,int *y)
{
 unsigned lo=0,hi=count;
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(table[mid].source<index)lo=mid+1;else hi=mid;}
 if(lo==count || table[lo].source!=index)return 0;
 *x=table[lo].x;*y=table[lo].y;return 1;
}
unsigned frontend_layout_backdrop(unsigned screen)
{return title(screen) || screen==11?0x20d:0;}
int frontend_layout_cell(unsigned screen,unsigned index,unsigned credits,
                         int *x,int *y,int *width,int *height)
{
 if(screen>12 || index>=2048 || !x || !y || !width || !height)return 0;
 (void)credits; /* The console prompt has one fixed width for either player. */
 unsigned col=index%64,row=index/64;
 *width=*height=8;*x=(int)col*8-96;*y=(int)row*8-8;
 if(screen==11 || screen==12)return visible(*x,*y,*width,*height);
 if(screen==10 && row>=26)return visible(*x,*y,*width,*height);
 if(screen==1)
  return text_cell(frontend_warning_text,sizeof(frontend_warning_text)/sizeof(frontend_warning_text[0]),index,x,y);
 if(screen==7){
  /* Original39-entry grid occupies304px. Keep its glyphs intact, center it,
   * and let the surrounding instructions begin at the original8px margin.
   * Rows24/25 include the documented native40th-stage extension. */
  *x=(int)col*8-((row>=14 && row<=25)?104:64);
  return visible(*x,*y,*width,*height);
 }
 if(screen==8 && ((row>=6 && row<=19) || row==22 || row==23)){
  /* Keep every original glyph1:1 and all score/name/progress columns aligned.
   * Remove the24px score/name gap and8px before the star. The64px TOTAL
   * label starts at72; its extra8px gap is omitted on that row only. */
  if(row==22 || row==23){
   if(col>=9 && col<17)*x=(int)col*8-72;
   else if(col>=18 && col<34)*x=(int)col*8-80;
   else if(col>=37 && col<43)*x=(int)col*8-104;
   else if(col>=44 && col<54)*x=(int)col*8-112;
   else return 0;
  }else if(col>=10 && col<34)*x=(int)col*8-80;
  else if(col>=37 && col<43)*x=(int)col*8-104;
  else if(col>=44 && col<54)*x=(int)col*8-112;
  else return 0;
  return visible(*x,*y,*width,*height);
 }
 if(title(screen)){
  if(row>=1 && row<15){
   /* SUPER and its star fit at the original pixel size. Center the complete
    *192px tile rectangle; no tile-local or VDP1 size conversion occurs. */
   if(col<22 || col>=46)return 0;
   *x=(int)col*8-96;*y=(int)row*8-8;
  }else if(row>=15 && row<27){
   /* The settled PANG field is painted from the same native authored masks
    * as its moving cells, at the controller's original commit milestones. */
   return 0;
  }else if(row==30 && col>=45 && col<=52)*x=(int)col*8-112;
  return visible(*x,*y,*width,*height);
 }
 if(screen==4 || screen==5){
  if(row>=16 && row<=26)
   return text_cell(frontend_mode_text,sizeof(frontend_mode_text)/sizeof(frontend_mode_text[0]),index,x,y);
  if(row>=6 && row<16){
   if(col>=10 && col<27)*x=(int)col*8-72;
   else if(col>=37 && col<54)*x=(int)col*8-120;
   else if(!(col>=30 && col<34 && row>=7 && row<12))return 0;
  }
 }
 return visible(*x,*y,*width,*height);
}
int frontend_layout_sprite(unsigned screen,int sx,int sy,unsigned layer,
                           int *x,int *y,int *width,int *height)
{
 if(screen>11 || layer>2 || !x || !y || !width || !height)return 0;
 {
  *x=sx-96;*y=sy-8;*width=*height=16;
  if((screen==4 || screen==5) && layer==2)*x=sx<256?sx-72:sx-120;
 }
 return visible(*x,*y,*width,*height);
}
static unsigned word(const unsigned char *p)
{return p[0]|((unsigned)p[1]<<8);}
int frontend_layout_title_object(const unsigned char p[32],
                                int *x,int *y,int *width,int *height,
                                unsigned *phase_x,unsigned *phase_y)
{
 if(!p || !x || !y || !width || !height || !phase_x || !phase_y ||
    !p[20] || p[20]>8 || (p[21]!=2 && p[21]!=4))return -1;
 int ox=(int16_t)word(p+24),oy=(int16_t)word(p+26);
 int ax=(int16_t)word(p+28),ay=(int16_t)word(p+30);
 if(oy<0 || oy>255 || ax<0 || ax>511 || ay<0 || ay>255)return -1;
 int sx=p[3]+((p[1]&16)<<4),sy=((p[2]+8)&255)-8;
 int dx=(sx-ox)&511,dy=(sy-oy)&255;
 if(dx>=256)dx-=512;
 if(dy>=128)dy-=256;
 if(dx<-64 || dx>64 || dy<-32 || dy>48)return -1;
 int cx=ax+dx-64,cy=ay+dy-8;
 int left=floor_five_sixths(cx);
 *width=floor_five_sixths(cx+16)-left;
 *height=16;
 /* sx-dx/ sy-dy are the actor origin on this hardware cell's wrapped page.
  * This preserves the original bottom/top wrap while the raster stays fixed. */
 *x=floor_five_sixths(sx-dx-64)+left-floor_five_sixths(ax-64);
 *y=sy-8;
 int px=cx%6,py=cy%6;
 *phase_x=(unsigned)(px<0?px+6:px);*phase_y=(unsigned)(py<0?py+6:py);
 return visible(*x,*y,*width,*height);
}
unsigned frontend_layout_title_sample(unsigned phase,unsigned pixel)
{
 return ((phase*5/6+pixel)*6/5)+1-phase;
}
