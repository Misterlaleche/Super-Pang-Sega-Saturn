#include "fauna_internal.h"

int fauna_step_with_ladders(fauna_context *c,unsigned slot,uint8_t draw[40],const uint8_t ladders[128])
{
 if(slot>=8)return -740;
 uint8_t *a=c->actors+slot*40;
 switch(a[17]){
 case 0:return fauna_puffer_step(c,slot,draw);
 case 3:return crocodile_step_with_ladders(a,c->scheduler,c->globals,c->player1,c->player2,
  c->balls,c->grid,c->round,c->bank4,draw,c->sound,c->sound_context,ladders);
 case 6:return fauna_bird_step(c,slot,draw);
 case 9:return fauna_bonus_step(c,slot,draw);
 default:return -741;
 }
}
int fauna_step(fauna_context *c,unsigned slot,uint8_t draw[40])
{return fauna_step_with_ladders(c,slot,draw,0);}
