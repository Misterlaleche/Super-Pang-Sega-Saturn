#include "spang_small_ball.h"
#include "spang_ball_collision.h"
int spang_small_ball_update(uint8_t actor[32], uint16_t phase_step,
    uint16_t dx, uint8_t freeze, spang_ball_read_fn read, void *context,
    uint8_t *flag_e0aa)
{
    const uint16_t state = (uint16_t)(actor[2] | ((uint16_t)actor[3] << 8));
    if (state != 0x8357 && state != 0x8395 && state != 0x83d5 && state != 0x8416)
        return -1;
    if (freeze) return -2;
    uint8_t work[32], flag = *flag_e0aa;
    for (unsigned i = 0; i < 32; ++i) work[i] = actor[i];
    const int left = state == 0x83d5 || state == 0x8416;
    const int reverse = state == 0x8395 || state == 0x8416;
    const uint8_t carry = spang_ball_phase_step(work, phase_step, reverse,
        read, context, &flag);
    uint32_t x = (uint32_t)work[9] | ((uint32_t)work[10] << 8)
        | ((uint32_t)work[11] << 16);
    x = (left ? x - dx : x + dx) & 0xffffffu;
    work[9] = (uint8_t)x;
    work[10] = (uint8_t)(x >> 8);
    work[11] = (uint8_t)(x >> 16);
    /* Original PUSH/POP HL preserves this coordinate across first collision. */
    const uint16_t integer_x = (uint16_t)(x >> 8);
    const uint16_t vertical = reverse && !carry ? 0x9e70 : 0x9eae;
    int result = spang_ball_collision(work, vertical, integer_x,
        left ? 0xfffe : 1, read, context);
    if (result < 0) return -2;
    result = spang_ball_collision(work, left ? 0x9e10 : 0x9e21,
        integer_x, 0, read, context);
    if (result < 0) return -2;
    work[0x1b] = (uint8_t)(((uint16_t)work[10]
        | ((uint16_t)work[11] << 8)) >> 1);
    for (unsigned i = 0; i < 32; ++i) actor[i] = work[i];
    *flag_e0aa = flag;
    return 0;
}
