#include "spang_bonus_life.h"
static uint8_t increment_lives(uint8_t lives)
{
    unsigned sum = (unsigned)lives + 1;
    unsigned value = sum & 255;
    unsigned adjust = ((lives & 15) == 15 || (value & 15) > 9) ? 6 : 0;
    if (sum > 255 || value > 0x99) adjust += 0x60;
    return (uint8_t)(value + adjust);
}
static uint16_t threshold(const uint8_t state[4])
{
    return (uint16_t)(state[1] | ((uint16_t)state[2] << 8));
}
static void next_threshold(uint8_t state[4], const uint8_t table[8])
{
    if (state[0] == 8) { state[1] = 0; state[2] = 0x90; }
    else { state[1] = table[state[0]]; state[2] = table[state[0] + 1]; }
}
int spang_bonus_life(uint8_t state[4], uint16_t score_high, const uint8_t table[8],
                     uint8_t audio_mode, uint8_t audio_enabled,
                     uint8_t ui_disabled, uint8_t effects[2])
{
    int awards = 0;
    if (state[0] > 10 || (state[0] & 1)) return -1;
    effects[0] = effects[1] = 0;
    if (state[0] == 10 || score_high < threshold(state)) return 0;
    state[3] = increment_lives(state[3]);
    ++awards;
    effects[0] = audio_mode != 0 || audio_enabled != 0;
    state[0] = (uint8_t)(state[0] + 2);
    if (state[0] == 10) { effects[1] = ui_disabled == 0; return awards; }
    next_threshold(state, table);
    while (score_high >= threshold(state)) {
        state[3] = increment_lives(state[3]);
        ++awards;
        state[0] = (uint8_t)(state[0] + 2);
        /* 2D95 returns directly: no redraw on this specific terminal path. */
        if (state[0] == 10) return awards;
        next_threshold(state, table);
    }
    effects[1] = ui_disabled == 0;
    return awards;
}
