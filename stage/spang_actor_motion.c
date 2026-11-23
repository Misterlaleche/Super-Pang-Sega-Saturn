#include "spang_actor_motion.h"
#include "spang_geometry.h"

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void step_position(uint8_t *p, uint16_t delta)
{
    uint32_t position = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16);
    position = spang_step_x(position, delta);
    p[0] = (uint8_t)position;
    p[1] = (uint8_t)(position >> 8);
    p[2] = (uint8_t)(position >> 16);
}

void spang_actor_step_x(uint8_t actor[32])
{
    step_position(actor + 0x09, read16(actor + 0x14));
}

void spang_actor_step_y(uint8_t actor[32], uint16_t incoming_delta)
{
    const uint32_t sum = (uint32_t)read16(actor + 0x16) + incoming_delta;
    const uint32_t low = sum & 0xffffu;
    const uint32_t carry = sum >> 16;
    /* ADD HL,DE's carry survives LD DE,0700 and feeds SBC HL,DE.
     * The borrow branch restores 0700, not that carry. */
    const uint16_t velocity = low >= 0x0700u + carry
        ? 0x0700u : (uint16_t)(low - carry);
    actor[0x16] = (uint8_t)velocity;
    actor[0x17] = (uint8_t)(velocity >> 8);
    step_position(actor + 0x0c, velocity);
}
