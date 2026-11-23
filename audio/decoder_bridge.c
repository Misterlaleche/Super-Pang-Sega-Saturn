#include "audio_c13_codec.h"
#include "spang_rice.h"
#include "spang_oki_adpcm.h"
int decode(const unsigned char *s,unsigned n,unsigned char *d){return c13_decode_block(s,n,d);}
int rice(const unsigned char *p,unsigned n,unsigned count,short *out){
 spang_rice r;if(spang_rice_init(&r,p,n,0,0,0,n,count))return -1;
 for(unsigned i=0;i<count;i++)if(spang_rice_next(&r,out+i)!=1)return -2;
 return spang_rice_next(&r,out)!=0?-3:0;
}
void oki(const unsigned char *p,unsigned bytes,short *out){
 spang_oki_adpcm s;spang_oki_adpcm_reset(&s);
 for(unsigned i=0;i<bytes*2;i++){
  unsigned nibble=(i&1)?p[i/2]:p[i/2]>>4;
  out[i]=(short)((int)spang_oki_adpcm_clock(&s,nibble)*32*3/20);
 }
}
