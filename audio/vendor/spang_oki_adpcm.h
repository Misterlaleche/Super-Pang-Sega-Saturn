#ifndef SPANG_OKI_ADPCM_H
#define SPANG_OKI_ADPCM_H
#include <stdint.h>
/* Integer OKI ADPCM decoder, matching the pinned MAME reference. Caller must
 * reset before first use; subsequent state must come from these functions.
 * Valid state:signal -2048..2047,step0..48. No voices, volume, banking,
 * timing, YM2413 or SCSP output. Source/destination/state do not overlap. */
typedef struct { int16_t signal; uint8_t step; } spang_oki_adpcm;
void spang_oki_adpcm_reset(spang_oki_adpcm *state);
/* Low four bits of nibble; returns signed12bit signal. */
int16_t spang_oki_adpcm_clock(spang_oki_adpcm *state,uint8_t nibble);
/* count packed bytes, high nibble first; writes count*2 PCM16 samples.
 * PCM16 = signal*16, independent of host endian. count0 touches no buffers.
 * Decoder state survives chunks. Destination must hold count*2 samples. */
void spang_oki_adpcm_decode(spang_oki_adpcm *state,const uint8_t *source,
                            uint32_t count,int16_t *output);
#endif
