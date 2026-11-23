#ifndef SPANG_ATTRACT_INPUT_H
#define SPANG_ATTRACT_INPUT_H
#include <stdint.h>
typedef struct {
    uint16_t pointer[2];
    uint8_t remaining[2],loaded;
} spang_attract_input;
/* Native BANK5:847F..84E1, original DATA pairs at9E6D. Pair0/1 are the
 * two tutorials,2/3 the Tour demonstrations,4 Panic. Outputs retain original
 * FCC0/FCC2 bits: left20,right10,fire08,up80,down40. No frame playback.
 * The first call loads without decrementing; duration00 means256 thereafter.
 * Return0 success, -1 for invalid arguments/ROM bounds. */
int spang_attract_input_init(spang_attract_input *,const uint8_t bank5[16384],unsigned pair);
int spang_attract_input_tick(spang_attract_input *,const uint8_t bank5[16384],uint8_t output[2]);
#endif
