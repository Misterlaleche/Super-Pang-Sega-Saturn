#include "spang_score.h"
static uint8_t decimal_add(uint8_t left, uint8_t right, unsigned *carry)
{
    unsigned cin = *carry;
    unsigned sum = (unsigned)left + right + cin;
    unsigned result = sum & 255;
    unsigned adjust = 0;
    if (((left & 15) + (right & 15) + cin > 15) || (result & 15) > 9) adjust += 6;
    *carry = sum > 255 || result > 0x99;
    if (*carry) adjust += 0x60;
    return (uint8_t)(result + adjust);
}
void spang_score_add(uint8_t score[4], uint16_t amount)
{
    unsigned carry = 0;
    score[2] = decimal_add(score[2], (uint8_t)amount, &carry);
    score[1] = decimal_add(score[1], (uint8_t)(amount >> 8), &carry);
    if (!carry) return;
    score[0] = decimal_add(score[0], 0, &carry);
    if (carry) {
        score[0] = score[1] = score[2] = 0x99;
        score[3] = 0x90;
    }
}
void spang_score_add_low(uint8_t score[4], uint16_t amount, uint8_t low)
{
    unsigned carry = 0;
    uint8_t e = (uint8_t)amount;
    score[3] = decimal_add(score[3], low, &carry);
    if (carry) ++e;
    spang_score_add(score, (uint16_t)((amount & 0xff00) | e));
}
int spang_score_promote(uint8_t best[4], const uint8_t score[4])
{
    unsigned i;
    for (i = 0; i < 3; ++i) {
        if (score[i] < best[i]) return 0;
        if (score[i] > best[i]) break;
    }
    if (i == 3 && score[3] <= best[2]) return 0;
    /* Original LDDR copies from the least significant byte backwards. */
    for (i = 4; i != 0; --i) best[i - 1] = score[i - 1];
    return 1;
}
