#include "core.h"
#include <string.h>
/* All CPU addresses below refer to separately decrypted DATA, not opcodes. */
static uint8_t rb(spfe_t *f,unsigned bank,unsigned p) {
 if(bank>15 || p>=0xc000){f->error=1;return 0;}
 const uint8_t *source=p<0x8000?f->fixed:f->banks[bank];
 if(!source){f->error=1;return 0;}
 return source[p<0x8000?p:p-0x8000];
}
static uint16_t rw(spfe_t *f,unsigned bank,unsigned p){return (uint16_t)(rb(f,bank,p)|(rb(f,bank,p+1)<<8));}
static void tile(spfe_t *f,unsigned p,uint16_t code,uint8_t color) {
 if(p<0xd000||p>=0xe000||(p&1)){f->error=2;return;}
 unsigned i=(p-0xd000)/2;
 if(f->tiles[i]!=code||f->attributes[i]!=color){f->tiles[i]=code;f->attributes[i]=color;f->background_serial++;}
}
static void attr(spfe_t *f,unsigned p,uint8_t c) {
 if(p<0xc800||p>=0xd000){f->error=3;return;}
 unsigned i=p-0xc800;if(f->attributes[i]!=c){f->attributes[i]=c;f->background_serial++;}
}
static void clear(spfe_t *f,uint16_t code,uint8_t color) {
 for(unsigned i=0;i<2048;i++){f->tiles[i]=code;f->attributes[i]=color;}
 memset(f->actor,0,sizeof f->actor);f->sprite_count=0;f->background_serial++;
}
static void pal_copy(spfe_t *f,unsigned bank,unsigned ptr,unsigned dst,unsigned words) {
 if(dst+words>2048){f->error=4;return;}
 for(unsigned i=0;i<words;i++)f->palette[dst+i]=rw(f,bank,ptr+i*2);
 f->palette_serial++;
}
static void pal_boot(spfe_t *f) {
 memset(f->palette,0,sizeof f->palette);pal_copy(f,12,0x8268,0,0x2f0);
 pal_copy(f,13,0x85ac,0x400,0x3ff);f->palette[0x7ff]=rb(f,13,0x8daa);f->palette_serial++;
}
static void pal_title(spfe_t *f) {
 pal_copy(f,13,rw(f,13,0x8000),0x200,0x90);pal_copy(f,13,0x94cc,0x10,0xf0);
 /* The native Saturn prompt uses the original small-font strokes. Its matte
  * and hidden blink match the title field, including spaces between words. */
 f->palette[0x27a]=f->palette[0x27b]=f->palette[0x27d]=f->palette[0x27e]=f->palette[0x20d];
 for(unsigned i=0;i<16;++i)f->palette[0x280+i]=f->palette[0x20d];
 ++f->palette_serial;
}
static void text_small_bank(spfe_t *f,unsigned bank,unsigned p) {
 unsigned guard=0;
 while(p<0xc000 && ++guard<2048) {
  unsigned dst=rw(f,bank,p);uint8_t c=rb(f,bank,p+2);p+=3;
  for(;;){uint8_t v=rb(f,bank,p++);if(f->error||!v)return;if(v==0x2f)break;
   tile(f,dst,v,c);dst=(dst&0xff00)|((dst+2)&255);if(++guard>=2048){f->error=5;return;}}
 }
 f->error=5;
}
static void text_small(spfe_t *f,unsigned p){text_small_bank(f,10,p);}
static void block16(spfe_t *f,unsigned dst,uint8_t lo,uint8_t hi,uint8_t color) {
 tile(f,dst,(uint16_t)(lo|(hi<<8)),color);
 tile(f,dst+2,(uint16_t)((uint8_t)(lo+1)|(hi<<8)),color);
 tile(f,dst+128,(uint16_t)((uint8_t)(lo+16)|(hi<<8)),color);
 tile(f,dst+130,(uint16_t)((uint8_t)(lo+17)|(hi<<8)),color);
}
static void text_large_bank(spfe_t *f,unsigned bank,unsigned p) {
 for(unsigned guard=0;guard<1024;) {
  unsigned dst=rw(f,bank,p);uint8_t color=rb(f,bank,p+2);p+=3;
  for(;;){uint8_t lo=rb(f,bank,p++),hi=rb(f,bank,p++);if(f->error||hi==255)return;
   if(hi==254)break;
   block16(f,dst,lo,hi,color);dst+=4;if(++guard>=1024){f->error=6;return;}}
 }
 f->error=6;
}
static void text_large(spfe_t *f,unsigned p){text_large_bank(f,10,p);}
static void command(spfe_t *f,unsigned kind,unsigned index) {
 if(kind==6)text_small(f,rw(f,10,0x8000+index));
 else if(kind==8)text_large(f,rw(f,10,0x80a0+index));
 else f->error=7;
}
static void sound_event(spfe_t *f,uint8_t kind,uint8_t value,uint8_t parameter) {
 if(f->sound_count>=sizeof f->sounds){f->error=10;return;}
 unsigned n=f->sound_count++;
 f->sounds[n]=value;f->sound_kinds[n]=kind;f->sound_parameters[n]=parameter;
}
static void sound(spfe_t *f,uint8_t v){sound_event(f,SPFE_SOUND_COMMAND,v,0);}

static void add_sprite(spfe_t *f,uint16_t code,int x,int y,uint8_t pal,uint8_t layer) {
 if(f->sprite_count>=127)return; /* Original budget, final OBJ RAM entry excluded. */
 spfe_sprite_t *s=&f->sprites[f->sprite_count++];s->code=code;s->x=(int16_t)(x&511);
 s->y=(int16_t)(((y+8)&255)-8);s->palette=pal&15;s->layer=layer;s->actor_group=0;
}
static void metasprite(spfe_t *f,const spfe_actor_t *a) {
 unsigned p=a->shape,n=rb(f,3,p+1),dp=rw(f,3,p+2);int x=(int)(a->x>>8),y=a->y>>8;p+=4;
 if(n>127u-f->sprite_count)return;
 while(n--){uint8_t lo=rb(f,3,p++),at=rb(f,3,p++);x+=(int8_t)rb(f,3,dp++);y+=rb(f,3,dp++);
  add_sprite(f,(uint16_t)(lo+((at&0xe0)<<3)),x,y,at,0);
  f->sprites[f->sprite_count-1].actor_group=(uint8_t)(a-f->actor+1);}
}
static void title_cells(spfe_t *f,unsigned selector) {
 if(selector==0)f->title_settled|=1;
 else if(selector==2)f->title_settled|=2;
 unsigned p=rw(f,3,0xaffc+selector),dst=rw(f,3,0xb2f6+selector*2)-128;
 unsigned rows=rb(f,3,0xaff3+selector/2);
 for(unsigned row=0;row<rows;row++)for(unsigned col=0;col<12;col++,p+=3)
  block16(f,dst+row*256+col*4,rb(f,3,p+1),rb(f,3,p+2),rb(f,3,0xaff6+selector+(col>=5)));
}
static void title_extra(spfe_t *f) {
 const unsigned dst[4]={0xdac0,0xdbc0,0xdcc0,0xdcc4};attr(f,0xcc92,0x20);
 for(unsigned i=0;i<4;i++)block16(f,dst[i],rb(f,3,0xabb8+i*3),rb(f,3,0xabb9+i*3),0x21);
}
static void launch(spfe_t *f,unsigned id) {
 memset(f->actor,0,sizeof f->actor);unsigned p=rw(f,5,0x9e02+id*2),n=rb(f,5,p++);
 if(n>8){f->error=8;return;}
 for(unsigned i=0;i<n;i++){f->actor[i].active=1;f->actor[i].type=rb(f,5,p++);f->actor[i].variant=rb(f,5,p++);}
}
static void move_set(spfe_t *f,spfe_actor_t *a,int initial) {
 unsigned p=a->motion;
 if(initial){a->x=(int32_t)rw(f,3,p)<<8;a->y=(uint16_t)(rb(f,3,p+2)<<8);p+=3;}
 a->vx=(int16_t)rw(f,3,p);a->vy=(int16_t)rw(f,3,p+2);a->count=rb(f,3,p+4);a->motion=(uint16_t)(p+5);
}
static void title_letter(spfe_t *f,spfe_actor_t *a) {
 if(!a->state){a->state=2;a->delay=rw(f,3,0xa9bf+a->variant);a->motion=rw(f,3,0xabf3+a->variant);
  a->x=(int32_t)rw(f,3,a->motion)<<8;a->y=(uint16_t)(rb(f,3,a->motion+2)<<8);a->motion+=3;
  a->shape=rw(f,3,0xacdb+a->variant);return;}
 if(!a->sub){if(--a->delay==0){a->sub=2;move_set(f,a,0);}return;}
 if(a->sub<=6){a->x=(a->x+a->vx)&0xffffff;a->y=(uint16_t)(a->y+a->vy);
  if(--a->count==0){a->sub+=2;move_set(f,a,1);}metasprite(f,a);return;}
 if(a->sub==8){
  if(a->variant==6){title_cells(f,0);title_extra(f);for(unsigned i=4;i<8;i++)f->actor[i].active=0;}
  if(a->variant==14){a->sub=10;a->count=60;title_cells(f,2);f->actor[0].active=f->actor[2].active=f->actor[3].active=0;}
  metasprite(f,a);return;
 }
 if(a->sub==10 && --a->count==0){a->active=0;
  /* AAB6 uses 1615 (first free slot), variants 8,6,4,2,0. */
  for(int v=8;v>=0;v-=2)for(unsigned i=0;i<8;i++)if(!f->actor[i].active){memset(&f->actor[i],0,sizeof f->actor[i]);f->actor[i].active=1;f->actor[i].type=4;f->actor[i].variant=(uint8_t)v;break;}
 }
}
static void super_letter(spfe_t *f,spfe_actor_t *a) {
 if(!a->state){a->state=2;a->count=43;a->x=248<<8;a->y=65<<8;a->vx=(int16_t)rw(f,3,0xb3c1+a->variant);a->shape=rw(f,3,0xb3cb+a->variant);
  if(f->state==SPFE_CREDIT_TITLE)a->sub=6;
  return;}
 if(a->state==2){
  if(a->sub==0||a->sub==6){do {a->x=(a->x+a->vx)&0xffffff;a->count--;}while(a->sub==6&&a->count);
   if(!a->count){a->sub=2;if(a->variant||f->state==SPFE_CREDIT_TITLE)a->state=4;}}
  else if(a->sub==2){a->sub=4;a->count=36;command(f,8,0x0c);title_cells(f,4);}
  else if(a->sub==4 && --a->count==0)a->state=4;
 }else if(a->state==4)a->state=6;
 metasprite(f,a);
}
static void footer_set(spfe_t *f,spfe_actor_t *a){a->x=448<<8;a->y=224<<8;a->shape=rw(f,3,0xa2ff+a->variant);}
static void footer(spfe_t *f,spfe_actor_t *a) {
 if(!a->state){a->state=2;footer_set(f,a);unsigned p=rw(f,3,0xa1cd+4);a->count=(uint8_t)(rb(f,3,p+a->variant/2)+1);return;}
 if(a->state==2){if(--a->count)return;a->state=4;}
 if(a->state==4){int x=(a->x>>8)+(int16_t)rw(f,3,0xa1e1+4);
  if(x<0){a->variant+=14;if(a->variant>=0x49){a->active=0;return;}footer_set(f,a);x=448;}
  a->x=x<<8;
  if(a->variant>=0x44){const int stop[3]={160,224,288};if(x<stop[(a->variant-0x44)/2])a->state=6;}}
 unsigned p=a->shape,n=rb(f,3,p++);int x=a->x>>8;
 if(n>127u-f->sprite_count)return;
 while(n--){uint8_t lo=rb(f,3,p++),at=rb(f,3,p++);add_sprite(f,(uint16_t)(lo+((at&0xe0)<<3)),x,a->y>>8,at,1);x+=16;}
}
/* BANK3:8D91..8E83, the two running characters below the ranking table. */
static void runner_set(spfe_t *f,spfe_actor_t *a,unsigned table)
{a->motion=rw(f,3,table+a->variant);a->count=rb(f,3,a->motion)&127;}
static void runner_step(spfe_t *f,spfe_actor_t *a)
{
 a->x=(a->x+a->vx)&0xffffff;
 if(--a->count==0){unsigned p=a->motion+5;if(rb(f,3,a->motion)&128)p=rw(f,3,p);
  a->motion=(uint16_t)p;a->count=rb(f,3,p)&127;}
}
static void runner(spfe_t *f,spfe_actor_t *a)
{
 if(!a->state){a->state=2;a->sub=0;a->x=468<<8;a->y=224<<8;a->vx=-512;
  a->delay=rw(f,3,0x8e7f+a->variant);runner_set(f,a,0x8e77);return;}
 if(a->state==4){a->active=0;a->state=a->sub=0;return;}
 if(a->sub==0){if(--a->delay==0){a->sub=2;a->phase=(uint8_t)rw(f,3,0x8e83+a->variant);}}
 else if(a->sub==2){runner_step(f,a);if(((unsigned)(a->x>>8)&255)<a->phase){a->sub=4;a->delay=60;runner_set(f,a,0x8e7b);}}
 else if(a->sub==4){if(--a->delay==0){a->sub=6;runner_set(f,a,0x8e77);}}
 else if(a->sub==6){runner_step(f,a);if((unsigned)(a->x>>8)<30)a->state=4;}
 unsigned lo=rb(f,3,a->motion+3),at=rb(f,3,a->motion+4),code=lo+((at&0xe0)<<3);
 int x=(a->x>>8)-16,y=(a->y>>8)-16;
 add_sprite(f,(uint16_t)code,x,y,(uint8_t)at,1);
 add_sprite(f,(uint16_t)(code+8),x,y+16,(uint8_t)at,1);
 add_sprite(f,(uint16_t)(code+9),x+16,y+16,(uint8_t)at,1);
 add_sprite(f,(uint16_t)(code+1),x+16,y,(uint8_t)at,1);
}
static void actors(spfe_t *f) {
 f->sprite_count=0;
 for(unsigned i=0;i<8;i++)if(f->actor[i].active){spfe_actor_t *a=&f->actor[i];
  switch(a->type){case 2:title_letter(f,a);break;case 4:super_letter(f,a);break;case 12:footer(f,a);break;case 14:runner(f,a);break;case 6:case 16:case 22:case 26:break;default:f->error=9;}}
}
static void large_word(spfe_t *f,unsigned *dst,unsigned code,unsigned color)
{block16(f,*dst,(uint8_t)code,(uint8_t)(code>>8),(uint8_t)color);*dst+=4;}
static void record_digit(spfe_t *f,unsigned *dst,unsigned digit,unsigned color,unsigned *seen)
{
 if(!digit && !*seen){*dst+=4;return;}
 ++*seen;large_word(f,dst,rw(f,0,0x152e + digit*2),color);
}
static void record_byte(spfe_t *f,unsigned *dst,unsigned value,unsigned color,unsigned *seen)
{record_digit(f,dst,value>>4,color,seen);record_digit(f,dst,value&15,color,seen);}
static void record_row(spfe_t *f,const uint8_t row[16],unsigned index,int total)
{
 unsigned dst=total?0xdb24:rw(f,0,0xa3ea+index*2);
 unsigned style=total?0xa41c:0xa3f4+row[0]*8,color=rb(f,0,style+6),seen=0;
 if(!total){for(unsigned i=0;i<3;++i)large_word(f,&dst,rw(f,0,style+i*2),color);dst+=4;}
 for(unsigned i=1;i<5;++i)record_byte(f,&dst,row[i],color,&seen);
 dst+=6;
 for(unsigned i=0;i<3;++i)large_word(f,&dst,(unsigned)(row[6+i*2]|(row[7+i*2]<<8)),color);
 dst+=2;
 if(row[12])dst+=4;else large_word(f,&dst,0x380,rb(f,0,style+7));
 if(f->mode==SPFE_TOUR){record_digit(f,&dst,row[13]&15,color,&seen);large_word(f,&dst,0xa8a,color);}
 else{large_word(f,&dst,0xa6,color);large_word(f,&dst,0xaaa,color);}
 record_byte(f,&dst,row[14],color,&seen);
}
void spfe_open_records(spfe_t *f,const spang_records *records,unsigned mode)
{
 if(!f || !records || (mode!=SPFE_TOUR && mode!=SPFE_PANIC))return;
 clear(f,0x70e,0x38);f->state=SPFE_RECORDS;f->state_ticks=0;f->hold=300;f->mode=(uint8_t)mode;
 /* FIXED1F14, request1C; the remaining font colors retain the boot palette. */
 pal_boot(f);pal_copy(f,13,mode==SPFE_TOUR?0x96ec:0x980c,0x300,0xa0);
 pal_copy(f,13,0x824c,0x50,0x10);pal_copy(f,12,0x8268,0,0x40);
 command(f,8,0);command(f,8,0x0e);command(f,8,mode==SPFE_TOUR?0x8e:0x90);
 unsigned m=mode==SPFE_PANIC;
 for(unsigned n=0;n<2;++n){f->record_row[n]=255;f->record_phase[n]=f->record_timer[n]=0;
  for(unsigned i=0;i<5;++i)if(records->rows[m][i*16+15]==n+1){f->record_row[n]=(uint8_t)i;break;}}
 for(unsigned i=0;i<5;++i)record_row(f,records->rows[m]+i*16,i,0);
 record_row(f,records->total[m],0,1);launch(f,4);
}
void spfe_tick_records(spfe_t *f,const spang_records *records)
{
 if(!f || !records || f->state!=SPFE_RECORDS)return;
 ++f->ticks;++f->state_ticks;f->sound_count=0;actors(f);
 /* BANK0:A241/A281 copies the blank D214 row during the six-tick blink. */
 for(unsigned n=0;n<2;++n)if(f->record_row[n]<5){
  if(!f->record_phase[n]){f->record_phase[n]=2;f->record_timer[n]=10;}
  else if(--f->record_timer[n]==0){unsigned row=f->record_row[n];
   if(f->record_phase[n]==2){
    f->record_phase[n]=4;f->record_timer[n]=6;unsigned dst=rw(f,0,0xa3ea+row*2);
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<44;++x){unsigned p=dst+y*128+x*2,i=(p-0xd000)/2;
     tile(f,p,f->tiles[(0xd214-0xd000)/2+x],f->attributes[i]);}
   }else{f->record_phase[n]=2;f->record_timer[n]=18;record_row(f,records->rows[f->mode==SPFE_PANIC]+row*16,row,0);}
  }
 }
 if(f->hold)--f->hold;
}
void spfe_clear_player_footer(spfe_t *f,unsigned owner)
{
 if(!f || owner>1)return;
 unsigned start=owner?0xddce:0xdd90;
 for(unsigned y=0;y<4;++y)for(unsigned x=0;x<17;++x){unsigned p=start+y*128+x*2;
  tile(f,p,0x20,f->attributes[(p-0xd000)/2]);}
}
void spfe_draw_name(spfe_t *f,const spang_record_entry *e,unsigned owner,int initial)
{
 if(!f || !e || owner>1)return;
 if(initial){spfe_clear_player_footer(f,owner);text_small_bank(f,0,owner?0xa180:0xa165);f->name_draw_count[owner]=0;}
 unsigned start=owner?0xded4:0xde94,dst=start;
 for(unsigned i=0;i<e->count;++i)large_word(f,&dst,e->name[i],0x5b);
 if(e->count<f->name_draw_count[owner]){
  dst=start+f->name_draw_count[owner]*4;large_word(f,&dst,0x70e,0x5b);
 }
 f->name_draw_count[owner]=e->count;
 dst=start+e->count*4;
 large_word(f,&dst,rw(f,0,0x9ee0+e->selected*2),
            e->blink_phase && e->blink_timer!=1?0x44:0x5b);
 dst=owner?0xdee4:0xdea4;
 large_word(f,&dst,rw(f,0,0x152e + 2*(e->seconds>>4)),0x5b);
 large_word(f,&dst,rw(f,0,0x152e + 2*(e->seconds&15)),0x5b);
}
void spfe_draw_player_gameover(spfe_t *f,unsigned owner)
{
 if(!f || owner>1)return;
 spfe_clear_player_footer(f,owner);text_large_bank(f,0,owner?0xa142:0xa128);
}
static void panel_color(spfe_t *f,int left,int selected) {
 unsigned base=left?0xc98a:0xc9a5;
 for(unsigned y=0;y<10;y++)for(unsigned x=0;x<17;x++)attr(f,base+y*64+x,(uint8_t)(left?(selected?0x23+(x>=8):0x25):(selected?0x20+(x>=8):0x22)));
}
static void panels(spfe_t *f) {
 const unsigned ptr[2]={0xa17e,0xa156},dst[2]={0xd314,0xd34a};
 for(unsigned s=0;s<2;s++)for(unsigned y=0;y<10;y++){
  uint8_t lo=rb(f,3,ptr[s]+y*4),hi=rb(f,3,ptr[s]+y*4+1);
  for(unsigned x=0;x<16;x++)tile(f,dst[s]+y*128+x*2,(uint16_t)((uint8_t)(lo+x)|(hi<<8)),0);
  tile(f,dst[s]+y*128+32,rw(f,3,ptr[s]+y*4+2),0);
 }
 panel_color(f,1,1);panel_color(f,0,0);
 for(unsigned y=0;y<5;y++)for(unsigned x=0;x<4;x++)tile(f,0xd3bc+y*128+x*2,(uint16_t)(0x700|rb(f,3,0xa142+y*4+x)),0x30);
}
static void indicator(spfe_t *f){for(unsigned y=0;y<5;y++)for(unsigned x=0;x<4;x++)attr(f,0xc9de +y*64+x,(uint8_t)(0x30+f->countdown_step));}
static void labels(spfe_t *f) {
 for(unsigned s=0;s<2;s++){uint16_t word=rw(f,3,(s?0x9f68:0x9f62)+4);uint8_t lo=(uint8_t)word,at=(uint8_t)(word>>8);
  int x=(s?0x2c:0x54)+((at&0x10)<<4);
  for(unsigned i=0;i<8;i++)add_sprite(f,(uint16_t)((uint8_t)(lo+i)+((at&0xe0)<<3)),x+(int)i*16,104,at,2);}
}
static void confirm(spfe_t *f) {f->confirm_phase=1;f->confirm_timer=4;f->confirm_flashes=9;f->confirm_toggle=0;sound(f,3);}
void spfe_open_mode(spfe_t *f) {
 clear(f,0x20,0x10);f->state=SPFE_MODE;f->state_ticks=0;f->mode=SPFE_PANIC;
 f->countdown_step=0;f->countdown_timer=120;f->confirm_phase=0;f->panel_pending=0;f->stage_select=0;f->select_done=0;f->previous_input=0;
 f->heading_timer=30;f->heading_color=0x58;
 pal_copy(f,13,0x8e4c,0x300,0xa0);pal_copy(f,13,0x820c,0,0x100);pal_copy(f,12,0x8668,0x200,0x70);
 command(f,8,0x8c);command(f,6,0x64);panels(f);launch(f,3);sound(f,0x30);labels(f);
}
static void mode_tick(spfe_t *f,uint16_t input,uint16_t edge) {
 if(!f->confirm_phase){
  if(f->panel_pending){panel_color(f,1,f->mode==SPFE_PANIC);panel_color(f,0,f->mode==SPFE_TOUR);f->panel_pending=0;}
  else if(edge&SPFE_FIRE){f->stage_select=(input&SPFE_DOWN)!=0;confirm(f);}
  else if((edge&SPFE_RIGHT) && f->mode!=SPFE_TOUR){f->mode=SPFE_TOUR;f->panel_pending=1;sound(f,13);}
  else if((edge&SPFE_LEFT) && f->mode!=SPFE_PANIC){f->mode=SPFE_PANIC;f->panel_pending=1;sound(f,13);}
  if(--f->countdown_timer==0){f->countdown_step++;if(f->countdown_step>=10)confirm(f);else{sound(f,0x1a);f->countdown_timer=120;indicator(f);}}
 }else if(f->confirm_phase==1){if(--f->confirm_timer==0){f->confirm_timer=4;
   /* A074 starts with normal selected colors, then alternates. */
   panel_color(f,f->mode==SPFE_PANIC,!f->confirm_toggle);f->confirm_toggle^=1;
   if(--f->confirm_flashes==0){f->confirm_phase=2;f->confirm_timer=15;}}}
 else if(--f->confirm_timer==0){f->state=SPFE_CONFIRMED;f->heading_color=0;}
 if(--f->heading_timer==0){f->heading_timer=30;for(unsigned x=0;x<30;x++){attr(f,0xc891+x,f->heading_color);attr(f,0xc8d1+x,f->heading_color);}f->heading_color=f->heading_color==0x58?0x44:0x58;}
 actors(f);labels(f);
}
/* BANK3:A71F..A926, actor24. The original has39 cells (three rows of13).
 * R10 adds stage40 with the same original glyphs below them; original rows,
 * input priority, repeat acceleration,999-tick timer and confirmation remain. */
static unsigned select_attribute(spfe_t *f,unsigned index) {
 if(index<39)return rw(f,3,0xa92f+index*2);
 return 0xc800+24*64+33; /* Native40th-entry extension, centered below row3. */
}
static void select_score(spfe_t *f) {
 unsigned value=f->select_index;
 /* Original A7EA/113A prints the first two BCD digits of the×1000 bonus. */
 tile(f,0xde32,(uint16_t)(value/10),0x5a);
 tile(f,0xde34,(uint16_t)(value%10),0x5a);
}
static void select_countdown(spfe_t *f) {
 unsigned value=f->select_remaining;
 tile(f,0xd594,(uint16_t)(value/100),0x5a);
 tile(f,0xd596,(uint16_t)(value/10%10),0x5a);
 tile(f,0xd598,(uint16_t)(value%10),0x5a);
}
void spfe_open_stage_select(spfe_t *f) {
 if(!f || f->error)return;
 clear(f,0x20,0x10);command(f,6,0x4a);
 /* Retain the complete original bonus string, moving its existing row by16px
  * to make room for the extra stage. No new wording or artwork is introduced. */
 for(unsigned col=0;col<64;col++){
  unsigned from=26*64+col;tile(f,0xd000+28*128+col*2,f->tiles[from],f->attributes[from]);
  tile(f,0xd000+26*128+col*2,0x20,0x10);
 }
 tile(f,0xd000+24*128+33*2,'*',0x44);
 tile(f,0xd000+25*128+32*2,'4',0x5a);
 tile(f,0xd000+25*128+33*2,'0',0x5a);
 f->state=SPFE_STAGE_SELECT;f->state_ticks=0;f->select_index=0;
 f->select_sub=f->select_repeat=f->select_last=f->select_delay=f->select_finish=f->select_done=0;
 f->select_remaining=999;attr(f,select_attribute(f,0),0x10);select_score(f);
}
static void select_tick(spfe_t *f,uint16_t input) {
 if(f->select_finish){
  sound(f,3);f->select_done=1;f->state=SPFE_CONFIRMED;return;
 }
 if(f->select_remaining){f->select_remaining--;select_countdown(f);}
 if(!f->select_remaining)f->select_finish=1;
 input&=SPFE_LEFT|SPFE_RIGHT|SPFE_UP|SPFE_DOWN|SPFE_FIRE;
 /* Original entry waits for fire release; entering with held confirm cannot
  * immediately accept the newly displayed stage grid. */
 if(!f->select_sub){
  if((input&SPFE_FIRE) && --f->select_delay)return;
  f->select_delay=rb(f,3,0xa927+f->select_repeat);f->select_sub=1;return;
 }
 if(input){if(!f->select_last)goto action;}
 else{f->select_last=0;f->select_repeat=0;}
 if(--f->select_delay)return;
action:
 f->select_delay=rb(f,3,0xa927+f->select_repeat);
 attr(f,select_attribute(f,f->select_index),0x44);f->select_last=(uint8_t)input;
 unsigned index=f->select_index;
 if(input&SPFE_FIRE)f->select_finish=1;
 else if(input&SPFE_LEFT){if(index<39 && index%13)index--;}
 else if(input&SPFE_RIGHT){if(index<39 && index%13<12)index++;}
 else if(input&SPFE_UP){if(index==39)index=32;else if(index>=13)index-=13;}
 else if(input&SPFE_DOWN){if(index<26)index+=13;else if(index<39)index=39;}
 f->select_index=(uint8_t)index;attr(f,select_attribute(f,index),0x10);select_score(f);
 if(f->select_repeat<7)f->select_repeat++;
}
/* Saturn Start supplies its virtual credit. Keep the original font/blink,
 * with one consistent console prompt at native8px size for either player. */
static void credit_prompt(spfe_t *f) {
 static const char prompt[]="PRESS START BUTTON";
 for(unsigned row=0;row<2;row++)for(unsigned col=0;col<39;col++)
  tile(f,0xde1a+row*128+col*2,0x070e,0x20);
 f->title_blink=32;f->title_blink_phase=0;f->displayed_credits=f->credits;
 for(unsigned i=0;prompt[i];++i)
  tile(f,0xde2e + i*2,prompt[i]==' '?0x070e:(uint16_t)prompt[i],0x27);
 if(f->credits)tile(f,0xdf68,f->credits,0x27);
}
static void credit_blink(spfe_t *f) {
 if(--f->title_blink==0){
  f->title_blink_phase^=1;f->title_blink=f->title_blink_phase?8:32;
  for(unsigned row=0;row<2;row++)for(unsigned col=0;col<39;col++)
   attr(f,0xcf0d+row*64+col,f->title_blink_phase?0x28:0x27);
 }
 if(f->displayed_credits!=f->credits)credit_prompt(f);
}
static void start_title(spfe_t *f,int credit) {
 clear(f,0x70e,0x20);pal_title(f);f->state=credit?SPFE_CREDIT_TITLE:SPFE_TITLE;f->state_ticks=0;f->hold=600;f->title_blink=32;f->title_ready=(uint8_t)credit;f->title_settled=0;
 launch(f,credit?6:1);
 if(credit){
  title_cells(f,0);title_extra(f);title_cells(f,2);title_cells(f,4);command(f,8,0x0c);command(f,6,0x24);credit_prompt(f);
  /* Credit entry is already the original instant-logo path (SUPER sub6).
   * Materialize its setup and final pose before the wrapper freezes this
   * title for fade-out; no logical time or audio command is advanced. */
  actors(f);actors(f);
 }
}
int spfe_init_mapped(spfe_t *f,const uint8_t *fixed,const uint8_t *const banks[16]) {
 static const unsigned required[]={0,3,5,10,12,13};
 if(!f || !fixed || !banks)return 0;
 for(unsigned n=0;n<sizeof required/sizeof required[0];n++)if(!banks[required[n]])return 0;
 memset(f,0,sizeof *f);f->fixed=fixed;
 for(unsigned n=0;n<16;n++)f->banks[n]=banks[n];
 clear(f,0x20,0x10);pal_boot(f);text_small(f,0xa1fe);
 f->state=SPFE_VERSION;f->hold=180;return !f->error;
}
int spfe_init(spfe_t *f,const uint8_t *rom,size_t size) {
 const uint8_t *banks[16];
 if(!f || !rom || size!=0x50000)return 0;
 for(unsigned n=0;n<16;n++)banks[n]=rom+0x10000+n*0x4000;
 return spfe_init_mapped(f,rom,banks);
}
void spfe_return_to_title(spfe_t *f) {
 if(!f || f->error)return;
 f->sound_count=0;f->previous_input=0;f->confirm_phase=0;f->stage_select=0;
 start_title(f,f->credits!=0);
}
static void console_text(spfe_t *f,unsigned col,unsigned row,const char *text) {
 for(;*text && col<52;++text,++col)
  tile(f,0xd000+row*128+col*2,*text==' '?0x070e:(uint8_t)*text,0x27);
}
void spfe_console_home(spfe_t *f,unsigned selected) {
 if(!f || f->error)return;
 start_title(f,1);
 /* Keep the complete original logo, replacing only its lower prompt area. */
 for(unsigned row=26;row<32;++row)for(unsigned col=0;col<64;++col)
  tile(f,0xd000+row*128+col*2,0x070e,0x20);
 console_text(f,28,26,"NEW GAME");console_text(f,28,28,"OPTIONS");
 console_text(f,26,selected?28:26,">");
 f->previous_input=0;f->confirm_phase=0;f->stage_select=0;
}
void spfe_console_notice(spfe_t *f,const char *message){
 for(unsigned col=12;col<52;col++)tile(f,0xd000+30*128+col*2,0x70e,0x20);
 if(message){unsigned length=0;while(length<40 && message[length])++length;if(length<40)console_text(f,32-length/2,30,message);}
}
static void console_number(spfe_t *f,unsigned col,unsigned row,int number){
 char text[5];unsigned at=0;if(number<0){text[at++]='-';number=-number;}
 if(number>=10)text[at++]=(char)('0'+number/10);
 text[at++]=(char)('0'+number%10);text[at]=0;
 console_text(f,col,row,text);
}
void spfe_console_options(spfe_t *f,const uint8_t settings[8],unsigned selected,int center) {
 if(!f || f->error)return;
 clear(f,0x070e,0x20);pal_title(f);f->sprite_count=0;
 if(center){
  console_text(f,24,3,"SCREEN POSITION");
  console_text(f,23,22,"X:");console_number(f,26,22,(int8_t)settings[6]);
  console_text(f,33,22,"Y:");console_number(f,36,22,(int8_t)settings[7]);
  console_text(f,16,25,"D-PAD: MOVE   A: RESET");console_text(f,28,27,"C: BACK");return;
 }
 static const char *const labels[]={"DIFFICULTY","LIVES","DEMO SOUNDS","SOUND","MUSIC VOLUME","FX VOLUME","SCREEN POSITION","BACK"};
 static const char *const difficulties[]={"EASY","NORMAL","DIFFICULT","VERY DIFFICULT"};
 console_text(f,28,3,"OPTIONS");
 for(unsigned row=0;row<8;row++)console_text(f,16,7+row*2,labels[row]);
 console_text(f,14,7+selected*2,">");
 console_text(f,34,7,difficulties[settings[0]&3]);console_number(f,34,9,settings[1]);
 console_text(f,34,11,settings[2]?"ON":"OFF");console_text(f,34,13,settings[3]?"MONO":"STEREO");
 console_number(f,34,15,settings[4]);console_number(f,34,17,settings[5]);
 console_text(f,17,26,"LEFT/RIGHT: CHANGE");console_text(f,17,28,"A: SELECT    C: BACK");
}
void spfe_tick(spfe_t *f,uint16_t input) {
 if(!f)return;
 spfe_tick_edges(f,input,input&(uint16_t)~f->previous_input);
}
void spfe_tick_edges(spfe_t *f,uint16_t input,uint16_t edge) {
 if(!f||f->error)return;
 f->previous_input=input;f->ticks++;f->state_ticks++;f->sound_count=0;
 /* Original coin is OKI command bytes97/80, not the provisional F01 FM1. */
 if(edge&SPFE_COIN){if(f->credits<9)f->credits++;sound_event(f,SPFE_SOUND_OKI,0x17,0x80);}
 switch(f->state){
 /* R10R2: the requested boot flow skips the standalone legal notice. */
 case SPFE_VERSION:if(--f->hold==0)start_title(f,f->credits!=0);break;
 case SPFE_WARNING:if(--f->hold==0)start_title(f,f->credits!=0);break;
 case SPFE_TITLE:
  actors(f);if(f->credits){start_title(f,1);break;}
  if(!f->title_ready){
   unsigned count=0,ready=1;
   for(unsigned i=0;i<8;++i)if(f->actor[i].active){
    if(f->actor[i].type==2)ready=0;
    if(f->actor[i].type==4){++count;if(f->actor[i].state<4)ready=0;}
   }
   if(ready && count==5){f->title_ready=1;credit_prompt(f);}
  }else credit_blink(f);
  if(--f->hold==0)f->state=SPFE_ATTRACT_REQUEST;
  break;
 case SPFE_CREDIT_TITLE:
  actors(f);credit_blink(f);
  if((edge&SPFE_START)&&f->credits){f->credits--;spfe_open_mode(f);f->previous_input=input;}break;
 case SPFE_MODE:mode_tick(f,input,edge);break;
 case SPFE_ATTRACT_REQUEST:if(f->credits)start_title(f,1);break;
 case SPFE_STAGE_SELECT:select_tick(f,input);break;
 case SPFE_CONFIRMED:break;
 case SPFE_RECORDS:case SPFE_NAME_ENTRY:break;
 }
}
unsigned spfe_clock_step(spfe_clock_t *c,uint32_t num,uint32_t den) {
 if(!c||!num||!den)return 0;
 uint64_t threshold=100ull*num;c->phase+=5742ull*den;
 unsigned n=(unsigned)(c->phase/threshold);c->phase%=threshold;return n;
}
int spfe_cell_destination(spfe_state_t state,unsigned index,int *x,int *y) {
 int col=(int)(index%64),row=(int)(index/64);*x=col*8;*y=row*8;
 if(state==SPFE_TITLE||state==SPFE_CREDIT_TITLE||state==SPFE_ATTRACT_REQUEST)return 1;
 *x-=80;*y-=16;
 if(state==SPFE_MODE||state==SPFE_CONFIRMED){
  /* Panel/body columns are independent groups. Heading and time stay centered. */
  if(row>=5){if(col<(row>=16?34:30))*x+=8;else if(col>=35)*x-=8;}
 }
 return *x>=0&&*x<352&&*y>=0&&*y<224;
}
static int scale11(int x){/* floor, including offscreen negative coordinates */return x>=0?x*11/12:-((-x*11+11)/12);}
void spfe_sprite_destination(spfe_state_t state,const spfe_sprite_t *s,int *x,int *y,int *w,int *h) {
 *x=s->x;*y=s->y;*w=16;*h=16;
 if(state==SPFE_TITLE||state==SPFE_CREDIT_TITLE||state==SPFE_ATTRACT_REQUEST){int ox=s->x-64,oy=s->y-8;*x=scale11(ox);*y=scale11(oy)+2;*w=scale11(ox+16)-*x;*h=scale11(oy+16)-(*y-2);}
 else{*x-=80;*y-=16;if(s->layer==2)*x+=s->x<256?8:-8;}
}
