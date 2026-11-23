#ifndef SPANG_SCENE_DATA_H
#define SPANG_SCENE_DATA_H
#include <stdint.h>
enum { SPANG_SCENE_BYTES=10240 };
/* Jo Engine LZSS format, with bounded input, history and output. */
int spang_scene_unpack(const uint8_t *source,unsigned length,uint8_t *out,unsigned capacity);
int spang_scene_load(unsigned scene,uint8_t out[SPANG_SCENE_BYTES]);
#endif
