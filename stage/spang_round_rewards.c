#include "spang_round_rewards.h"
#include "spang_score.h"
#include "spang_motion_bonus.h"
#include <string.h>
int spang_round_reward_queue(spang_round_frame *s,uint8_t p1[128],uint8_t p2[128],
 const uint8_t table[257],uint8_t best[4],const uint8_t thresholds[8])
{
 if(!s || !p1 || !p2 || !table || !best || !thresholds)return -140;
 unsigned length=s->queue_length;
 if(s->direct_length>8 || length>64 || (length&1) || p1[0x4a]>10 || (p1[0x4a]&1) || p2[0x4a]>10 || (p2[0x4a]&1))return -141;
 for(unsigned n=0;n<length;n+=2){unsigned c=s->queue[n];if(c!=10 && c!=12 && c!=14 && c!=16)return -142;}
 for(unsigned n=0;n<s->direct_length;++n)if(s->direct[n*3]!=0 && s->direct[n*3]!=2)return -142;
 uint8_t players[2][128],high[4];memcpy(players[0],p1,128);memcpy(players[1],p2,128);memcpy(high,best,4);
 for(unsigned n=0;n<s->direct_length;++n){const uint8_t *a=s->direct+n*3;uint8_t *p=players[a[0]/2];if(!p[0])continue;
  spang_score_add(p+0x40,(uint16_t)(a[1]|((unsigned)a[2]<<8)));
  uint8_t effects[2];if(spang_motion_bonus(p,thresholds,effects)<0)return -143;spang_score_promote(high,p+0x40);
 }
 for(unsigned n=0;n<length;n+=2){unsigned command=s->queue[n],parameter=s->queue[n+1];
  uint8_t *p=players[command==12 || command==16];if(!p[0])continue;
  if(command>=14)spang_score_add_low(p+0x40,0,0x50);
  else spang_score_add(p+0x40,(uint16_t)(table[parameter]|((unsigned)table[parameter+1]<<8)));
  uint8_t effects[2];if(spang_motion_bonus(p,thresholds,effects)<0)return -143;
  spang_score_promote(high,p+0x40);
 }
 memcpy(p1,players[0],128);memcpy(p2,players[1],128);memcpy(best,high,4);
 unsigned consumed=length/2+s->direct_length;
 memset(s->queue,255,length);s->queue_length=0;s->direct_length=0;return (int)consumed;
}
