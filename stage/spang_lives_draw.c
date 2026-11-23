#include "spang_lives_draw.h"
static uint16_t inc_low(uint16_t address)
{
    return (uint16_t)((address & 0xff00) | ((address + 1) & 255));
}
static uint16_t icon(uint16_t dest, uint8_t tile, uint8_t high, uint8_t attr,
                     spang_video_write write, void *context)
{
    uint16_t p = dest, a;
    write(context, p, tile); p = inc_low(p);
    write(context, p, high); p = inc_low(p);
    write(context, p, (uint8_t)(tile + 1)); p = inc_low(p);
    write(context, p, high); p = (uint16_t)(p + 0x7d);
    write(context, p, (uint8_t)(tile + 16)); p = inc_low(p);
    write(context, p, high); p = inc_low(p);
    write(context, p, (uint8_t)(tile + 17)); p = inc_low(p);
    write(context, p, high); p = (uint16_t)(p + 0xff81);
    a = (uint16_t)(0xc800 + ((dest & 0x0fff) >> 1));
    write(context, a, attr); a = inc_low(a);
    write(context, a, attr); a = (uint16_t)(a + 0x3f);
    write(context, a, attr); a = inc_low(a);
    write(context, a, attr);
    return p;
}
static uint16_t digit(uint16_t dest, uint8_t value,
                      spang_video_write write, void *context)
{
    uint16_t next = inc_low(dest);
    write(context, dest, value);
    write(context, next, 0);
    next = inc_low(next);
    write(context, (uint16_t)(0xc800 + ((dest & 0x0fff) >> 1)), 0x10);
    return next;
}
void spang_lives_draw(uint8_t player, uint8_t lives, uint8_t globals[3],
                      spang_video_write write, void *context)
{
    uint16_t dest = player ? 0xdeda : 0xde94;
    uint8_t count = (uint8_t)(lives - 1), value, correction;
    unsigned i;
    globals[0] = player ? 1 : 2;
    globals[1] = player ? 0x24 : 0xe4;
    globals[2] = player ? 7 : 6;
    if (!count) return;
    for (i = 0; i < (count < 4 ? count : 3); ++i)
        dest = icon(dest, globals[1], globals[2], globals[0], write, context);
    /* DEC A; DAA. Carry from the final valid attribute-address ADD is zero. */
    value = (uint8_t)(lives - 1);
    correction = ((lives & 15) == 0 || (value & 15) > 9) ? 6 : 0;
    if (value > 0x99) correction = (uint8_t)(correction + 0x60);
    value = (uint8_t)(value - correction);
    if (value < 4) return;
    dest = (uint16_t)(dest + 0x80);
    if (value >> 4) dest = digit(dest, (uint8_t)(value >> 4), write, context);
    (void)digit(dest, (uint8_t)(value & 15), write, context);
}
