#include "spang_items.h"
#include <string.h>
static unsigned w(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t*p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned rw(const uint8_t*b,unsigned a){return w(b+(a-0x8000));}
static void state(uint8_t*a,unsigned pc){put(a+2,pc);}
static void finish(uint8_t*a){a[0]=0;state(a,0xb069);}
void spang_items_init(uint8_t pool[256]){unsigned i;memset(pool,0,256);for(i=0;i<8;i++){uint8_t*a=pool+i*32;a[1]=(uint8_t)i;a[16]=6;state(a,0xb069);}}
void spang_items_stage(uint8_t g[128],unsigned stage,const uint8_t b[16384]){if(stage>=40)return;put(g+0x3c,0xa164+stage*32);put(g+0x3e,rw(b,0x8a18+stage*2));put(g+0x42,rw(b,0x8a90+b[0x8a68-0x8000+stage]));g[0x44]=0;g[0x4d]=0;g[0x49]=0;g[0x4a]=0;}
int spang_items_spawn(uint8_t p[256],unsigned type,unsigned x,unsigned y){unsigned i;if(type>20||(type&1))return -1;for(i=0;i<8;i++,p+=32)if(!p[0]){p[0]=2;p[15]=(uint8_t)type;put(p+10,x);p[13]=(uint8_t)y;return (int)i;}return -1;}
int spang_items_spawn_platform(uint8_t p[256],unsigned index,unsigned x,unsigned y){int i;if(index>5)return -1;i=spang_items_spawn(p,0,x,y);if(i>=0){p[i*32+15]=(uint8_t)index;state(p+i*32,0xb04a);}return i;}
uint8_t spang_items_random(uint16_t *seed){unsigned old=*seed,n=(old*3)&65535;uint8_t a=(uint8_t)(old+(n>>8));*seed=(uint16_t)((n&0xff00)|a);return a;}
void spang_items_stop_start(uint8_t stop[6],uint8_t g[128],const uint8_t b[16384],uint8_t random){unsigned offset=b[0x1437+(random&31)],duration=rw(b,0x9459+offset);if(duration>w(stop+1))put(stop+1,duration);stop[0]=1;g[0x2b]=1;g[0x2c]=0;}
void spang_items_stop_tick(uint8_t stop[6],uint8_t g[128]){if(!stop[0])return;if(stop[0]==1){unsigned n=(w(stop+1)-1)&65535;put(stop+1,n);if(!n){stop[0]=2;stop[3]=4;stop[4]=8;g[0x2c]=1;}return;}if(--stop[3])return;if(stop[0]==2){g[0x2c]=0;stop[3]=8;stop[0]=3;}else if(!--stop[4]){g[0x2b]=g[0x2c]=0;stop[0]=0;}else{g[0x2c]=1;stop[3]=4;stop[0]=2;}}
void spang_items_slow_start(uint8_t slow[4],uint8_t g[128]){slow[0]=1;slow[1]=10;g[0x29]=g[0x28]>10?(uint8_t)(g[0x28]-10):1;if(g[0x29]>=g[0x22]){slow[0]=2;slow[1]=30;}}
int spang_items_slow_tick(uint8_t slow[4],uint8_t g[128]){if(!slow[0]||--slow[1])return 0;if(slow[0]==1){--g[0x22];slow[1]=10;if(g[0x22]<=g[0x29]){slow[0]=2;slow[1]=30;}return 1;}if(slow[0]==2){slow[0]=3;slow[1]=15;return 0;}++g[0x22];slow[1]=15;if(g[0x22]>=g[0x28]){g[0x22]=g[0x28];slow[0]=0;}return 1;}
int spang_items_drop(uint8_t p[256],uint8_t g[128],const uint8_t s[32],const uint8_t b[16384],uint8_t random,int alternate){unsigned type,addr;int i;
 if(alternate){addr=w(g+0x42)+g[0x44];g[0x44]=(g[0x44]+2)&0x3e;if(addr<0x8000||addr>=0xbfff)return -1;if((random&15)>=b[addr-0x8000]||b[addr+1-0x8000]<2)return -1;type=b[addr+1-0x8000]-2;}
 else {if(g[0x4d]&&!--g[0x4d]){i=spang_items_spawn(p,16,0,0);if(i>=0)g[0x4c]=2;return i;}if(g[0x3e]&&!--g[0x3e])type=g[0x3f];else{addr=w(g+0x3c)+(random&31);if(addr<0x8000||addr>0xbfff||!b[addr-0x8000])return -1;type=(uint8_t)(b[addr-0x8000]-2);}}
 return spang_items_spawn(p,type,w(s+10),s[13]);}
int spang_items_collect(uint8_t a[32],uint8_t p[128],uint8_t g[128]){unsigned t=a[15],dx=(w(a+10)+17-w(p+10))&65535;uint8_t dy=(uint8_t)(p[13]+17-a[13]);if(a[0]!=1||g[0x2d])return 0;if(t>20||(t&1))return -1;if(dx>=35||dy>=35)return 0;a[29]=0;if(p[82]==t){if(p[83]<3)p[83]++;a[29]=p[83];}else p[83]=0;p[82]=(uint8_t)t;a[24]=(uint8_t)(p[17]*2);a[0]=2;state(a,0xb121);
 if(t<8)p[87]=(uint8_t)((p[87]&8)+t);else if(t==10)g[0x2b]=1;else if(t==14){unsigned v=p[96]+1;if((v&15)>9)v+=6;p[96]=(uint8_t)v;}else if(t==16){p[0]=5;p[1]=1;p[35]=0;}else if(t==18||t==20){p[84]=2;p[0]=1;p[1]=1;p[35]=0;}return 1;}
static unsigned floorcell(const uint8_t*a,const uint8_t*grid){unsigned y=(uint8_t)(a[13]+8);return grid[((y>>3)*64+((w(a+10)&511)>>3))&2047];}
static unsigned smalls(const uint8_t*b){unsigned i,n=0;for(i=0;i<32;i++,b+=32)if(b[0]==1&&b[15]<8)n++;return n;}
static void split(uint8_t*b,uint8_t owner,int all){unsigned i;for(i=0;i<32;i++,b+=32)if(b[0]==1&&(all||b[15]>=8)){b[0]=2;b[27]=0;state(b,0x912e);b[24]=owner;}}
int spang_items_tick(uint8_t a[32],uint8_t g[128],const uint8_t grid[2048],uint8_t balls[1024],const uint8_t b[16384],uint8_t frame){unsigned pc=w(a+2),t=a[15],anim;int draw;if(!a[0])return 0;if(pc!=0xb04a&&(t>20||(t&1)))return -1;
 switch(pc){case 0xb04a:
 if(g[0x19]>=42||t>5)return -1;
 t=b[0xaf3d-0x8000+g[0x19]*6+t];a[15]=(uint8_t)t;
 if(t>20||(t&1))return -1;
 if(t==20){if(g[0x64]){finish(a);return 0;}g[0x64]++;}
 /* fall through */
 case 0xb041:case 0xb069:
 a[0]=1;a[30]=1;a[31]=0;a[18]=0;if(t==16){unsigned stage=g[0x19];if(stage>=40)return -1;put(a+10,rw(b,0xad66+stage*3));a[13]=b[0xad68-0x8000+stage*3];}if(t==14){if(g[0x49]){finish(a);return 0;}g[0x49]++;}anim=rw(b,0xb389+t);if(anim<0x8000||anim>0xbfff)return -1;put(a+6,anim);a[8]=b[anim-0x8000]&127;state(a,pc==0xb041?0xb0d0:0xb0a0);return 0;
 case 0xb0d0:return SPANG_ITEM_DRAW; /* Tutorial B041: fixed display until collected. */
 case 0xb0a0:put(a+12,w(a+12)+0x140);if(floorcell(a,grid)){a[13]=(uint8_t)(((uint8_t)(a[13]+8)&248)-8);state(a,0xb0d3);put(a+4,180);}return SPANG_ITEM_DRAW;
 case 0xb0d3:case 0xb0fc:draw=pc==0xb0d3||!(frame&4);if(!((frame+a[1])&3)&&!floorcell(a,grid)){state(a,0xb0a0);return draw;}
 if(!--a[4]){if(pc==0xb0d3){if(!a[5]--){a[4]=90;state(a,0xb0fc);}}else{a[0]=2;state(a,0xb348);}}return draw;
 case 0xb348:finish(a);return 0;
 case 0xb121:
 if(t<8){state(a,0xb348);return SPANG_ITEM_SOUND|SPANG_ITEM_SCORE;}if(t==8){state(a,0xb348);return SPANG_ITEM_SOUND|SPANG_ITEM_FREEZE|SPANG_ITEM_SCORE;}if(t==10){g[0x2b]=1;g[0x2c]=0;state(a,0xb348);return SPANG_ITEM_SOUND|SPANG_ITEM_SCORE|SPANG_ITEM_BOMB;}
 if(t==12){if(g[0x4a]){finish(a);return SPANG_ITEM_SOUND;}g[0x4a]++;a[28]=(uint8_t)(smalls(balls)+1);state(a,0xb1e7);return SPANG_ITEM_SOUND;}
 if(t==16){g[0x4e]=(uint8_t)(2+a[24]);a[19]=10;a[18]=5;state(a,0xb2f2);return SPANG_ITEM_SOUND;}
 state(a,0xb348);return SPANG_ITEM_SOUND|SPANG_ITEM_SCORE|(t==14?SPANG_ITEM_LIFE:0);
 case 0xb1e7:if(a[31]==4){a[30]=10;state(a,0xb237);return 0;}if(!--a[30]){a[31]++;a[30]=30;split(balls,a[24],0);}return 0;
 case 0xb237:if(!--a[30]){finish(a);return SPANG_ITEM_SPECIAL;}return 0;
 case 0xb2f2:if(!--a[19]){split(balls,a[24],1);a[19]=30;if(!--a[18])state(a,0xb348);}return 0;
 default:return -1;}
}

int spang_items_shield_contact(uint8_t p[128],uint8_t balls[1024],uint8_t frame,uint8_t blocked){
 static const uint8_t radius[5]={8,10,15,22,28},offset[5]={12,14,18,26,30},height[5]={28,32,40,56,64};unsigned n;
 if(p[0]>=3||blocked||!p[84])return 0;
 for(n=0;n<16;n++){unsigned slot=(n%8)*4+(n/8)*2+(frame&1);uint8_t*b=balls+slot*32;unsigned selector=b[28],r,x;if(b[0]!=1)continue;if(selector<1||selector>5)return -1;r=radius[selector-1];x=(w(b+10)+r-w(p+10))&65535;if(x>=r*2+1||(uint8_t)(b[13]+offset[selector-1]-p[13])>=height[selector-1])continue;p[0]=4;p[36]=210;p[35]=0;p[84]=4;b[0]=2;b[27]=0;state(b,0x912e);return 1;}return 0;}
void spang_items_shield_tick(uint8_t p[128],uint8_t frame){if(p[0]!=3&&p[0]!=4)return;if(!(frame&(p[0]==4?3:1)))p[1]^=1;if(--p[36])return;if(p[0]==4){p[0]=3;p[36]=30;}else{p[0]=1;p[1]=1;p[35]=0;}}
int spang_items_shield_sprite(uint8_t p[128],uint8_t frame,const uint8_t bank0[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining){
 static const uint8_t order[9]={0,8,16,17,9,1,2,10,18};unsigned table,desc,i,x,code,attr;
 if(!p[84])return 0;
 if(p[17]>1||p[84]>14||(p[84]&1))return -1;
 table=p[84]==2?0xbda6+(frame&12)+p[17]*2:0xbdae +p[84]*2+p[17]*2;
 desc=rw(bank0,table);if(desc<0x8000||desc>=0xbfff)return -1;
 if(p[84]!=2)p[84]=(p[84]+2)&14;
 if(*remaining<9)return 0;
 if((*cursor&31)||*cursor+288>4096)return -1;
 code=bank0[desc-0x8000];attr=bank0[desc+1-0x8000];x=(w(p+10)-24)&65535;
 for(i=0;i<9;i++){unsigned col=i/3,row=col==1?2-i%3:i%3,d=*cursor+i*32,xx=(x+col*16)&65535;objects[d]=(uint8_t)(code+order[i]);objects[d+1]=(uint8_t)(attr|((xx>>4)&16));objects[d+2]=(uint8_t)(p[13]-24+row*16);objects[d+3]=(uint8_t)xx;}
 *cursor+=288;*remaining-=9;return 9;}

uint16_t spang_items_special_bonus(const uint8_t a[32],const uint8_t balls[1024],const uint8_t bank1[16384]){
 unsigned count=smalls(balls),n,rate,lo=0,hi=0;if(count<a[28])return 0;count-=a[28];rate=bank1[0x3039+(count>>2)];
 for(n=0;n<=count;n++){unsigned sum=lo+rate,adjust=0;if(((lo&15)+(rate&15)>15)||(sum&15)>9)adjust=6;if(sum>0x99)adjust|=0x60;sum+=adjust;if(sum>255)hi++;lo=sum&255;}return (uint16_t)((hi<<8)|lo);}
void spang_items_stop_start_draw(uint8_t stop[6],uint8_t clock[32],uint8_t g[128],const uint8_t bank3[16384],uint8_t random){
 unsigned offset=bank3[0x1437+(random&31)],duration=rw(bank3,0x9459+offset),old=w(stop+1);spang_items_stop_start(stop,g,bank3,random);
 if(old>=duration)return;
 clock[0]=8;put(clock+6,rw(bank3,0x9457+offset));clock[8]=bank3[w(clock+6)-0x8000];put(clock+10,216);clock[13]=32;
}
int spang_items_stop_sprite(uint8_t clock[32],const uint8_t bank3[16384],uint8_t objects[4096],uint16_t *cursor,uint8_t *remaining){
 unsigned animation=w(clock+6),count,coords,i,x=w(clock+10);uint8_t y=clock[13],age=(uint8_t)(clock[8]-1);if(!clock[0])return 0;
 if(!age)animation+=14;
 /* Validate the complete next header before reading its duration/count/pointer. */
 if(animation<0x8000||animation>0xbffc)return -1;
 if(!age)age=bank3[animation-0x8000];
 count=bank3[animation+1-0x8000];coords=rw(bank3,animation+2);if(!count||count>64||coords<0x8000||coords+count*2>0xc000||animation+4+count*2>0xc000)return -1;
 put(clock+6,animation);clock[8]=age;
 if(*remaining<count)return 0;
 if((*cursor&31)||*cursor+count*32>4096)return -1;
 for(i=0;i<count;i++){unsigned dest=*cursor+i*32,payload=animation+4+i*2-0x8000;x=(x+(int8_t)bank3[coords+i*2-0x8000])&65535;y=(uint8_t)(y+(int8_t)bank3[coords+i*2+1-0x8000]);objects[dest]=bank3[payload];objects[dest+1]=(uint8_t)(bank3[payload+1]|((x>>4)&16));objects[dest+2]=y;objects[dest+3]=(uint8_t)x;}
 *cursor+=(uint16_t)(count*32);*remaining-=(uint8_t)count;return (int)count;
}

int spang_items_drop_rng(uint8_t pool[256],uint8_t g[128],const uint8_t source[32],const uint8_t bank1[16384],uint16_t *seed,int alternate){
 uint8_t random=0;
 if(alternate || (g[0x4d]!=1 && g[0x3e]!=1))random=spang_items_random(seed);
 return spang_items_drop(pool,g,source,bank1,random,alternate);
}

int spang_items_weapon_hud(unsigned type,unsigned owner,const uint8_t b[16384],spang_video_write write,void *context)
{
 if(type>6 || (type&1) || owner>1)return -1;
 unsigned ptr=rw(b,0xb125+type),dest=rw(b,0xb184+owner*2);
 if(ptr<0x8000 || ptr>0xbffd)return -1;
 unsigned code=rw(b,ptr+1),tile=(dest-0xd000)/2;
 for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){unsigned c=code+y*16+x,t=tile+y*64+x;
  write(context,(uint16_t)(0xd000+t*2),(uint8_t)c);
  write(context,(uint16_t)(0xd001+t*2),(uint8_t)(c>>8));
  write(context,(uint16_t)(0xc800+t),0x15);
 }return 0;
}
