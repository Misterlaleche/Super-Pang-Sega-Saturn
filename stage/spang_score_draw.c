#include "spang_score_draw.h"
void spang_score_draw(const uint8_t score[4], uint16_t destination,
                      int suppressed, spang_video_write write, void *context)
{
    unsigned i;
    int started = 0;
    if (suppressed) return;
    for (i = 0; i < 7; ++i) {
        uint8_t digit = (uint8_t)((score[i >> 1] >> ((i & 1) ? 0 : 4)) & 15);
        if (digit) started = 1;
        if (started) write(context, destination, digit);
        destination = (uint16_t)((destination & 0xff00) | ((destination + 2) & 255));
    }
}
