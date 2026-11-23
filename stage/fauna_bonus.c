#include "fauna_internal.h"

static int award(fauna_context *c,uint8_t *a,unsigned table)
{
 unsigned value=fa_bw(c->bank4,table+2*(c->globals[0x19]&3));
 if(!value){
  /* BANK4:B2D7..B2F4 selects the hit owner, then ADD 1 / DAA.
   * Keep the original packed-BCD wrap from 99 to 00. The stage owner
   * refreshes the life HUD after this actor step (original FIXED166B). */
  uint8_t *p=a[24]?c->player2:c->player1;
  unsigned life=p[0x60],next=life+1;
  if((life&15)+1>9)next+=6;
  if(life+1>0x99)next+=0x60;
  p[0x60]=(uint8_t)next;return 0;
 }
 return fauna_queue(c,(value>>8)+a[24],value&255);
}
int fauna_bonus_step(fauna_context *c,unsigned slot,uint8_t draw[40])
{
 uint8_t *a=c->actors+slot*40;const uint8_t *b=c->bank4;
 switch(a[2]){
 case 0:
  a[2]=2;a[0]=a[1]=1;fa_put(a+10,56);a[13]=32;a[19]=176;a[25]=1;fa_put(a+20,0x180);fa_animation(a,b,0xb36e);
  if(c->player1[0]){a[24]=0;int r=award(c,a,0xb39a);if(r)return r;}
  if(c->player2[0]){a[24]=2;int r=award(c,a,0xb39a);if(r)return r;}
  return 0;
 case 2:
  if(!a[25])a[2]=4;
  else {
   fa_move(a+9,(int16_t)fa_word(a+20));int r=fauna_animate(a,b);if(r)return r;
   if(!--a[19]){
    if(a[3]==0){fa_put(a+20,0xfee0);a[19]=102;a[3]=2;fa_animation(a,b,0xb384);}
    else if(a[3]==2){fa_put(a+20,0x280);a[19]=106;a[3]=4;fa_animation(a,b,0xb36e);}
    else if(a[3]==4)a[2]=6;else return -722;
   }
  }
  memcpy(draw,a,40);return 1;
 case 4:
  a[2]=6;
  {unsigned type=b[0x33aa+(c->globals[0x19]&3)];int spawned=fauna_score(c,a,type);
   if(spawned && type==0x5e && c->sound)c->sound(c->sound_context,0x13);}
  return award(c,a,0xb3a2);
 case 6:memset(a,0,6);++c->globals[0x1e];return 0;
 default:return -723;
 }
}
