#include "spang_geometry.h"
#include "spang_collision_tables.inc"

uint32_t spang_step_x(uint32_t position_q8, uint16_t delta_bits)
{
    const uint32_t increment = (delta_bits & 0x8000u)
        ? (0xffff0000u | (uint32_t)delta_bits) : (uint32_t)delta_bits;
    return (position_q8 + increment) & 0x00ffffffu;
}

int spang_harpoon_overlap(uint8_t selector, uint8_t b, uint8_t c,
                          uint8_t d, uint8_t e, uint8_t y)
{
    uint8_t center, width, height, diameter;
    const uint8_t (*table)[2] = 0;
    switch (selector) {
    case 1: center = 2; width = 5; height = 3; diameter = 7; break;
    case 2: center = 4; width = 9; height = 7; diameter = 15; break;
    case 3: center = 8; width = 17; table = profile_3; break;
    case 4: center = 12; width = 25; table = profile_4; break;
    default: center = 16; width = 33; table = profile_5; break;
    }
    const uint8_t column = (uint8_t)(c + center - b);
    if (column >= width) return 0;
    if (table) {
        height = table[column][0];
        diameter = table[column][1];
    }
    const uint8_t bottom = (uint8_t)(y + height);
    if (bottom < e) return 0;
    const uint8_t top = (uint8_t)(bottom - diameter);
    if (top >= d) return 0;
    return 1;
}
