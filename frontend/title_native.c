#include "title_native.h"
#include "title_native_data.h"

static unsigned pen(unsigned letter,int x,int y)
{
 const frontend_title_bitmap *b=frontend_title_bitmaps+letter;
 y-=b->y;
 if(y<0 || (unsigned)y>=b->height)return 0;
 unsigned row=(unsigned)y,start=b->offsets[row],end=b->offsets[row+1];
 x-=b->x+(int)b->left[row];
 if(x<0 || (unsigned)x>=(end-start)*2u)return 0;
 unsigned value=b->pixels[start+(unsigned)x/2u];
 return x&1?value&15:value>>4;
}
int frontend_title_native_actor(unsigned shape)
{
 static const uint16_t shapes[8]={0xaceb,0xad29,0xad67,0xadad,0xadf3,0xae3b,0xae83,0xaebf};
 for(unsigned i=0;i<8;i++)if(shapes[i]==shape)return (int)i;
 return -1;
}
int frontend_title_native_raster(unsigned actor,uint8_t *output,unsigned capacity,
                                 int *left,int *top,unsigned *width,unsigned *height)
{
 if(actor>=8 || !output || !left || !top || !width || !height)return 0;
 const frontend_title_bitmap *b=frontend_title_bitmaps+actor/2;
 unsigned w=0,h=b->height;
 for(unsigned row=0;row<h;row++){
  unsigned end=b->left[row]+2u*(b->offsets[row+1]-b->offsets[row]);
  if(end>w)w=end;
 }
 w=(w+7u)&~7u;
 if(actor&1u){unsigned limit=184u-(unsigned)b->y;if(h>limit)h=limit;}
 if(!w || !h || w*h/2>capacity)return 0;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x+=2){
  unsigned a=pen(actor/2,b->x+(int)x,b->y+(int)y);
  unsigned c=pen(actor/2,b->x+(int)x+1,b->y+(int)y);
  output[y*w/2+x/2]=(uint8_t)((a<<4)|c);
 }
 *left=b->x;*top=b->y;*width=w;*height=h;return 1;
}
int frontend_title_native_cell(unsigned code,unsigned px,unsigned py,
                               unsigned width,unsigned height,uint8_t *output)
{
 if(!output || px>=6 || py>=6 || width!=(px+16)*5/6-px*5/6 || height!=16)return 0;
 unsigned key=(code<<6)+py*6+px,lo=0,hi=sizeof(frontend_title_slices)/sizeof(frontend_title_slices[0]);
 while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(frontend_title_slices[mid].key<key)lo=mid+1;else hi=mid;}
 if(lo==sizeof(frontend_title_slices)/sizeof(frontend_title_slices[0]) || frontend_title_slices[lo].key!=key)return 0;
 const frontend_title_slice *s=frontend_title_slices+lo;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<16;x+=2){
  unsigned first=y<s->clip_height && x<width?pen(s->letter,s->x+(int)x,s->y+(int)y):0;
  unsigned second=y<s->clip_height && x+1<width?pen(s->letter,s->x+(int)x+1,s->y+(int)y):0;
  output[y*8+x/2]=(uint8_t)((first<<4)|second);
 }
 return 1;
}
int frontend_title_native_background(unsigned mask,int x,int y)
{
 if(!(mask&3) || x<0 || x>=320 || y<112 || y>=192)return -1;
 for(int letter=3;letter>=0;--letter)if(mask&(1u<<((unsigned)letter/2))){
  unsigned value=pen((unsigned)letter,x,y);
  if(value)return (0x20+letter)*16+(int)value-1;
 }
 for(int letter=3;letter>=0;--letter)if(mask&(1u<<((unsigned)letter/2)))
  if(pen((unsigned)letter,x-13,y-8))return 0x24e;
 return -1;
}
