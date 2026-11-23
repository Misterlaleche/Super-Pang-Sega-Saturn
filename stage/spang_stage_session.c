#include "spang_stage_session.h"
#include "spang_items.h"
#include "spang_round_speed.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned value){p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);}
int spang_stage_session_reset(uint8_t g[128])
{
 if(!g)return -140;
 static const uint8_t zero[]={0x2d,0x2b,0x2c,0x44,0x4a,0x4e,0x4d,0x40,0x3e,0x3f,0x57};
 for(unsigned i=0;i<sizeof zero;++i)g[zero[i]]=0;
 g[0x5c]=15;put(g+0x3c,0xa664);put(g+0x42,0xa704);put(g+0x60,0x9e10);
 return 0;
}
int spang_stage_session_begin(uint8_t globals[128],uint8_t scores[128],
 uint16_t *seed,const uint8_t b[16384],const uint8_t fixed[32768])
{
 if(!globals || !scores || !seed || !b || !fixed)return -140;
 unsigned stage=globals[0x19],rank=globals[0x52];
 if(stage>=40 || rank>=32 || globals[0x1a] || globals[0x23])return -141;
 unsigned choice=b[0xa68+stage];
 if(choice>6 || (choice&1) || fixed[0x2f45+rank]>40)return -142;
 unsigned alternate=word(b+0xa90+choice);
 if(alternate<0x8000 || alternate>0xbfc0)return -142;
 uint8_t g[128];memcpy(g,globals,128);uint16_t next=*seed;
 (void)spang_stage_session_reset(g);
 if(stage<35 && !g[0x4c] && !spang_items_random(&next))g[0x4d]=10;
 g[0x40]=b[0x946+stage];put(g+0x3e,word(b+0xa18+stage*2));
 put(g+0x3c,0xa164+stage*32);put(g+0x42,alternate);
 if(spang_round_speed_apply(g,0,fixed))return -143;
 memcpy(globals,g,128);*seed=next;scores[0]=2;put(scores+2,0xa263);
 return 0;
}
