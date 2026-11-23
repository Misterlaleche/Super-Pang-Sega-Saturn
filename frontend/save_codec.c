#include "save_codec.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t crc32(const uint8_t *p,unsigned n)
{
 uint32_t crc=0xffffffffu;
 while(n--){crc^=*p++;for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
 return ~crc;
}
void spang_save_encode(uint8_t out[216],const uint8_t records[200],uint32_t generation)
{
 memcpy(out,"SPANGS02",8);put32(out+8,generation);memcpy(out+12,records,200);put32(out+212,crc32(out,212));
}
int spang_save_decode(const uint8_t *data,unsigned length,uint8_t records[200],uint32_t *generation)
{
 if(!data)return 0;
 if(length==208 && !memcmp(data,"SPANGS01",8) && get32(data+204)==crc32(data,204)){
  static const uint8_t defaults[8]={1,3,1,0,10,10,0,0};
  memcpy(records,data+12,192);memcpy(records+192,defaults,8);
 }else{
  if(length!=216||memcmp(data,"SPANGS02",8)||get32(data+212)!=crc32(data,212))return 0;
  memcpy(records,data+12,200);
 }
 *generation=get32(data+8);return 1;
}
int spang_save_newer(uint32_t candidate,uint32_t previous)
{uint32_t distance=candidate-previous;return distance!=0&&distance<0x80000000u;}
