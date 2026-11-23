#include "spang_player_control.h"
#include "spang_actor_motion.h"
int spang_player_side(uint8_t actor[128], const uint8_t grid[2048])
{
    const int left = actor[0x15] >= 0x80;
    const uint16_t x = actor[10] | ((uint16_t)actor[11] << 8);
    const uint16_t edge = (uint16_t)(x + (left ? -8 : 7));
    unsigned row = (uint8_t)(actor[13]-9u) >> 3;
    uint16_t address = (uint16_t)(0xf000 | (row << 6) | ((edge & 511) >> 3));
    int hit = 0;
    for (unsigned i=0;i<3;++i,address+=64) {
        if (address<0xf000 || address>=0xf800) return -1;
        if (grid[address-0xf000]) {hit=1;break;}
    }
    if (!hit) {
        if (address<0xf000 || address>=0xf800) return -1;
        if (!grid[address-0xf000]) return 0;
        address=(address&0xff00)|(uint8_t)(address+(left?1:-1));
        if (address<0xf000 || address>=0xf800) return -1;
        if (!grid[address-0xf000]) return 1;
        address=(uint16_t)(address+(left?0xfeff:0xff01));
        if (address<0xf000 || address>=0xf800) return -1;
        if (!grid[address-0xf000]) return 0;
    }
    unsigned delta=((left?(uint8_t)~edge:(uint8_t)edge)&7)+1;
    if (delta>=3) return 0;
    uint16_t corrected=(uint16_t)(left?x+delta:x-delta);
    actor[10]=(uint8_t)corrected;actor[11]=(uint8_t)(corrected>>8);
    actor[0x5b]=0;actor[0x5c]=0;
    return left ? x+delta>65535 : x<delta;
}
int spang_player_animate(uint8_t actor[128], const uint8_t bank0[16384])
{
    uint8_t timer=(uint8_t)(actor[8]-1);
    if (timer) {actor[8]=timer;return 0;}
    unsigned current=actor[6]|((unsigned)actor[7]<<8);
    if (current<0x8000 || current+6>=0xc000) return -1;
    unsigned next=current+5;
    if (bank0[current-0x8000]>=0x80)
        next=bank0[next-0x8000]|((unsigned)bank0[next+1-0x8000]<<8);
    if (next<0x8000 || next>=0xc000) return -1;
    actor[6]=(uint8_t)next;actor[7]=(uint8_t)(next>>8);
    actor[8]=bank0[next-0x8000]&0x7f;
    return 0;
}
int spang_player_sprite(const uint8_t actor[128], uint8_t code, uint8_t attr,
                       uint8_t frame, uint8_t objects[4096],
                       uint16_t *cursor, uint8_t *remaining)
{
    if (*remaining < 4) return 0;
    if ((*cursor & 31) || *cursor > 0xf80) return -1;
    if (actor[0x23] && (frame & 4)) attr = (attr & 0xf0) | 9;
    uint16_t x = (uint16_t)(actor[10] + ((uint16_t)actor[11] << 8) - 16u);
    const uint8_t left_attr = attr | ((x >> 4) & 0x10);
    const uint8_t right_attr = left_attr | (((uint16_t)(x + 16u) >> 4) & 0x10);
    const uint8_t codes[4] = {code, (uint8_t)(code+8), (uint8_t)(code+9), (uint8_t)(code+1)};
    for (unsigned i=0; i<4; ++i) {
        unsigned dest = *cursor + i*32;
        objects[dest] = codes[i];
        objects[dest+1] = i<2 ? left_attr : right_attr;
        objects[dest+2] = (uint8_t)(actor[13] - ((i==0 || i==3) ? 16u : 0u));
        objects[dest+3] = (uint8_t)(x + (i>=2 ? 16u : 0u));
    }
    *cursor += 128;
    *remaining -= 4;
    return 4;
}
int spang_player_walk(uint8_t actor[128], int direction)
{
    /* Bank 0 AF93/AFD5 normal floor, tail call fixed 1302. Ice is separate. */
    if (actor[0x5c] || (direction != -1 && direction != 1)) return -1;
    actor[0x14] = 0;
    actor[0x15] = direction == 1 ? 2 : 0xfe;
    spang_actor_step_x(actor);
    return 0;
}
int spang_player_input(uint8_t actor[128], uint8_t input, uint8_t previous,
                      const uint8_t bank0[16384])
{
    /* ABDE..AC50, ADEB and 12B8. Bit 3 rising edge dispatches weapons;
     * reject that whole path before any writes. Do not reorder directions. */
    if ((input & (uint8_t)~previous) & 8) return -1;
    unsigned state = 0, facing = actor[0x21], table = 0xadf8;
    if (input & 0x20) { state = 8; facing = 1; table = 0xae00; }
    else if (input & 0x10) { state = 6; facing = 0; table = 0xae00; }
    else if (input & 0x80) state = 2;
    else if (input & 0x40) state = 4;
    if (actor[0x20] == state) return 0;
    const unsigned index = (uint8_t)((uint8_t)(actor[0x11] * 2u) + facing);
    const unsigned address = table + (uint8_t)(index * 2u);
    if (address < 0x8000 || address + 1 >= 0xc000) return -1;
    const unsigned animation = bank0[address - 0x8000]
        | ((unsigned)bank0[address + 1 - 0x8000] << 8);
    if (animation < 0x8000 || animation >= 0xc000) return -1;
    actor[0x20] = (uint8_t)state;
    actor[0x21] = (uint8_t)facing;
    actor[6] = (uint8_t)animation;
    actor[7] = (uint8_t)(animation >> 8);
    actor[8] = bank0[animation - 0x8000] & 0x7f;
    return 0;
}
