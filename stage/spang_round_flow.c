#include "spang_round_flow.h"
#include "spang_round_speed.h"
#include <string.h>
static void victory(uint8_t p[128])
{if(p[0]){p[2]=2;p[3]=6;p[4]=p[5]=0;}}
int spang_round_advance(uint8_t globals[128],const uint8_t fixed[32768])
{
 if(!globals || !fixed || globals[0x19]>=39 || globals[0x52]>=32)return -1;
 uint8_t g[128];memcpy(g,globals,128);
 ++g[0x19];g[0x18]=fixed[0x1260+g[0x19]];
 if(g[0x18]>=15 || fixed[0x3232+g[0x19]]>=32)return -1;
 g[0x17]=fixed[0x128a+g[0x18]];
 if(g[0x17]>=3 || spang_round_speed_adjust(g,fixed[0x320a+g[0x19]],fixed))return -1;
 g[0]=6;g[1]=0;g[0x0f]=g[0x47]=g[0x48]=g[0x49]=g[0x5f]=g[0x64]=0;
 memcpy(globals,g,128);return 0;
}
int spang_round_retry_account(uint8_t globals[128],uint8_t p1[128],uint8_t p2[128],const uint8_t fixed[32768])
{
 if(!globals || !p1 || !p2 || !fixed || globals[0x19]>=40 || globals[0x52]>=32 || fixed[0x3232+globals[0x19]]>=32)return -1;
 uint8_t g[128];memcpy(g,globals,128);++g[0x0f];
 if(spang_round_speed_adjust(g,(uint8_t)(0-g[0x57]),fixed))return -1;
 memcpy(globals,g,128);p1[0x5f]=p2[0x5f]=0;return 0;
}
int spang_round_flow(uint8_t g[128],uint8_t p[128],uint8_t p2[128],uint8_t a[32],const uint8_t fixed[32768],uint8_t e[2])
{
 e[0]=e[1]=0;
 if(g[0]==8){
  if(!g[0x20]){g[4]=30;g[0]=0x12;e[0]=0x35;}
  return 0;
 }
 if(g[0]!=0x12)return 1;
 if(g[1]==0){
  if(!--g[4]){
   g[1]=2;victory(p);victory(p2);
   a[2]=a[3]=0;a[0]=1;a[17]=2;
   return spang_round_speed_adjust(g,(uint8_t)(0-g[0x57]),fixed);
  }
 }else if(g[1]==2){
  if(g[2]==0){if(!a[0]){g[2]=2;e[1]=1;}}
  else if(g[2]==2){g[6]=g[0x4e]?180:g[0x0c]==3?150:60;g[2]=4;}
  else if(g[2]==4){if(!--g[6]){g[1]=4;g[2]=0;}}
  else return -1;
 }else if(g[1]==4){
  if(g[0x19]>=40)return -1;
  p[0x5f]=p2[0x5f]=0;
  if((p[0x60]>=6 || p2[0x60]>=6) && spang_round_speed_adjust(g,1,fixed))return -1;
  if(g[0x19]==39){g[1]=10;e[1]=8;}
  else if(fixed[0x39c5+g[0x19]]){g[1]=6;e[1]=2;}
  else{if(spang_round_advance(g,fixed))return -1;e[1]=4;}
 }else return 1;
 return 0;
}
