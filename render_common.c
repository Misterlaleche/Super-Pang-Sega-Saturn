#include "include/render_common.h"
#include "stage/stage.h"
#include "art/r10_r4/glyph_inflate.h"
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static unsigned be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int rc_glyph_block_decode(const uint8_t *p,unsigned bytes,unsigned block,uint8_t *out,unsigned cap)
{
    unsigned total,format,blocks,start,end,n,at=0,flags=0,bits=0;
    if(!p || !out || bytes<12)return 0;
    format=be32(p+4);
    if(format!=4096 && format!=0x44460001u)return 0;
    total=be32(p);if(!total || total>32768u*32)return 0;
    if(format==0x44460001u && (total&31))return 0;
    blocks=(total+4095)/4096;
    if(bytes<8+4*(blocks+1) || block>=blocks)return 0;
    start=be32(p+8+block*4);end=be32(p+12+block*4);
    n=total-block*4096;if(n>4096)n=4096;
    if(start<8+4*(blocks+1) || start>=end || end>bytes || cap<n || end-start<4)return 0;
    if(format==0x44460001u)
    {
        unsigned decoded=n,sum1=1,sum2=0;
        if(rc_glyph_inflate(out,&decoded,p+start+4,end-start-4)!=TINF_OK || decoded!=n)return 0;
        /* At most4096 bytes: both unreduced sums fit in uint32_t. */
        for(unsigned i=0;i<n;++i){sum1+=out[i];sum2+=sum1;}
        if((((sum2%65521u)<<16)|(sum1%65521u))!=be32(p+start))return 0;
        return (int)n;
    }
    if(be32(p+start)!=n)return 0;
    start+=4;
    while(at<n)
    {
        if(!bits){if(start>=end)return 0;flags=p[start++];bits=8;}
        if(flags&128){if(start>=end)return 0;out[at++]=p[start++];}
        else
        {
            unsigned word,distance,length;
            if(end-start<2)return 0;
            word=be16(p+start);start+=2;distance=(word>>4)+1;length=(word&15)+3;
            if(distance>at || length>n-at)return 0;
            while(length--){out[at]=out[at-distance];++at;}
        }
        flags<<=1;--bits;
    }
    return start==end?(int)n:0;
}
int rc_glyph_blocks_valid(const uint8_t *p,unsigned bytes,unsigned expected,uint8_t *scratch,unsigned capacity)
{
    if(!p || bytes<12 || be32(p)!=expected || !expected || expected>32768u*32)return 0;
    unsigned blocks=(expected+4095)/4096;
    if(bytes<8+4*(blocks+1) || be32(p+8)!=8+4*(blocks+1) || be32(p+8+blocks*4)!=bytes)return 0;
    for(unsigned i=0;i<blocks;++i)if(!rc_glyph_block_decode(p,bytes,i,scratch,capacity))return 0;
    return 1;
}
int rc_cell_catalog_valid_split(const uint8_t *catalog,unsigned bytes,unsigned tail_bytes)
{
    if(!catalog || bytes<4096+128 || (bytes-4096)%128)return 0;
    if(tail_bytes%128)return 0;
    unsigned unique=(bytes-4096+tail_bytes)/128;
    if(unique>2048)return 0;
    for(unsigned code=0;code<2048;++code)if(be16(catalog+code*2)>=unique)return 0;
    return 1;
}
int rc_cell_catalog_valid(const uint8_t *catalog,unsigned bytes)
{return rc_cell_catalog_valid_split(catalog,bytes,0);}
const uint8_t *rc_cell_pixels_split(const uint8_t *patterns,const uint8_t *map,const uint8_t *tail,unsigned split,unsigned code)
{
    unsigned ordinal=map?be16(map+code*2):code;
    return tail && ordinal>=split?tail+(ordinal-split)*128:patterns+ordinal*128;
}
const uint8_t *rc_cell_pixels(const uint8_t *patterns,const uint8_t *map,unsigned code)
{
    return patterns+(map?be16(map+code*2):code)*128;
}
int rc_glyph_catalog_valid(const uint8_t *map,unsigned map_bytes,unsigned glyph_bytes)
{
    unsigned count,unique,previous=0;
    if(!map || map_bytes<4)return 0;
    if(be32(map)==0xffff0001u)
    {
        unsigned present=0;
        if(map_bytes<8)return 0;
        count=be16(map+4);unique=be16(map+6);
        if(!count || count>32768 || !unique || unique>count ||
           map_bytes!=8+count*2 || glyph_bytes!=unique*32)return 0;
        for(unsigned i=0;i<count;++i)
        {
            unsigned ordinal=be16(map+8+i*2);
            if(ordinal==65535)continue;
            if(ordinal>=unique)return 0;
            ++present;
        }
        return present>=unique;
    }
    count=be16(map);unique=be16(map+2);
    if(!count || count>32768 || !unique || unique>count ||
       map_bytes!=4+count*4 || glyph_bytes!=unique*32)return 0;
    for(unsigned i=0;i<count;++i)
    {
        unsigned code=be16(map+4+i*4),ordinal=be16(map+6+i*4);
        if(code>32767 || (i && code<=previous) || ordinal>=unique)return 0;
        previous=code;
    }
    return 1;
}
int rc_glyph_offset(const uint8_t *map,unsigned code)
{
    if(!map || code>32767)return -1;
    if(be32(map)==0xffff0001u)
    {
        unsigned ordinal;
        if(code>=be16(map+4))return -1;
        ordinal=be16(map+8+code*2);
        return ordinal==65535?-1:(int)(ordinal*32);
    }
    unsigned lo=0,hi=be16(map),count=hi;
    while(lo<hi)
    {
        unsigned mid=(lo+hi)/2,v=be16(map+4+mid*4);
        if(v<code)lo=mid+1;else hi=mid;
    }
    if(lo==count || be16(map+4+lo*4)!=code)return -1;
    return (int)(be16(map+6+lo*4)*32);
}
int rc_screen_x(int x)
{
    int v=(x-64)*5;
    return v>=0?v/6:-((-v+5)/6);
}
int rc_world_boundary_x(int x)
{
    int v=(x-64)*5+2;
    return v>=0?v/6:-((-v+5)/6);
}
int rc_map_screen_x(int x)
{
    return x-96;
}
unsigned rc_hud_source_x(unsigned x)
{
    /* Score/lives/weapon and TIME/WORLD/HI stay on their original 1:1 grid.
     * Source320..335 is blank on all HUD rows; TOUR/MODE starts at336. */
    return x+(x<256?64:80);
}
unsigned rc_background_glyph_x(unsigned code,unsigned x)
{
    /* The 5/6 world map omits source X132, the single U/S separator in
     * the original BONUS label. Keep that original column in its two tiles
     * instead of sampling the adjacent double-width S stroke twice. */
    if((code==0x0acb || code==0x0adb) && x==5)return 4;
    return x;
}
uint16_t rc_rgb555(uint16_t c)
{
    unsigned r=(c>>8)&15,g=(c>>4)&15,b=c&15;
    r=(r<<1)|(r>>3);g=(g<<1)|(g>>3);b=(b<<1)|(b>>3);
    return (uint16_t)(0x8000|r|(g<<5)|(b<<10));
}
static int is_lucky_bonus(const rc_group *g)
{
    /* Original clear actor 8ACD starts with the L cell (7AB). */
    return g->role==STAGE_ROLE_CLEAR && g->source_code==0x7ab &&
           g->width==352 && g->height==16 && g->count==20;
}
int rc_collect(rc_scene *s,const uint8_t *raw,unsigned count)
{
    unsigned i,j;
    uint8_t panic_width[RC_MAX_OBJECTS];
    uint16_t panic_seen[RC_MAX_OBJECTS];
    s->object_count=s->group_count=0;
    if(count>RC_MAX_OBJECTS)return 0;
    for(i=0;i<count;++i)
    {
        const uint8_t *p=raw+32*i;
        rc_object *o=&s->objects[s->object_count];
        unsigned id=le16(p+7),w=le16(p+13),h=le16(p+15);
        int left=(int16_t)le16(p+9),top=(int16_t)le16(p+11);
        unsigned role=p[17],columns=0,ordinal=0;
        if(!(p[0]|p[1]|p[2]|p[3]) && !(p[18]&128))continue;
        o->code=p[0]+((p[1]&0xe0)<<3);o->palette=p[1]&15;
        o->x=p[3]+((p[1]&16)<<4);o->y=((p[2]+8)&255)-8;
        o->ladder_row=0;
        if(p[18]&128)
        {
            /* Panic retains the original hardware bytes, including F8 for
             * clipped incoming rows. Byte18 is a square width descriptor,
             * byte19 its column-serpentine cell ordinal, and20..23 the signed
             * source origin. Recover the logical cell position before either
             * composition or cache hashing; F8 alone cannot identify a row.
             * Special icons are separate one-cell groups using this contract. */
            int x,y;unsigned column,row;
            columns=p[18]&127;ordinal=p[19];
            if(columns<1 || columns>4 || ordinal>=columns*columns ||
               p[4]!=0xa6 || !id || (role!=1 && role!=4) ||
               w!=columns*16 || h!=w ||
               left!=(int16_t)le16(p+20) || top!=(int16_t)le16(p+22))return 0;
            column=ordinal/columns;row=ordinal%columns;
            if(column&1)row=columns-1-row;
            x=left+(int)column*16;y=top+(int)row*16;
            if(x<-32768 || x>32767 || y<-32768 || y>32767 ||
               ((unsigned)x&511)!=(unsigned)o->x ||
               (p[2]!=(uint8_t)y && !(p[2]==248 && (((unsigned)y+8)&256))))return 0;
            o->x=(int16_t)x;o->y=(int16_t)y;
        }
        else if(p[18])
        {
            if(role!=18 || p[4]!=0xa7 || !id || p[18]>10)return 0;
            o->ladder_row=(uint8_t)(p[18]|(p[2]==248?128:0));
            o->y=top+(p[18]-1)*16;
            if(p[2]!=248 && p[2]!=(uint8_t)o->y)return 0;
        }
        if(!id || !(p[4]==0xa6 || p[4]==0xa7 || p[4]==0xaa))
        {id=0x8000+i;w=h=16;left=o->x;top=o->y;role=0;}
        if(!w || !h || w>RC_MAX_SOURCE_WIDTH || h>RC_MAX_HEIGHT || w*h>RC_MAX_PIXELS)return 0;
        /* Original screen coordinates wrap; the group origin is signed. */
        /* A wide message can legitimately span more than half the coordinate
         * modulus. Only unwrap an out-of-bounds coordinate if it lands inside
         * this group's declared footprint. */
        if(o->x<left && o->x+512>=left && o->x+512<left+(int)w)o->x+=512;
        else if(o->x>=left+(int)w && o->x-512>=left && o->x-512<left+(int)w)o->x-=512;
        if(o->y<top && o->y+256>=top && o->y+256<top+(int)h)o->y+=256;
        else if(o->y>=top+(int)h && o->y-256>=top && o->y-256<top+(int)h)o->y-=256;
        for(j=0;j<s->group_count && s->groups[j].id!=id;++j){}
        if(j==s->group_count)
        {
            rc_group *g=&s->groups[s->group_count++];
            g->id=id;g->left=left;g->top=top;g->width=w;g->height=h;
            g->first=i;g->count=0;g->palette=o->palette;g->role=role;
            g->source_code=o->code;
            panic_width[j]=(uint8_t)columns;panic_seen[j]=0;
        }
        {
            rc_group *g=&s->groups[j];
            if(g->left!=left || g->top!=top || g->width!=w || g->height!=h ||
               g->palette!=o->palette || g->role!=role || panic_width[j]!=columns)return 0;
            if(columns)
            {
                unsigned bit=1u<<ordinal;
                if(panic_seen[j]&bit)return 0;
                panic_seen[j]|=(uint16_t)bit;
            }
            if(o->x==left && o->y==top)g->source_code=o->code;
            ++g->count;o->group=j;
        }
        ++s->object_count;
    }
    for(i=0;i<s->group_count;++i)
    {
        if(s->groups[i].width>RC_MAX_WIDTH && !is_lucky_bonus(&s->groups[i]))return 0;
        if(panic_width[i])
        {
            unsigned cells=panic_width[i]*panic_width[i];
            if(s->groups[i].count!=cells || panic_seen[i]!=(1u<<cells)-1)return 0;
        }
        if(s->groups[i].role==18)
        {uint32_t mask;if(!rc_ladder_mask(s,i,&mask))return 0;}
    }
    return 1;
}
unsigned rc_ladder_rungs(const rc_scene *s,unsigned group)
{
    const rc_group *g;unsigned count,seen=0,odd_tail=0;
    if(group>=s->group_count)return 0;
    g=&s->groups[group];
    if(g->role!=18 || g->width!=32 || !g->height ||
       g->height>RC_MAX_HEIGHT || (g->height&15))return 0;
    count=g->height/8;
    if(g->count!=count)return 0;
    for(unsigned i=0;i<s->object_count;++i)if(s->objects[i].group==group)
    {
        const rc_object *o=&s->objects[i];
        int x=o->x-g->left,y=o->y-g->top;
        unsigned col,slot,code;
        if((x!=0 && x!=16) || y<0 || (y&15) || y+16>g->height)return 0;
        col=(unsigned)x/16;slot=(unsigned)y/16*2+col;code=0x206+col;
        if(seen&(1u<<slot))return 0;
        seen|=1u<<slot;
        if(y+16==g->height && o->code==code+8)odd_tail|=1u<<col;
        else if(o->code!=code)return 0;
    }
    if(seen!=(1u<<count)-1 || (odd_tail && odd_tail!=3))return 0;
    return count-(odd_tail?1:0);
}
int rc_ladder_mask(const rc_scene *s,unsigned group,uint32_t *mask)
{
    unsigned marked=0;uint32_t hidden=0;
    if(!mask || !rc_ladder_rungs(s,group))return 0;
    const rc_group *g=&s->groups[group];
    for(unsigned i=0;i<s->object_count;++i)if(s->objects[i].group==group)
    {
        const rc_object *o=&s->objects[i];
        unsigned row=(unsigned)(o->y-g->top)/16,col=(unsigned)(o->x-g->left)/16;
        if(o->ladder_row)
        {
            if((o->ladder_row&127)!=row+1)return 0;
            ++marked;
            if(o->ladder_row&128)hidden|=1u<<(row*2+col);
        }
    }
    if(marked && marked!=g->count)return 0;
    *mask=hidden;return 1;
}
static void hash_word(uint32_t key[2],unsigned v)
{
    unsigned i;
    for(i=0;i<4;++i)
    {
        unsigned b=v&255;
        key[0]=(key[0]^b)*16777619u;
        key[1]=(key[1]+b+0x9e3779b9u)^(key[1]<<6)^(key[1]>>2);
        v>>=8;
    }
}
void rc_group_key(const rc_scene *s,unsigned group,uint32_t key[2])
{
    unsigned i;const rc_group *g=&s->groups[group];
    key[0]=2166136261u;key[1]=0x85ebca6bu;
    hash_word(key,g->width);hash_word(key,g->height);hash_word(key,g->count);
    for(i=0;i<s->object_count;++i)if(s->objects[i].group==group)
    {
        const rc_object *o=&s->objects[i];
        hash_word(key,o->code);hash_word(key,(unsigned)(o->x-g->left));
        hash_word(key,(unsigned)(o->y-g->top));
    }
}
static void compose_source(const rc_scene *s,unsigned group,const uint8_t *cells,const uint8_t *map,
                           const uint8_t *tail,unsigned split,uint8_t *scratch)
{
    const rc_group *g=&s->groups[group];unsigned i,x,y;
    for(i=0;i<(unsigned)g->width*g->height;++i)scratch[i]=0;
    for(i=s->object_count;i>0;--i)
    {
        const rc_object *o=&s->objects[i-1];
        if(o->group!=group)continue;
        const uint8_t *cell=rc_cell_pixels_split(cells,map,tail,split,o->code);
        int base_x=o->x-g->left,base_y=o->y-g->top;
        for(y=0;y<16;++y)for(x=0;x<16;++x)
        {
            int dx=base_x+(int)x,dy=base_y+(int)y;
            unsigned packed=cell[y*8+x/2];
            unsigned pen=(x&1)?packed&15:packed>>4;
            if(pen && dx>=0 && dy>=0 && dx<g->width && dy<g->height)
                scratch[dy*g->width+dx]=pen;
        }
    }
}
int rc_compose_split(const rc_scene *s,unsigned group,const uint8_t *cells,const uint8_t *map,const uint8_t *tail,unsigned split,uint8_t *scratch,
               uint8_t *out,unsigned cap,unsigned *width,unsigned *height,unsigned *anchor,
               int native)
{
    const rc_group *g;unsigned i,x,y,nw,pw,bytes,ax;
    uint16_t source_columns[RC_MAX_WIDTH];
    if(group>=s->group_count)return 0;
    g=&s->groups[group];
    if(!g->width || !g->height || g->height>RC_MAX_HEIGHT ||
       (unsigned)g->width*g->height>RC_MAX_PIXELS)return 0;
    if(g->width>RC_MAX_WIDTH)
    {
        if(native || !is_lucky_bonus(g))return 0;
        /* LUCKY and BONUS each have an empty 16px cell after the word.
         * Remove 12px from each space and 8px after the final period.
         * Every original opaque pen stays on the native 1:1 grid. */
        nw=RC_MAX_WIDTH;
        for(x=0;x<nw;++x)source_columns[x]=(uint16_t)(x+(x>=80?12:0)+(x>=164?12:0));
        ax=nw/2;
    }
    else
    {
        nw=native?g->width:(g->width*200u+109u)/219u;
        for(x=0;x<nw;++x)source_columns[x]=(uint16_t)(native?x:((2*x+1)*g->width)/(2*nw));
        ax=native?g->width/2:(g->width*100u+109u)/219u;
    }
    pw=(nw+7u)&~7u;bytes=pw*g->height/2;
    if(bytes>cap)return 0;
    compose_source(s,group,cells,map,tail,split,scratch);
    for(i=0;i<bytes;++i)out[i]=0;
    for(y=0;y<g->height;++y)for(x=0;x<nw;++x)
    {
        unsigned sx=source_columns[x],pen=scratch[y*g->width+sx];
        unsigned at=(y*pw+x)/2;
        if(x&1)out[at]|=pen;else out[at]=pen<<4;
    }
    *width=pw;*height=g->height;*anchor=ax;
    return 1;
}
int rc_compose_platform_split(const rc_scene *s,unsigned group,const uint8_t *cells,const uint8_t *map,
               const uint8_t *tail,unsigned split,uint8_t *scratch,uint8_t *out,unsigned cap,
               unsigned *width,unsigned *height)
{
    const rc_group *g;unsigned pw=0,bytes;
    uint16_t source_columns[RC_MAX_WIDTH];
    if(!s || !cells || !scratch || !out || !width || !height || group>=s->group_count)return 0;
    g=&s->groups[group];
    if(!g->width || !g->height || (g->width&15) || (g->height&15) ||
       g->width>RC_MAX_WIDTH || g->height>RC_MAX_HEIGHT ||
       (unsigned)g->width*g->height>RC_MAX_PIXELS)return 0;
    for(unsigned phase=0;phase<6;phase+=2)
    {
        unsigned nw=(unsigned)(rc_world_boundary_x(64+phase+g->width)-rc_world_boundary_x(64+phase));
        unsigned padded=(nw+7u)&~7u;
        if(pw && padded!=pw)return 0;
        pw=padded;
    }
    bytes=pw*g->height/2;
    /* Validate all three images before touching either caller buffer. */
    if(bytes>cap/3)return 0;
    compose_source(s,group,cells,map,tail,split,scratch);
    for(unsigned i=0;i<bytes*3;++i)out[i]=0;
    for(unsigned phase=0;phase<6;phase+=2)
    {
        unsigned origin=(unsigned)rc_world_boundary_x(64+phase);
        unsigned nw=(unsigned)rc_world_boundary_x(64+phase+g->width)-origin;
        uint8_t *image=out+(phase/2)*bytes;
        for(unsigned x=0;x<nw;++x)source_columns[x]=(uint16_t)((6*(origin+x)+3)/5-phase);
        for(unsigned y=0;y<g->height;++y)for(unsigned x=0;x<nw;++x)
        {
            unsigned pen=scratch[y*g->width+source_columns[x]],at=(y*pw+x)/2;
            if(x&1)image[at]|=(uint8_t)pen;else image[at]=(uint8_t)(pen<<4);
        }
    }
    *width=pw;*height=g->height;return 1;
}
int rc_compose_indexed(const rc_scene *s,unsigned group,const uint8_t *cells,const uint8_t *map,uint8_t *scratch,
               uint8_t *out,unsigned cap,unsigned *width,unsigned *height,unsigned *anchor,int native)
{return rc_compose_split(s,group,cells,map,0,0,scratch,out,cap,width,height,anchor,native);}
int rc_compose(const rc_scene *s,unsigned group,const uint8_t *cells,uint8_t *scratch,
               uint8_t *out,unsigned cap,unsigned *width,unsigned *height,unsigned *anchor)
{
    return rc_compose_indexed(s,group,cells,0,scratch,out,cap,width,height,anchor,0);
}
int rc_compose_native(const rc_scene *s,unsigned group,const uint8_t *cells,uint8_t *scratch,
               uint8_t *out,unsigned cap,unsigned *width,unsigned *height,unsigned *anchor)
{
    return rc_compose_indexed(s,group,cells,0,scratch,out,cap,width,height,anchor,1);
}
