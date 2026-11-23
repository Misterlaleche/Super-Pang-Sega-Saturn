#include "spang_ball_phase.h"
uint8_t spang_ball_phase_step(uint8_t actor[32], uint16_t step, int reverse,
    spang_ball_read_fn read, void *context, uint8_t *flag_e0aa)
{
    uint16_t phase = (uint16_t)((uint16_t)actor[0x1e]
        | ((uint16_t)actor[0x1f] << 8));
    const int reflected = reverse && phase < step;
    if (reverse)
        phase = reflected ? (uint16_t)(step - phase)
                          : (uint16_t)(phase - step);
    else
        phase = (uint16_t)(phase + step);
    *flag_e0aa = 0;
    actor[0x1e] = (uint8_t)phase;
    actor[0x1f] = (uint8_t)(phase >> 8);
    const uint16_t height = (uint16_t)actor[0x0e]
        + read(context, (uint16_t)(0xb535u + phase));
    actor[0x0d] = (uint8_t)height;
    if (reflected) {
        actor[0x12] = (uint8_t)(actor[0x12] - 1u);
        const uint8_t index = (uint8_t)((actor[0x0f] * 2u
            + actor[0x12]) * 2u);
        actor[0x02] = read(context, (uint16_t)(0x8200u + index));
        actor[0x03] = read(context, (uint16_t)(0x8200u + index + 1u));
        return 1;
    }
    return (uint8_t)(height >> 8);
}
