#include "spang_attract_input.h"
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
int spang_attract_input_init(spang_attract_input *s,const uint8_t bank[16384],unsigned pair)
{
    if(!s || !bank || pair>4)return -1;
    unsigned a=word(bank+0x1e6d+pair*4),b=word(bank+0x1e6f+pair*4);
    if(a<0x8000 || a>=0xbfff || b<0x8000 || b>=0xbfff)return -1;
    s->pointer[0]=(uint16_t)a;s->pointer[1]=(uint16_t)b;
    s->remaining[0]=s->remaining[1]=s->loaded=0;return 0;
}
int spang_attract_input_tick(spang_attract_input *s,const uint8_t bank[16384],uint8_t output[2])
{
    if(!s || !bank || !output)return -1;
    for(unsigned i=0;i<2;++i){
        unsigned p=s->pointer[i];
        if(p<0x8000 || p>=0xbfff)return -1;
        output[i]=bank[p-0x8000];
        if(!s->loaded)s->remaining[i]=bank[p+1-0x8000];
        else if(--s->remaining[i]==0){
            p+=2;if(p>=0xbfff)return -1;
            s->pointer[i]=(uint16_t)p;s->remaining[i]=bank[p+1-0x8000];
        }
    }
    s->loaded=1;return 0;
}
