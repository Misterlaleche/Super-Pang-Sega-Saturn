#ifndef R04_FAUNA_INTERNAL_H
#define R04_FAUNA_INTERNAL_H
#include "fauna.h"
#include <string.h>
static inline unsigned fa_word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static inline void fa_put(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static inline unsigned fa_bw(const uint8_t *b,unsigned a){return fa_word(b+(a-0x8000));}
static inline void fa_move(uint8_t *p,int delta){uint32_t n=p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);n=(n+(uint32_t)delta)&0xffffff;p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);}
static inline void fa_animation(uint8_t *a,const uint8_t *b,unsigned ptr){fa_put(a+6,ptr);a[8]=b[ptr-0x8000]&127;}
int fauna_animate(uint8_t actor[40],const uint8_t bank4[16384]);
int fauna_queue(fauna_context *context,unsigned command,unsigned parameter);
int fauna_score(fauna_context *context,const uint8_t actor[40],unsigned type);
int fauna_puffer_step(fauna_context *context,unsigned slot,uint8_t draw[40]);
int fauna_bird_step(fauna_context *context,unsigned slot,uint8_t draw[40]);
int fauna_bonus_step(fauna_context *context,unsigned slot,uint8_t draw[40]);
#endif
