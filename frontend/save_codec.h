#ifndef SPANG_SAVE_CODEC_H
#define SPANG_SAVE_CODEC_H
#include <stdint.h>
enum {SPANG_SAVE_RECORD_BYTES=192,SPANG_SAVE_PAYLOAD_BYTES=200,SPANG_SAVE_BYTES=216};
void spang_save_encode(uint8_t out[216],const uint8_t records[200],uint32_t generation);
int spang_save_decode(const uint8_t *data,unsigned length,uint8_t records[200],uint32_t *generation);
int spang_save_newer(uint32_t candidate,uint32_t previous);
#endif
