#include "spang_attract_tutorial.h"
#include <string.h>
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void cell(uint8_t *attrs,uint8_t *chars,unsigned p,unsigned code,unsigned color)
{
    unsigned index=(p-0xd000)/2;put(chars+index*2,code);attrs[index]=(uint8_t)color;
}
static void clear_box(spang_attract_tutorial *s,uint8_t *attrs,uint8_t *chars)
{
    unsigned dest=s->which?0xd194:0xd1ba,color=s->which?0x1e:0x1d;
    for(unsigned y=0;y<6;++y)for(unsigned x=0;x<22;++x)
        cell(attrs,chars,dest+y*128+x*2,0x70e,color);
}
static void rim(uint8_t *attrs,uint8_t *chars,unsigned p,unsigned n,unsigned code)
{
    for(unsigned x=0;x<n;++x)
        cell(attrs,chars,p+x*2,0xf00+code+(x>0 && x+1<n),x+1==n?0xeb:0x6b);
}
static void middle(uint8_t *attrs,uint8_t *chars,unsigned p)
{
    cell(attrs,chars,p,0xf08,0x6b);
    for(unsigned x=1;x<21;++x)cell(attrs,chars,p+x*2,0x70e,0x6c);
    cell(attrs,chars,p+42,0xf08,0xeb);
}
static int geometry(spang_attract_tutorial *s,const uint8_t *bank,uint8_t *attrs,uint8_t *chars,int reverse)
{
    uint8_t *a=s->actor;unsigned p=word(a+10);
    if(p<0x8000 || p>0xbffc)return -1;
    unsigned dest=word(bank+(p-0x8000));a[30]=bank[p+2-0x8000];a[31]=bank[p+3-0x8000];
    put(a+10,p+(reverse?-4:4));
    if(dest<0xd000 || dest>=0xde00 || (dest&1) || a[30]<2 || a[30]>32 || !a[31] || a[31]>6)return -1;
    rim(attrs,chars,dest,a[30],6);
    for(;;){
        dest+=128;
        if(--a[31]==0){rim(attrs,chars,dest,a[30],0x16);return 0;}
        middle(attrs,chars,dest);
        if(a[31]!=1){dest+=128;middle(attrs,chars,dest);}
    }
}
static void next_delay(spang_attract_tutorial *s,const uint8_t *bank)
{
    uint8_t *a=s->actor;a[19]=bank[(s->which?0x108d:0x1084)+a[28]++];
}
int spang_attract_tutorial_init(spang_attract_tutorial *s,unsigned which)
{
    if(!s || which>1)return -1;
    memset(s,0,sizeof *s);s->which=(uint8_t)which;s->actor[0]=1;s->actor[17]=0x14;return 0;
}
int spang_attract_tutorial_tick(spang_attract_tutorial *s,const uint8_t bank[16384],
                               uint8_t attrs[2048],uint8_t chars[4096],uint16_t *command)
{
    if(!s || !bank || !attrs || !chars || !command || s->error)return -1;
    uint8_t *a=s->actor;*command=0;
    if(s->done)return 0;
    if(!a[2]){a[2]=(uint8_t)(s->which*2+2);a[28]=0;return 0;}
    if(a[2]==6){a[0]=0;s->done=1;return 0;}
    if(a[2]!=2 && a[2]!=4)goto bad;
    switch(a[3]){
    case 0:
        a[3]=2;if(s->which)a[19]=30;else next_delay(s,bank);break;
    case 2:
        if(--a[19]==0){a[3]=4;a[29]=0;put(a+10,s->which?0x90cc:0x9098);}break;
    case 4:
        if(geometry(s,bank,attrs,chars,0))goto bad;
        if(++a[29]<13)break;
        a[3]=6;
        if(s->which){next_delay(s,bank);*command=(uint16_t)(0x1478+a[28]*2);}
        else{
            put(a+10,0x90c8);a[19]=90;s->paused=2;
            *command=(uint16_t)word(bank+0xf9e + (a[28]-1)*2);
        }
        break;
    case 6:
        if(--a[19])break;
        if(!s->which){a[3]=8;++a[29];}
        else{
            put(a+10,0x90fc);a[29]=12;a[3]=4;
            if(a[28]>=10){a[3]=8;++a[29];}
        }
        break;
    case 8:
        clear_box(s,attrs,chars);
        if(--a[29]){if(geometry(s,bank,attrs,chars,1))goto bad;break;}
        if(s->which){a[3]=10;a[19]=255;}
        else{
            s->paused=0;a[3]=0;
            if(a[28]>=8){a[3]=10;next_delay(s,bank);}
        }
        break;
    case 10:if(--a[19]==0)a[2]=6;break;
    default:goto bad;
    }
    return 0;
bad:s->error=1;return -1;
}
