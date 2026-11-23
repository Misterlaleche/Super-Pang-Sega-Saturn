#include "spang_harpoon.h"
#include <string.h>
int spang_harpoon_break(uint8_t actor[32], uint8_t grid[2048])
{
    if(actor[2]!=0xe8 || actor[3]!=0x81 || (actor[31]&0x40))return -1;
    unsigned address=actor[29]|((unsigned)actor[30]<<8);
    if(actor[31]<0x80)address-=actor[31]&7;
    if(address<0xf000 || address>=0xf800)return -1;
    uint8_t cell=grid[address-0xf000];unsigned count=cell&7;
    if(cell && !count)return -1;
    for(unsigned i=0;i<count;++i){unsigned a=(address&0xff00)|((address+i)&255);if(a<0xf000 || a>=0xf800)return -1;}
    for(unsigned i=0;i<count;++i)grid[((address&0xff00)|((address+i)&255))-0xf000]=0;
    actor[2]=0x34;actor[3]=0x82;actor[29]=(uint8_t)address;actor[30]=(uint8_t)(address>>8);actor[31]=cell;
    return (int)count;
}
int spang_harpoon_hit(uint8_t actor[32], uint8_t ball[32], uint8_t player[128])
{
    if(actor[17]>2 || (actor[17]&1) || ball[0]!=1 || ball[21] || ball[28]<1 || ball[28]>5)return -1;
    uint16_t hits=(uint16_t)((player[0x3e]|((uint16_t)player[0x3f]<<8))+1u);
    player[0x3e]=(uint8_t)hits;player[0x3f]=(uint8_t)(hits>>8);
    actor[0]=2;actor[2]=0x34;actor[3]=0x82;
    ball[0]=2;ball[27]=0;ball[2]=0xc1;ball[3]=0x90;ball[24]=actor[17];
    uint8_t combo=0;
    if(player[0x59]==ball[28]){combo=(uint8_t)(player[0x5a]+2u);if(combo>=6)combo=6;}
    player[0x59]=ball[28];player[0x5a]=combo;ball[29]=combo;
    return 1;
}
int spang_harpoon_init(uint8_t actor[32], const uint8_t bank2[16384])
{
    if ((actor[15]!=0 && actor[15]!=2) || actor[17]>2 || (actor[17]&1)) return -1;
    unsigned offset=actor[15]*2+actor[17];
    unsigned animation=bank2[0xbc+offset]|((unsigned)bank2[0xbd+offset]<<8);
    if (animation<0x8000 || animation+1>=0xc000) return -1;
    actor[0]=1;
    actor[27]=(uint8_t)((actor[10]|((unsigned)actor[11]<<8))>>1);
    actor[14]=(uint8_t)(actor[13]+32u);
    actor[6]=(uint8_t)animation;actor[7]=(uint8_t)(animation>>8);
    actor[19]=bank2[animation-0x8000]&0x7f;actor[8]=bank2[animation+1-0x8000];
    actor[2]=bank2[0xc4+offset];actor[3]=bank2[0xc5+offset];return 0;
}
int spang_harpoon_flight(uint8_t actor[32], uint8_t frame,
                        const uint8_t bank2[16384], const uint8_t grid[2048])
{
    return spang_harpoon_flight_draw(actor,frame,bank2,grid,0);
}
int spang_harpoon_flight_draw(uint8_t actor[32], uint8_t frame,
                        const uint8_t bank2[16384], const uint8_t grid[2048],
                        uint8_t draw[32])
{
    if (!((actor[2]==0xcc && actor[3]==0x80 && actor[15]==0) ||
          (actor[2]==0x52 && actor[3]==0x81 && actor[15]==2))) return -1;
    uint8_t work[32];memcpy(work,actor,32);
    uint16_t y=(uint16_t)((work[12]|((uint16_t)work[13]<<8))-0x240u);
    work[12]=(uint8_t)y;work[13]=(uint8_t)(y>>8);
    if (!work[15] && !--work[19]) {
        unsigned current=work[6]|((unsigned)work[7]<<8);
        if (current<0x8000 || current+5>=0xc000) return -1;
        unsigned next=current+4;
        if (bank2[current-0x8000]>=0x80)
            next=bank2[next-0x8000]|((unsigned)bank2[next+1-0x8000]<<8);
        if (next<0x8000 || next+1>=0xc000) return -1;
        work[6]=(uint8_t)next;work[7]=(uint8_t)(next>>8);
        work[19]=bank2[next-0x8000]&0x7f;work[8]=bank2[next+1-0x8000];
    }
    if(draw)memcpy(draw,work,32);
    if ((uint8_t)(frame+work[1])&1) {
        unsigned x=work[10]|((unsigned)work[11]<<8);
        unsigned index=((work[13]>>3)<<6)|((x&511)>>3);
        uint8_t cell=grid[index];
        if (cell && cell<0xfc) {
            work[0]=2;work[29]=(uint8_t)index;work[30]=(uint8_t)(0xf0+(index>>8));
            work[31]=cell;work[2]=0xe8;work[3]=0x81;
        } else if (cell) {
            work[13]|=7;
            uint8_t bottom=(uint8_t)(work[13]+32);
            if (bottom>=work[14]) work[14]=bottom;
            work[2]=work[15]?0x76:0x34;work[3]=work[15]?0x81:0x82;
        }
    }
    memcpy(actor,work,32);return 0;
}
int spang_harpoon_sprite(const uint8_t actor[32], const uint8_t bank2[16384],
                        uint8_t objects[4096], uint16_t *cursor, uint8_t *remaining)
{
    unsigned length=(uint8_t)(actor[14]-actor[13]),whole=length>>4;
    if (whole<2) return -1;
    unsigned reserved=whole+1;
    if (*remaining<reserved) return 0;
    unsigned count=whole+((length&15)!=0);
    if ((*cursor&31) || *cursor+count*32>0xfe0) return -1;
    unsigned animation=actor[6]|((unsigned)actor[7]<<8);
    if (animation<0x8000 || animation+3>=0xc000) return -1;
    uint8_t code=bank2[animation+2-0x8000];
    uint16_t x=(uint16_t)(actor[10]+((uint16_t)actor[11]<<8)-8u);
    uint8_t attr=bank2[animation+3-0x8000]|((x>>4)&0x10);
    for (unsigned i=0;i<count;++i) {
        unsigned dest=*cursor+i*32;
        /* The tail ADD uses the saved remainder, not the body tile code. */
        objects[dest]=(uint8_t)(i==whole ? (length&15)+actor[8] : code+(i?8u:0u));
        objects[dest+1]=attr;objects[dest+2]=(uint8_t)(actor[13]+i*16);
        objects[dest+3]=(uint8_t)x;
    }
    *cursor+=(uint16_t)(count*32);*remaining-=(uint8_t)count;
    return (int)count;
}
