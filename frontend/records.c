#include "records.h"
#include <string.h>

static unsigned mode_index(unsigned mode){return mode==2;}
static uint16_t word(const uint8_t *p){return (uint16_t)(p[0]|((unsigned)p[1]<<8));}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
uint16_t spang_records_glyph(const uint8_t bank[16384],unsigned index)
{return bank && index<33?word(bank+0x1ee0+index*2):0x70e;}
static void ranks(uint8_t rows[80])
{
 unsigned rank=0;rows[0]=0;
 for(unsigned i=1;i<5;++i){if(memcmp(rows+(i-1)*16+1,rows+i*16+1,4))++rank;rows[i*16]=(uint8_t)rank;}
}
void spang_records_init(spang_records *s,const uint8_t bank[16384])
{
 if(!s || !bank)return;
 memset(s,0,sizeof *s);memcpy(s->rows,bank+0x244f,160);memcpy(s->total,bank+0x1f3,32);
 s->revision=1;
}
static void commit_row(uint8_t *row,const spang_record_entry *e,unsigned owner)
{
 if(row[5]!=owner+1)return;
 row[5]=0;
 for(unsigned i=0;i<3;++i)put(row+6+i*2,i<e->count?e->name[i]:0x70e);
 if(e->mode==0)row[13]=e->world;
 row[14]=e->stage;
 /* BANK0:99B9..9A1B, preserve the original two replacement sequences. */
 unsigned a=word(row+6),b=word(row+8),c=word(row+10);
 if((a==0xc4 && b==0x88 && c==0xce) || (a==0x8a && b==0xc8 && c==0xa4))
  for(unsigned i=0;i<3;++i)put(row+6+i*2,0x32a);
}
static void commit(spang_records *s,unsigned owner)
{
 spang_record_entry *e=s->entry+owner;unsigned m=mode_index(e->mode);
 commit_row(s->total[m],e,owner);
 for(unsigned i=0;i<5;++i)if(s->rows[m][i*16+5]==owner+1){commit_row(s->rows[m]+i*16,e,owner);break;}
 ranks(s->rows[m]);e->phase=0;++s->revision;
}
static void insert_fields(spang_records *s,uint8_t *row,const uint8_t *p,
                          const uint8_t *score,unsigned owner)
{
 memcpy(row+1,score,4);row[5]=(uint8_t)(owner+1);row[12]=p[0x4e];
 for(unsigned m=0;m<2;++m)for(unsigned i=0;i<5;++i)
  if(s->rows[m][i*16+15]==owner+1)s->rows[m][i*16+15]=0;
 row[15]=(uint8_t)(owner+1);
}
int spang_records_submit(spang_records *s,unsigned mode,const uint8_t p[128],
                         unsigned owner,const uint8_t fixed[32768])
{
 if(!s || !p || !fixed || owner>1 || (mode!=0 && mode!=2))return 0;
 unsigned m=mode_index(mode);const uint8_t *score=p+0x44;
 if(memcmp(p+0x40,score,4)>0)score=p+0x40;
 /* A later completed attempt must not leave an earlier pending row owned by
  * the same editor. This also makes separate player-finish events lossless. */
 if(s->entry[owner].phase)commit(s,owner);
 if(memcmp(score,s->total[m]+1,4)>=0)insert_fields(s,s->total[m],p,score,owner);
 unsigned place=0;while(place<5 && memcmp(score,s->rows[m]+place*16+1,4)<0)++place;
 if(place==5)return 0;
 if(place<4)memmove(s->rows[m]+(place+1)*16,s->rows[m]+place*16,(4-place)*16);
 insert_fields(s,s->rows[m]+place*16,p,score,owner);ranks(s->rows[m]);
 spang_record_entry *e=s->entry+owner;memset(e,0,sizeof *e);
 e->phase=1;e->seconds=0x20;e->prescale=60;e->selected=1;
 e->repeat_left=e->repeat_right=10;e->blink_timer=1;e->blink_phase=2;
 e->mode=(uint8_t)mode;e->world=(uint8_t)(p[0x48]+1);
 e->stage=mode==2?p[0x4d]:fixed[0x101b+p[0x49]];
 for(unsigned i=0;i<3;++i)e->name[i]=0x70e;
 ++s->revision;return 1;
}
static void selection(spang_record_entry *e,int direction)
{
 if(e->count==3)e->selected=(uint8_t)((e->selected+1)&1);
 else e->selected=(uint8_t)((e->selected+33+direction)%33);
 e->repeat_left=e->repeat_right=10;e->blink_phase=2;e->blink_timer=1;
}
unsigned spang_records_tick(spang_records *s,unsigned owner,uint8_t held,
                            uint8_t pressed,const uint8_t bank[16384])
{
 if(!s || !bank || owner>1)return 0;
 spang_record_entry *e=s->entry+owner;unsigned result=0;
 if(!e->phase)return 0;
 if(e->phase==2){commit(s,owner);return SPANG_RECORD_CHANGED|SPANG_RECORD_FINISHED;}
 if(--e->prescale==0){
  e->prescale=60;
  if(!e->seconds){e->phase=2;return SPANG_RECORD_CHANGED;}
  e->seconds=(uint8_t)((e->seconds&15)?e->seconds-1:e->seconds-7);
  result|=SPANG_RECORD_CHANGED;
 }
 if(pressed&8){
  result|=SPANG_RECORD_SOUND|SPANG_RECORD_CHANGED;
  if(e->selected==1)e->phase=2;
  else if(!e->selected){if(e->count){--e->count;e->name[e->count]=0x70e;selection(e,0);}}
  else{
   e->name[e->count++]=spang_records_glyph(bank,e->selected);
   if(e->count==3)e->selected=1;
   e->repeat_left=e->repeat_right=10;e->blink_phase=2;e->blink_timer=1;
   return result; /* BANK0:9914 pops the caller; blink resumes next tick. */
  }
 }else if(pressed&0x20){selection(e,-1);result|=SPANG_RECORD_CHANGED;}
 else if(pressed&0x10){selection(e,1);result|=SPANG_RECORD_CHANGED;}
 else if(held&0x20){if(--e->repeat_left==0){selection(e,-1);result|=SPANG_RECORD_CHANGED;}}
 else if(held&0x10){if(--e->repeat_right==0){selection(e,1);result|=SPANG_RECORD_CHANGED;}}
 if(--e->blink_timer==0){e->blink_timer=20;e->blink_phase=e->blink_phase?0:2;result|=SPANG_RECORD_CHANGED;}
 return result;
}
