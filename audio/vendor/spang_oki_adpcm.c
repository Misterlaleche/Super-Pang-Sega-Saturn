/* SPDX-License-Identifier: BSD-3-Clause
 * OKI algorithm adapted from MAME okiadpcm.cpp, Andrew Gardner/Aaron Giles.
 * Reference retained in reference/vendor/mame; full license terms in
 * reference/licenses/MAME-OKI-BSD-3-Clause.txt. */
#include "spang_oki_adpcm.h"
/* floor(16*(11/10)^step), precomputed to avoid floating point on SH-2. */
static const uint16_t steps[49] = {16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552};
static const int8_t shifts[8] = {-1,-1,-1,-1,2,4,6,8};
void spang_oki_adpcm_reset(spang_oki_adpcm *s)
{
    s->signal = 0;
    s->step = 0;
}
int16_t spang_oki_adpcm_clock(spang_oki_adpcm *s,uint8_t n)
{
    unsigned step = steps[s->step];
    int difference = (int)(step >> 3);
    int signal, index;
    if (n & 1) difference += (int)(step >> 2);
    if (n & 2) difference += (int)(step >> 1);
    if (n & 4) difference += (int)step;
    signal = s->signal + ((n & 8) ? -difference : difference);
    if (signal > 2047) signal = 2047;
    if (signal < -2048) signal = -2048;
    index = s->step + shifts[n & 7];
    if (index > 48) index = 48;
    if (index < 0) index = 0;
    s->signal = (int16_t)signal;
    s->step = (uint8_t)index;
    return s->signal;
}
void spang_oki_adpcm_decode(spang_oki_adpcm *s,const uint8_t *p,uint32_t n,int16_t *o)
{
    while (n--) {
        uint8_t value = *p++;
        *o++ = (int16_t)(spang_oki_adpcm_clock(s,value >> 4) * 16);
        *o++ = (int16_t)(spang_oki_adpcm_clock(s,value) * 16);
    }
}
