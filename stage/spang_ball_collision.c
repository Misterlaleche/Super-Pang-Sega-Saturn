#include "spang_ball_collision.h"
int spang_ball_collision(uint8_t actor[32], uint16_t entry, uint16_t x,
    uint16_t bc, spang_ball_read_fn read, void *context)
{
    uint8_t y;
    switch (entry) {
    case 0x9e10:
        x = (uint16_t)(x - 4u);
        y = (uint8_t)(actor[0x0d] + (actor[0x12] == 2 ? 1u : 254u));
        break;
    case 0x9e21:
        x = (uint16_t)(x + 3u);
        y = (uint8_t)(actor[0x0d] + (actor[0x12] == 0 ? 1u : 254u));
        break;
    case 0x9e70:
        x = (uint16_t)(x + bc);
        y = (uint8_t)(actor[0x0d] - 4u);
        break;
    case 0x9eae:
        x = (uint16_t)(x + bc);
        y = (uint8_t)(actor[0x0d] + 3u);
        break;
    default: return -1;
    }
    const uint16_t cell = (uint16_t)(0xf000u + ((uint16_t)(y >> 3) << 6)
        + ((x >> 3) & 63u));
    if (!read(context, cell)) return 0;
    if (entry == 0x9eae && y >= 0xd0u && actor[0x0f] >= 0x26u)
        return -2;
    if (entry == 0x9e10 || entry == 0x9e21) {
        actor[0x0a] = (uint8_t)((actor[0x0a] & 0xf8u) + 4u);
        actor[0x12] = (uint8_t)((actor[0x12] + 2u) & 3u);
    } else if (entry == 0x9e70) {
        actor[0x0d] = (uint8_t)(y + ((~y) & 7u) + 5u);
        actor[0x12] = (uint8_t)(actor[0x12] - 1u);
    } else {
        if (y >= 0xd0u) {
            /* A0BF..A0E0 normal-type branch; EXX preserves caller BC. */
            const uint16_t table = (uint16_t)(0xa10bu
                + (uint8_t)(actor[0x0f] * 2u));
            actor[0x1e] = read(context, table);
            actor[0x1f] = read(context, (uint16_t)(table + 1u));
            actor[0x0e] = read(context, (uint16_t)(table + 2u));
        }
        actor[0x0d] = (uint8_t)(y - ((y & 7u) + 4u));
        actor[0x12] = (uint8_t)(actor[0x12] + 1u);
    }
    const uint8_t index = (uint8_t)(actor[0x12] * 2u);
    actor[0x02] = read(context, (uint16_t)(0x8200u + index));
    actor[0x03] = read(context, (uint16_t)(0x8200u + index + 1u));
    return 1;
}
