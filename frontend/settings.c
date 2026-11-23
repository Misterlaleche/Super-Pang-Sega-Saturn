#include "settings.h"
#include <string.h>
static spang_settings settings={1,3,1,0,10,10,0,0};
static unsigned revision=1;
void spang_settings_reset(void){const spang_settings defaults={1,3,1,0,10,10,0,0};settings=defaults;revision=1;}
const spang_settings *spang_settings_get(void){return &settings;}
unsigned spang_settings_revision(void){return revision;}
int spang_settings_valid(const uint8_t d[8])
{return d&&d[0]<4&&d[1]>=1&&d[1]<=4&&d[2]<2&&d[3]<2&&d[4]<=10&&d[5]<=10&&(int8_t)d[6]>=-16&&(int8_t)d[6]<=16&&(int8_t)d[7]>=-12&&(int8_t)d[7]<=12;}
int spang_settings_import(const uint8_t d[8])
{if(!spang_settings_valid(d))return 0;if(memcmp(&settings,d,8)){memcpy(&settings,d,8);++revision;}return 1;}
void spang_settings_export(uint8_t d[8]){memcpy(d,&settings,8);}
int spang_settings_adjust(unsigned row,int direction)
{
 uint8_t data[8];static const unsigned lo[6]={0,1,0,0,0,0},hi[6]={3,4,1,1,10,10};
 if(row>=6||!direction)return 0;
 spang_settings_export(data);int n=data[row]+(direction>0?1:-1);
 if(n<(int)lo[row])n=(int)lo[row];
 if(n>(int)hi[row])n=(int)hi[row];
 if(data[row]==n)return 0;
 data[row]=(uint8_t)n;return spang_settings_import(data);
}
int spang_settings_move(int dx,int dy)
{
 int x=settings.x+dx,y=settings.y+dy;
 if(x< -16)x=-16;
 if(x>16)x=16;
 if(y< -12)y=-12;
 if(y>12)y=12;
 if(x==settings.x&&y==settings.y)return 0;
 settings.x=(int8_t)x;settings.y=(int8_t)y;++revision;return 1;
}
void spang_settings_center(void){if(settings.x||settings.y){settings.x=settings.y=0;++revision;}}
