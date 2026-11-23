#include "spang_rice.h"
/* Maximum possible zigzag quotient for each k. Avoid SH-2 variable-shift
 * helpers in the per-sample/per-bit hot path. */
static const uint32_t quotient_limits[17]={
    131070,65535,32767,16383,8191,4095,2047,1023,511,
    255,127,63,31,15,7,3,1
};
static const uint8_t bit_masks[8]={1,2,4,8,16,32,64,128};
static const uint8_t low_masks[9]={0,1,3,7,15,31,63,127,255};
int spang_rice_init(spang_rice *d,const uint8_t *low,uint32_t low_size,
                    const uint8_t *high,uint32_t high_size,uint32_t offset,
                    uint32_t bytes,uint32_t samples)
{
    d->error = 1;
    if ((!low && low_size) || (!high && high_size) ||
        high_size > UINT32_MAX-low_size) return -1;
    uint32_t total = low_size+high_size;
    if (offset > total || bytes > total-offset || (!samples != !bytes)) return -1;
    d->low=low;d->high=high;d->low_size=low_size;
    d->position=offset;d->end=offset+bytes;d->remaining=samples;
    d->previous=0;d->current=0;d->bits=0;d->block_samples=0;d->k=0;
    d->error=0;
    return 0;
}
static inline __attribute__((always_inline)) int read_byte(spang_rice *d)
{
    if (d->position==d->end) {d->error=1;return -1;}
    uint32_t p=d->position++;
    return p<d->low_size?d->low[p]:d->high[p-d->low_size];
}
static inline __attribute__((always_inline)) int bit(spang_rice *d)
{
    if (!d->bits) {
        int value=read_byte(d);if(value<0)return -1;
        d->current=(uint8_t)value;d->bits=8;
    }
    unsigned mask=bit_masks[--d->bits];
    int value=(d->current & mask)!=0;
    d->current=(uint8_t)(d->current & (mask-1));
    return value;
}
/* Read at most16 bits. Append whole bytes with fixed shifts, retaining only
 * the right-aligned partial byte. One small variable shift replaces up to16
 * per-bit calls/branches. current/bits remain bounded to one partial byte. */
static int bits(spang_rice *d,unsigned count)
{
    if (!count) return 0;
    uint32_t value=d->current;
    unsigned available=d->bits;
    while (available<count) {
        int byte=read_byte(d);if(byte<0)return -1;
        value=(value << 8)|(unsigned)byte;available+=8;
    }
    unsigned left=available-count;
    d->current=(uint8_t)(value & low_masks[left]);d->bits=(uint8_t)left;
    return (int)(value >> left);
}
static int fail(spang_rice *d) {d->error=1;return -1;}
int spang_rice_next(spang_rice *d,int16_t *out)
{
    if (d->error) return -1;
    if (!d->remaining) return 0;
    if (!d->block_samples) {
        int k=bits(d,8);if(k<0)return -1;
        if (k>16) return fail(d);
        d->k=(uint8_t)k;d->previous=0;
    }
    uint32_t quotient=0;
    uint32_t limit=quotient_limits[d->k];
    for (;;) {
        int b=bit(d);if (b<0) return -1;
        if (b) break;
        if (++quotient>limit) return fail(d);
    }
    int remainder=bits(d,d->k);if(remainder<0)return -1;
    uint32_t encoded=(quotient << d->k)|(uint32_t)remainder;
    if (encoded>131070u) return fail(d);
    int32_t delta=(encoded&1)?-(int32_t)(encoded>>1)-1:(int32_t)(encoded>>1);
    int32_t value=d->previous+delta;
    if (value < -32768 || value > 32767) return fail(d);
    if (d->remaining==1 && (d->position!=d->end || d->current)) return fail(d);
    d->previous=value;--d->remaining;
    d->block_samples=(uint8_t)((d->block_samples+1)&63);
    *out=(int16_t)value;
    return 1;
}
