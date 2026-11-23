#include "spang_scene_data.h"
#include "stage_data.h"

int spang_scene_unpack(const uint8_t *src,unsigned length,uint8_t *out,unsigned capacity)
{
    if(!src || !out || length<4)return -1;
    unsigned size=((unsigned)src[0]<<24)|((unsigned)src[1]<<16)|((unsigned)src[2]<<8)|src[3];
    if(size>capacity)return -1;
    unsigned in=4,pos=0,flags=0,bits=0;
    while(pos<size){
        if(!bits){if(in>=length)return -1;flags=src[in++];bits=8;}
        if(flags&128){if(in>=length)return -1;out[pos++]=src[in++];}
        else{
            if(in+2>length)return -1;
            unsigned value=((unsigned)src[in]<<8)|src[in+1];in+=2;
            unsigned distance=(value>>4)+1,count=(value&15)+3;
            if(distance>pos || count>size-pos)return -1;
            for(unsigned n=0;n<count;++n){out[pos]=out[pos-distance];++pos;}
        }
        flags<<=1;--bits;
    }
    return (int)size;
}
int spang_scene_load(unsigned scene,uint8_t out[SPANG_SCENE_BYTES])
{
    if(!out || scene>=STAGE_SCENE_COUNT)return -1;
    const stage_scene_blob *s=stage_scenes+scene;
    if(s->base>scene)return -1;
    const stage_scene_blob *base=stage_scenes+s->base;
    if(base->base!=s->base || spang_scene_unpack(base->bytes,base->length,out,SPANG_SCENE_BYTES)!=SPANG_SCENE_BYTES)return -1;
    if(s->base==scene)return 0;
    unsigned in=0;
    while(in<s->length){
        if(in+3>s->length)return -1;
        unsigned at=((unsigned)s->bytes[in]<<8)|s->bytes[in+1],count=s->bytes[in+2]+1;in+=3;
        if(at+count>SPANG_SCENE_BYTES || in+count>s->length)return -1;
        for(unsigned n=0;n<count;++n)out[at+n]=s->bytes[in+n];
        in+=count;
    }
    return 0;
}
