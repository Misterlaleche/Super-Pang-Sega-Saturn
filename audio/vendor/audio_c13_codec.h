#ifndef AUDIO_C13_CODEC_H
#define AUDIO_C13_CODEC_H
#include <stdint.h>
#include <string.h>
/* C13L v1: 1024 PCM16 BE samples. BE16 total block bytes, then tag.
 * Tag f0 is raw; otherwise high nibble is predictor order 0..2 and low
 * nibble Rice k 0..15. Predictors start at zero each block. Residuals wrap
 * modulo 65536 before signed zigzag, so every int16 input is exact.
 * Rice uses q zero bits, a one bit, k remainder bits; trailing bits zero. */

/* Left-aligned reservoir. SH-2 has no variable barrel shift: use its 32x32
 * multiplier for dynamic positioning, instead of libgcc shifts per sample. */
typedef struct {const uint8_t *p,*end;uint32_t bits;unsigned have;} c13_bits;
static const uint32_t c13_powers[]={
 1u,2u,4u,8u,16u,32u,64u,128u,256u,512u,1024u,2048u,4096u,8192u,16384u,32768u,
 65536u,131072u,262144u,524288u,1048576u,2097152u,4194304u,8388608u,16777216u};
static inline int c13_fill(c13_bits*s,unsigned need){
 while(s->have<need){
  if(s->p==s->end)return -1;
  s->bits|=(uint32_t)*s->p++ * c13_powers[24-s->have];s->have+=8;
 }return 0;
}
static inline uint32_t c13_take(c13_bits*s,uint32_t scale){
 uint32_t top,rest;
#if defined(__sh__)
 __asm__ volatile("dmulu.l %2,%3\n\tsts mach,%0\n\tsts macl,%1"
  : "=r"(top),"=r"(rest):"r"(s->bits),"r"(scale):"mach","macl");
#else
 uint64_t product=(uint64_t)s->bits*scale;top=(uint32_t)(product>>32);rest=(uint32_t)product;
#endif
 s->bits=rest;return top;
}
static __attribute__((noinline)) int c13_decode_block(const uint8_t*src,unsigned bytes,uint8_t*dst){
 if(bytes<3||bytes>2051||(((unsigned)src[0]<<8)|src[1])!=bytes)return -1;
 unsigned tag=src[2],order=tag>>4,k=tag&15;
 if(tag==0xf0){if(bytes!=2051)return -1;memcpy(dst,src+3,2048);return 0;}
 if(order>2)return -1;
 c13_bits s={src+3,src+bytes,0,0};uint32_t previous=0,older=0,scale=c13_powers[k],maximum=65535u>>k;
 for(unsigned i=0;i<1024;i++){
  uint32_t q=0;
  if(c13_fill(&s,k+1))return -1;
  while(!(s.bits&0x80000000u)){
   s.bits<<=1;--s.have;if(++q>maximum)return -1;
   if(!s.have&&c13_fill(&s,1))return -1;
  }
  s.bits<<=1;--s.have;
  if(c13_fill(&s,k))return -1;
  uint32_t u=q*scale+c13_take(&s,scale);s.have-=k;
  uint32_t delta=(u>>1)^(0u-(u&1));
  uint32_t prediction=order==0?0:order==1?previous:2u*previous-older;
  uint32_t value=(prediction+delta)&65535u;older=previous;previous=value;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  ((uint16_t*)(void*)dst)[i]=(uint16_t)value;
#else
  dst[2*i]=(uint8_t)(value>>8);dst[2*i+1]=(uint8_t)value;
#endif
 }
 if(s.p!=s.end||s.bits)return -1;
 return 0;
}
#endif
