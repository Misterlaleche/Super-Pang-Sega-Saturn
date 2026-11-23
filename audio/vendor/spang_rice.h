#ifndef SPANG_RICE_H
#define SPANG_RICE_H
#include <stdint.h>
/* Lossless signed PCM16 delta/Rice stream. Blocks contain at most64 samples:
 * reset predictor0, read8bit k(0..16), then unary zero quotient/one delimiter
 * and k-bit remainder, MSB first. Zigzag maps negative delta to -2*delta-1.
 * Blocks are bit-contiguous; each asset ends with zero padding to a byte.
 * Two immutable, non-overlapping views form one concatenated byte address
 * space. Buffers must remain valid throughout decoding; do not modify fields.
 * Serialized use only; no allocation, whole-effect buffer, or device I/O. */
typedef struct {
    const uint8_t *low, *high;
    uint32_t low_size, position, end, remaining;
    int32_t previous;
    uint8_t current, bits, block_samples, k, error;
} spang_rice;
/* Non-null decoder required. Null view allowed only for zero view size.
 * offset/bytes bound exactly one asset. Return0 initialized,-1 invalid.
 * Invalid initialization leaves a latched error, safe to pass to next. */
int spang_rice_init(spang_rice *decoder,const uint8_t *low,uint32_t low_size,
                    const uint8_t *high,uint32_t high_size,uint32_t offset,
                    uint32_t bytes,uint32_t samples);
/* Non-null output required. Return1 sample,0 end,-1 corrupt/truncated stream.
 * Output is unchanged on0/-1; errors latch. Checks PCM range, k, maximum
 * possible delta, exact byte extent and zero final padding. */
int spang_rice_next(spang_rice *decoder,int16_t *output);
#endif
