#include "spang_video_commands.h"
static uint16_t low_inc(uint16_t address)
{
    return (uint16_t)((address & 0xff00) | ((address + 1) & 255));
}
static int get(const uint8_t *bank, uint16_t *source, uint8_t *value)
{
    if (*source < 0x8000 || *source >= 0xc000) return -1;
    *value = bank[*source - 0x8000];
    ++*source;
    return 0;
}
static int get_word(const uint8_t *bank, uint16_t *source, uint16_t *value)
{
    uint8_t lo, hi;
    if (get(bank, source, &lo) || get(bank, source, &hi)) return -1;
    *value = (uint16_t)(lo | ((uint16_t)hi << 8));
    return 0;
}
static int emit(spang_video_write write, void *context, uint16_t address, uint8_t value)
{
    if (address < 0xc800 || address > 0xdfff) return -1;
    write(context, address, value);
    return 0;
}
int spang_video_command(uint16_t handler, uint8_t parameter,
                        const uint8_t bank10[16384],
                        spang_video_write write, void *context)
{
    uint16_t source, table;
    if (handler == 0x116d || handler == 0x11e8) table = 0x8000;
    else if (handler == 0x1496) table = 0x80a0;
    else if (handler == 0x11a6) table = 0x8140;
    else return -1;
    table = (uint16_t)(table + parameter);
    if (get_word(bank10, &table, &source)) return -1;
    return spang_video_stream(handler,source,bank10,write,context);
}
int spang_video_stream(uint16_t handler,uint16_t source,const uint8_t bank10[16384],spang_video_write write,void *context)
{
    uint16_t dest=0,attr_dest=0;uint8_t attr=0;int header=1;
    if(handler==0x14a5)handler=0x1496;
    if(handler==0x117c)handler=0x116d;
    if(handler!=0x116d && handler!=0x11e8 && handler!=0x11a6 && handler!=0x1496)return -1;
    for (;;) {
        uint8_t tile, high;
        if (header) {
            if (get_word(bank10, &source, &dest) || get(bank10, &source, &attr)) return -1;
            attr_dest = (uint16_t)(0xc800 + ((dest & 0x0fff) >> 1));
            header = 0;
        }
        if (get(bank10, &source, &tile)) return -1;
        if (handler == 0x116d || handler == 0x11e8) {
            if (tile == 0) return 0;
            if (tile == 0x2f) { header = 1; continue; }
            if (emit(write, context, dest, tile)) return -1;
            dest = low_inc(dest);
            if (emit(write, context, dest, handler == 0x11e8 ? 2 : 0)) return -1;
            dest = low_inc(dest);
            if (emit(write, context, attr_dest, attr)) return -1;
            attr_dest = low_inc(attr_dest);
        } else {
            uint16_t next, a;
            if (get(bank10, &source, &high)) return -1;
            if (high == 0xff) return 0;
            if (high == 0xfe) { header = 1; continue; }
            if (handler == 0x11a6) {
                if (emit(write, context, dest, tile)) return -1;
                dest = low_inc(dest);
                if (emit(write, context, dest, high)) return -1;
                dest = low_inc(dest);
                if (emit(write, context, attr_dest, attr)) return -1;
                attr_dest = low_inc(attr_dest);
                continue;
            }
            next = dest;
            if (emit(write, context, next, tile)) return -1;
            next = low_inc(next);
            if (emit(write, context, next, high)) return -1;
            next = low_inc(next);
            ++tile;
            if (emit(write, context, next, tile)) return -1;
            next = low_inc(next);
            if (emit(write, context, next, high)) return -1;
            next = (uint16_t)(next + 0x7d);
            tile = (uint8_t)(tile + 15);
            if (emit(write, context, next, tile)) return -1;
            next = low_inc(next);
            if (emit(write, context, next, high)) return -1;
            next = low_inc(next);
            ++tile;
            if (emit(write, context, next, tile)) return -1;
            next = low_inc(next);
            if (emit(write, context, next, high)) return -1;
            next = (uint16_t)(next + 0xff81);
            a = (uint16_t)(0xc800 + ((dest & 0x0fff) >> 1));
            if (emit(write, context, a, attr)) return -1;
            a = low_inc(a);
            if (emit(write, context, a, attr)) return -1;
            a = (uint16_t)(a + 0x3f);
            if (emit(write, context, a, attr)) return -1;
            a = low_inc(a);
            if (emit(write, context, a, attr)) return -1;
            dest = next;
        }
    }
}
