#include "spang_tour_ending.h"
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static unsigned bcd_add(uint8_t *p,unsigned add)
{
 unsigned before=*p,sum=before+add,carry=sum>255;
 if((before&15)+(add&15)>9)sum+=6;
 if(carry || sum>0x99){sum+=0x60;carry=1;}
 *p=(uint8_t)sum;return carry;
}
static void score_add(uint8_t p[128],unsigned n)
{if(bcd_add(p+0x41,n))bcd_add(p+0x40,1);}
static unsigned decimal_bcd(unsigned n)
{unsigned tens=0;while(n>=10 && tens<9){n-=10;++tens;}return (tens*16+n)&255;}
static void queued(spang_tour_queue_fn fn,void *ctx,unsigned value)
{fn(ctx,value>>8,value&255);}
static uint16_t low_inc(uint16_t p){return (uint16_t)((p&0xff00)|((p+1)&255));}
static uint16_t glyph(uint16_t at,unsigned code,spang_video_write w,void *ctx)
{
 uint16_t p=at;unsigned lo=code&255,hi=code>>8;
 w(ctx,p,(uint8_t)lo);p=low_inc(p);w(ctx,p,(uint8_t)hi);p=low_inc(p);
 w(ctx,p,(uint8_t)(lo+1));p=low_inc(p);w(ctx,p,(uint8_t)hi);
 p=(uint16_t)(p+0x7d);
 w(ctx,p,(uint8_t)(lo+16));p=low_inc(p);w(ctx,p,(uint8_t)hi);p=low_inc(p);
 w(ctx,p,(uint8_t)(lo+17));p=low_inc(p);w(ctx,p,(uint8_t)hi);
 return (uint16_t)(p+0xff81);
}
static void glyph_attrs(uint16_t at,unsigned attr,spang_video_write w,void *ctx)
{
 uint16_t p=(uint16_t)(0xc800+((at&0xfff)>>1));
 w(ctx,p,(uint8_t)attr);p=low_inc(p);w(ctx,p,(uint8_t)attr);
 p=(uint16_t)(p+0x3f);w(ctx,p,(uint8_t)attr);p=low_inc(p);w(ctx,p,(uint8_t)attr);
}
static void clear_screen(spang_video_write w,void *ctx)
{
 /* FIXED109B ordered writes, including its final even byte DD80. */
 w(ctx,0xd081,0);w(ctx,0xd080,0x20);
 for(unsigned i=0;i<0xcff;++i)w(ctx,(uint16_t)(0xd082+i),(i&1)?0:0x20);
 w(ctx,0xc840,0x10);
 for(unsigned i=0;i<0x67f;++i)w(ctx,(uint16_t)(0xc841+i),0x10);
}
static int label(uint8_t a[40],unsigned row,const uint8_t b[16384],
 const uint8_t digits[20],unsigned attr,spang_tour_queue_fn q,
 spang_video_write w,void *ctx)
{
 const uint8_t *s=b+0x9d0;unsigned count=*s++,palette=*s++,hi=*s++;
 if(!count || count>64)return -1;
 uint16_t dest=row?0xdca4:0xda24,ap=row?0xce52:0xcd12;
 for(unsigned y=0;y<2;++y){
  uint16_t p=(uint16_t)(dest+y*128),pa=(uint16_t)(ap+y*64);
  for(unsigned x=0;x<count;++x){w(ctx,p,*s++);p=low_inc(p);w(ctx,p,(uint8_t)hi);p=low_inc(p);w(ctx,pa,(uint8_t)palette);pa=low_inc(pa);}
 }
 queued(q,ctx,row?0x87e:0x87c);
 a[31]=0;uint16_t p=row?0xdcb6:0xda36;
 p=glyph(p,word(digits+2*(a[33]>>4)),w,ctx);
 glyph(p,word(digits+2*(a[33]&15)),w,ctx);
 queued(q,ctx,row?0x882:0x880);
 a[31]=0;p=row?0xdcc2:0xda42;
 for(unsigned n=0;n<2;++n){
  unsigned digit=n?(a[33]&15):(a[33]>>4);
  if(!digit && !a[31]){p=(uint16_t)((p&0xff00)|((p+4)&255));continue;}
  ++a[31];uint16_t at=p;p=glyph(p,word(digits+2*digit),w,ctx);glyph_attrs(at,attr,w,ctx);
 }
 queued(q,ctx,row?0x886:0x884);return 0;
}
static int image(uint8_t a[40],unsigned table,unsigned chars,unsigned attrs,
 const uint8_t b[16384],spang_video_write w,void *ctx)
{
 unsigned cols=a[34],rows=a[35],source=table-0x8000;
 if(!cols || !rows || source+cols*rows*3>16384)return -1;
 uint16_t ah=(uint16_t)attrs,ad=(uint16_t)(attrs+64),ch=(uint16_t)chars,cd=(uint16_t)(chars+128);
 for(unsigned y=0;y<rows;++y){
  a[33]=(uint8_t)cols;
  do{
   unsigned palette=b[source++],lo=b[source++],hi=b[source++];
   w(ctx,ah,(uint8_t)palette);ah=low_inc(ah);w(ctx,ah,(uint8_t)palette);ah=low_inc(ah);
   w(ctx,ad,(uint8_t)palette);ad=low_inc(ad);w(ctx,ad,(uint8_t)palette);ad=low_inc(ad);
   unsigned right=(lo+((palette&128)?255:1))&255;
   unsigned bottom=(lo+16)&255,bottom_right=(right+16)&255;
   w(ctx,ch,(uint8_t)lo);ch=low_inc(ch);w(ctx,ch,(uint8_t)hi);ch=low_inc(ch);
   w(ctx,ch,(uint8_t)right);ch=low_inc(ch);w(ctx,ch,(uint8_t)hi);ch=low_inc(ch);
   w(ctx,cd,(uint8_t)bottom);cd=low_inc(cd);w(ctx,cd,(uint8_t)hi);cd=low_inc(cd);
   w(ctx,cd,(uint8_t)bottom_right);cd=low_inc(cd);w(ctx,cd,(uint8_t)hi);cd=low_inc(cd);
  }while(--a[33]);
  ah=(uint16_t)(ah+word(a+36));ad=(uint16_t)(ad+word(a+36));
  ch=(uint16_t)(ch+word(a+38));cd=(uint16_t)(cd+word(a+38));
 }
 return 0;
}
static int allocate(uint8_t pool[320],uint8_t g[128],uint8_t a[40],unsigned p2)
{
 /* BANK3 caller enters an empty ending pool. Zero free count is outside
  * that original invariant; reject before touching another actor's RAM. */
 if(!g[0x1e])return -1;
 --g[0x1e];
 for(unsigned i=0;i<8;++i){
  uint8_t *child=pool+i*40;if(child[0])continue;
  put(a+(p2?38:36),0xfa00+i*40);child[17]=0x12;child[15]=p2?4:2;
  child[0]=2;child[10]=0x74;child[13]=(p2 && !a[30])?0xc8:0xa0;return 0;
 }
 return 0;
}
static int retire_children(uint8_t pool[320],uint8_t a[40])
{
 for(unsigned i=36;i<=38;i+=2){unsigned p=word(a+i);if(!p)continue;
  if(p<0xfa00 || p>=0xfb40 || (p-0xfa00)%40)return -1;
  pool[p-0xfa00+2]=4;
 }
 return 0;
}
static void wait_page(uint8_t a[40],const uint8_t in[4],const uint8_t p1[128],const uint8_t p2[128])
{
 a[31]=0;if(--a[32])return;a[32]=1;
 if((p1[0x60] && (in[0]&in[1])) || (p2[0x60] && (in[2]&in[3]))){a[31]=2;return;}
 if(!--a[19])a[31]=1;
}
static int icon(uint8_t a[40],uint8_t g[128],const uint8_t b[16384],spang_video_write w,void *ctx)
{
 if(a[2]==4){a[0]=a[2]=0;return 0;}
 if(!a[2]){a[2]=2;a[0]=1;put(a+6,a[15]==2?0x9b26:0x9b2b);a[8]=b[word(a+6)-0x8000]&127;}
 if(a[2]!=2)return -1;
 unsigned frame=word(a+6),count=a[15]==2?6:4;
 if(frame<0x8000 || frame+4>=0xc000)return -1;
 const uint8_t *source=b+(frame-0x8000);
 if(word(source+1)!=(a[15]==2?0x246f:0x21ad))return -1;
 if(g[0x14]<count)return 0;
 unsigned cursor=word(g+0x10);if((cursor&31)||cursor<0xd000||cursor+count*32>0xdfe0)return -1;
 g[0x14]-=(uint8_t)count;
 static const uint8_t deltas[6]={0,8,9,1,2,10};
 for(unsigned i=0;i<count;++i)w(ctx,(uint16_t)(cursor+i*32),(uint8_t)(source[3]+deltas[i]));
 unsigned x=(word(a+10)-(count==6?24:16))&65535,y=(a[13]-16)&255,attr=source[4];
 for(unsigned column=0;column<count/2;++column){
  unsigned i=column*2,first=cursor+i*32;attr|=(x>>4)&16;
  w(ctx,(uint16_t)(first+1),(uint8_t)attr);w(ctx,(uint16_t)(first+2),(uint8_t)y);w(ctx,(uint16_t)(first+3),(uint8_t)x);
  first+=32;w(ctx,(uint16_t)(first+3),(uint8_t)x);y=(y+((column&1)?240:16))&255;
  w(ctx,(uint16_t)(first+2),(uint8_t)y);w(ctx,(uint16_t)(first+1),(uint8_t)attr);x=(x+16)&65535;
 }
 put(g+0x10,cursor+count*32);return 0;
}
int spang_tour_ending_step(uint8_t a[40],uint8_t pool[320],uint8_t g[128],
 uint8_t p1[128],uint8_t p2[128],const uint8_t in[4],uint8_t extra[4],
 const uint8_t b[16384],const uint8_t digits[20],
 spang_tour_queue_fn q,spang_video_write w,void *ctx,uint8_t events[2])
{
 if(!a||!pool||!g||!p1||!p2||!in||!extra||!b||!digits||!q||!w||!events)return -1;
 events[0]=events[1]=0;
 if(a[15])return a[15]==2||a[15]==4?icon(a,g,b,w,ctx):-1;
 if(!a[2]){
  p1[0x4a]=p2[0x4a]=10;a[30]=1;
  for(unsigned i=34;i<40;++i)a[i]=0;
  extra[0]=0x10;
  for(unsigned i=0;i<p1[0x60];++i)score_add(p1,0x10);
  for(unsigned i=0;i<p2[0x60];++i)score_add(p2,0x10);
  a[19]=60;a[2]=2;if(g[0x1a])a[3]=2;return 0;
 }
 if(a[2]==4){a[2]=a[3]=a[4]=g[14]=0;events[1]=1;return 0;}
 if(a[2]!=2 || (a[4]&1))return -1;
 if(!a[3]){
  if(a[4]>18)return -1;
  if(a[4]!=2 && --a[19])return 0;
  switch(a[4]){
   case 0:a[4]=2;events[0]=0x36;return 0;
   case 2:a[4]=4;a[19]=40;queued(q,ctx,0x878);return 0;
   case 4:a[4]=6;a[19]=40;queued(q,ctx,0x644);return 0;
   case 6:a[4]=8;a[19]=30;queued(q,ctx,0x87a);return 0;
   case 8:if(p1[0])bcd_add(p1+0x40,1);if(p2[0])bcd_add(p2+0x40,1);a[4]=10;a[19]=30;return 0;
   case 10:if(extra[1]||extra[2]){a[4]=12;a[19]=40;queued(q,ctx,0x646);}else{a[4]=18;a[19]=255;}return 0;
   case 12:
    if(p1[0x60]&&extra[1]){
     a[33]=a[34]=(uint8_t)decimal_bcd(extra[1]);a[30]=0;
     if(allocate(pool,g,a,0)||label(a,0,b,digits,extra[0],q,w,ctx))return -1;
     a[19]=40;
    }else a[19]=1;
    a[4]=14;return 0;
   case 14:
    if(p2[0x60]&&extra[2]){
     a[33]=a[35]=(uint8_t)decimal_bcd(extra[2]);
     if(allocate(pool,g,a,1)||label(a,a[30]?0:1,b,digits,extra[0],q,w,ctx))return -1;
     a[19]=40;
    }else a[19]=1;
    a[4]=16;return 0;
   case 16:
    if(p1[0x60]&&a[34])score_add(p1,a[34]);
    if(p2[0x60]&&a[35])score_add(p2,a[35]);
    a[19]=110;a[4]=18;return 0;
   case 18:a[3]=2;a[4]=0;return retire_children(pool,a);
  }
 }
 if(a[3]!=2 || a[4]>16)return -1;
 switch(a[4]){
  case 0:
   clear_screen(w,ctx);a[36]=a[37]=a[38]=a[39]=0;a[4]=2;events[0]=0x37;return 0;
  case 2:
   a[32]=30;a[19]=210;clear_screen(w,ctx);a[34]=8;a[35]=8;a[36]=0x70;a[38]=0xe0;a[4]=4;
   return image(a,0x9bf5,0xd440,0xca20,b,w,ctx);
  case 4:a[4]=6;queued(q,ctx,0x63c);return 0;
  case 6:
  case 10:{
   unsigned state=a[4];wait_page(a,in,p1,p2);if(!a[31])return 0;
   if(a[31]<2){a[19]=240;a[32]=1;}else{a[32]=30;a[19]=210;}
   clear_screen(w,ctx);
   if(state==6){a[34]=8;a[35]=8;a[36]=0x70;a[38]=0xe0;a[4]=8;return image(a,0x9cb5,0xd41e,0xca0f,b,w,ctx);}
   a[34]=12;a[35]=7;a[36]=0x68;a[38]=0xd0;a[4]=12;return image(a,0x9d75,0xd328,0xc994,b,w,ctx);
  }
  case 8:a[4]=10;queued(q,ctx,0x63e);return 0;
  case 12:
   a[4]=14;
   if(extra[3])queued(q,ctx,g[0x1a]?0x668:0x666);
   else{queued(q,ctx,g[0x1a]?0x84e:0x84c);queued(q,ctx,0x850);}return 0;
  case 14:
   wait_page(a,in,p1,p2);if(!a[31])return 0;
   if(a[31]<2)a[2]=4;else{a[19]=60;a[4]=16;}return 0;
  case 16:if(!--a[19])a[2]=4;return 0;
 }
 return -1;
}
