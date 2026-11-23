/* Host-only encoder. The Saturn only links the small, bounded decoder. */
#include <stdint.h>
#include <string.h>
static void put(uint8_t*dst,unsigned*bit,uint32_t value,unsigned n){
 for(unsigned j=n;j;j--){dst[*bit/8]|=(uint8_t)(((value>>(j-1))&1u)<<(7-*bit%8));++*bit;}
}
unsigned c13_encode_block(const uint8_t*src,uint8_t*dst){
 uint32_t residual[3][1024],cost[3][16]={{0}},previous=0,older=0;
 for(unsigned i=0;i<1024;i++){
  uint32_t value=((uint32_t)src[2*i]<<8)|src[2*i+1];
  for(unsigned order=0;order<3;order++){
   uint32_t prediction=order==0?0:order==1?previous:2u*previous-older;
   uint32_t d=(value-prediction)&65535u;
   uint32_t u=d&32768u?((65536u-d)*2u-1u):d*2u;residual[order][i]=u;
   for(unsigned k=0;k<16;k++)cost[order][k]+=(u>>k)+1+k;
  }older=previous;previous=value;
 }
 unsigned best=16384,order_best=15,k_best=0;
 for(unsigned o=0;o<3;o++)for(unsigned k=0;k<16;k++)if(cost[o][k]<best){best=cost[o][k];order_best=o;k_best=k;}
 unsigned bytes=3+(best+7)/8;memset(dst,0,bytes);dst[0]=(uint8_t)(bytes>>8);dst[1]=(uint8_t)bytes;dst[2]=(uint8_t)((order_best<<4)|k_best);
 if(order_best==15){memcpy(dst+3,src,2048);return bytes;}
 unsigned bit=24;
 for(unsigned i=0;i<1024;i++){
  uint32_t u=residual[order_best][i];bit+=u>>k_best;
  put(dst,&bit,1,1);put(dst,&bit,u&((1u<<k_best)-1u),k_best);
 }return bytes;
}
