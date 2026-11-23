#include <jo/jo.h>
#include "include/boot_diag.h"
#include <internal/sprites.h>
#include <internal/vdp1_command_pipeline.h>
#include "include/runtime.h"
#include "include/render_common.h"
#include "art/authored_index.h"
#include "art/r10_r10/gameplay/gameplay_native.h"
#include "art/r10_r10/presentation/start_prompt.h"
#include "art/r10_r14/presentation/pause_prompt.h"
#include "frontend/frontend.h"
#include "frontend/layout.h"
#include "frontend/title_native.h"

#define RT_SCENE_DOUBLE_BUFFER 1

/* Graphics stay resident. CLUTs follow the original palette; compound cells
 * use a fixed native grid cached independently of position and retries. */
#define CACHE_COUNT 256
typedef struct {uint32_t key[2];int16_t sprite,anchor;uint16_t width,height;uint8_t native;} cached_group;
static cached_group cache[CACHE_COUNT];
static unsigned cache_count,fault_flags,fault_detail;
static unsigned fault_first_flags,video_ready;
static int fault_code;
static int16_t cell_ids[2048],auth_ids[ART_AUTHORED_COUNT];
static int16_t item_ids[25];
static int16_t demo_prompt_id,pause_prompt_id;
static uint16_t demo_prompt_clut,pause_prompt_clut;
/* Four original ladder cells, six world sampling phases. Each immutable
 * padded16x16 texture is128bytes; all40 Tour stages need16 variants (2KiB).
 * Hidden cells skip commands and cannot create additional texture variants. */
static int16_t ladder_ids[4][6];
/* The13 original moving title actors use296 nonempty code/phase pairs.
 * Translation cannot add variants; the fixed table also bounds malformed
 * future data. Footer/menu use variant36 in the same table. Each packed
 * word has a17-bit key and12-bit(sprite+3), saving2KiB over the old dense
 * frontend index. Textures are immutable once submitted to VDP1. */
#define FRONTEND_TITLE_CACHE_COUNT 512u
static uint32_t frontend_title_cells[FRONTEND_TITLE_CACHE_COUNT];
static int16_t pang_actor_texture[8];
static int16_t pang_actor_left[8],pang_actor_top[8];
static uint16_t authored_order[ART_AUTHORED_COUNT];
static uint16_t cluts[16],spare_cluts[16],viewx[960],viewy[240],colors[2048];
static uint8_t *cells,*cell_tail,*glyphs,*glyph_index;
static const uint8_t *cell_patterns;
static uint8_t previous_attrs[2048],previous_chars[4096],previous_palette[4096];
static uint8_t dirty[2048],bank_changed[128];
static uint8_t source_scratch[RC_MAX_PIXELS];
static unsigned glyph_blocks[8];
/* Both bitmap paths share the same eight scratch slots and eviction order. */
static uint8_t glyph_order[8]={0,1,2,3,4,5,6,7};
static uint8_t texture_scratch[RC_MAX_PIXELS/2];
static rc_scene scene;
static int first_background=1,ready;
static unsigned previous_scene=~0u;
static volatile uint16_t *background_target=(volatile uint16_t *)JO_VDP2_VRAM_A0;
static uint32_t rendered_scene_revision;
static unsigned previous_frontend_credits;
static int screen_offset_x,screen_offset_y;
static int16_t center_glyph_ids[128];
static uint16_t center_lut;
static int center_prepare_glyphs(void);
void rt_set_screen_offset(int x,int y)
{
 if(x< -16||x>16||y< -12||y>12)return;
 screen_offset_x=x;screen_offset_y=y;
 jo_vdp2_move_nbg1(-x,-y);
}
static unsigned previous_frontend_title_mask;
static int scene_invalidated;
typedef struct {uint16_t tile,begin,end;} background_run;
static background_run background_runs[3][64];
static unsigned background_run_count[3],previous_palette_revision;
static uint32_t previous_tile_revision;
static unsigned background_name_layout,background_bonus_mask,background_bonus_awarded;
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static unsigned be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static void fail(unsigned flag,unsigned detail){rt_report_fault_code(flag,(int)detail);}
static void invalidate_glyph_blocks(void)
{for(unsigned i=0;i<8;++i)glyph_blocks[i]=~0u;}
static void prepare_authored_order(void)
{
    /* Stable order retains the first original record among equal codes. */
    for(unsigned i=0;i<ART_AUTHORED_COUNT;++i)
    {
        unsigned at=i;
        while(at && art_authored_records[authored_order[at-1]].source_code>
                     art_authored_records[i].source_code)
        {authored_order[at]=authored_order[at-1];--at;}
        authored_order[at]=(uint16_t)i;
    }
}
static unsigned authored_match(const rc_group *g,unsigned rungs)
{
    unsigned lo=0,hi=ART_AUTHORED_COUNT;
    while(lo<hi)
    {
        unsigned mid=lo+(hi-lo)/2;
        if(art_authored_records[authored_order[mid]].source_code<g->source_code)lo=mid+1;
        else hi=mid;
    }
    for(;lo<ART_AUTHORED_COUNT;++lo)
    {
        unsigned i=authored_order[lo];const ArtAuthoredRecord *a=&art_authored_records[i];
        if(a->source_code!=g->source_code)break;
        if(a->source_width==g->width && a->source_height==g->height && a->role==g->role &&
           (g->role!=STAGE_ROLE_LADDER || (rungs && authored_ladder_rungs[i]==rungs)))return i;
    }
    return ART_AUTHORED_COUNT;
}
static void prepare_background_mode(unsigned mode)
{
    const uint16_t *xm=viewx+mode*320;
    unsigned x,count=0;
    for(x=0;x<320;)
    {
        unsigned end=x+1,tile=xm[x]>>3;
        while(end<320 && (xm[end]>>3)==tile)++end;
        if(count>=64){fail(RT_FAULT_ASSET,5);return;}
        background_runs[mode][count].tile=(uint16_t)tile;
        background_runs[mode][count].begin=(uint16_t)x;
        background_runs[mode][count].end=(uint16_t)end;
        ++count;x=end;
    }
    background_run_count[mode]=count;
}
static void prepare_background_runs(void)
{
    unsigned mode,x;
    /* The entire original map fits this centered 320-pixel window. Its
     * letters, compass and route keep their original pixel geometry. */
    for(x=0;x<320;++x)
    {
        viewx[320+x]=(uint16_t)rc_hud_source_x(x);
        viewx[640+x]=(uint16_t)(96+x);
    }
    for(mode=0;mode<3;++mode)prepare_background_mode(mode);
    background_name_layout=0;background_bonus_mask=background_bonus_awarded=0;
}
static uint8_t *load_art(const char *name,int expected)
{
    int length=0;uint8_t *p=(uint8_t *)jo_fs_read_file_in_dir(name,"ART",&length);
    if(!p || (expected>=0 && length!=expected))
    {if(p)jo_free(p);fail(RT_FAULT_ASSET,(unsigned)expected);return JO_NULL;}
    return p;
}
static int any_pixels(const uint8_t *p,unsigned n)
{unsigned i;for(i=0;i<n;++i)if(p[i])return 1;return 0;}
static int glyph_offset(unsigned code)
{
    int offset=rc_glyph_offset(glyph_index,code);
    if(offset<0)fail(RT_FAULT_GLYPH,code);
    return offset;
}
static unsigned background_origin(const stage_view *v)
{
    int name=v->phase==STAGE_FRONTEND && v->scene_index==73;
    unsigned scene_index=name?v->globals[0x7c]:v->scene_index;
    unsigned phase=name?v->globals[0x7d]:v->phase;
    /* Keep the original long Maya labels inside the native-width window.
     * The map, route and labels retain their original unscaled pixels. */
    if(phase==STAGE_MAP){
        if(scene_index==44)return 112;
        if(scene_index==47)return 80;
    }
    if(scene_index!=54)return 96;
    unsigned page=stage_ending_page();
    return page==0?104:page==1?72:page==3?96:116;
}
/* Retain original8x8 cells within the two17-cell player panels. Tour's
 * TIME word moves below its digits; the unused score-underlines hold WORLD
 * and HI. Panic's level/gauge uses its empty weapon-box space. No resampling
 * or additional tile/bitmap buffer is needed.2048 denotes a blank cell. */
static unsigned hud_source_tile(unsigned col,unsigned row,unsigned layout)
{
    if(layout==3 && col>=13 && col<27)return row*64+col+12;
    if(layout==2)
    {
        if(row==28)
        {
            if(col>=3 && col<8)return 29*64+col+23;
            if(col>=8 && col<11)return 29*64+col+26;
            if(col>=29 && col<31)return 30*64+col-3;
            if(col>=31 && col<39)return 30*64+col-1;
            if(col==28 || col==39)return 2048;
        }
        if(row>=29 && col>=16 && col<24)return (row-2)*64+col+9;
    }
    if(col>=17 && col<23)
    {
        if(layout==4)return row<29 && col>=18 && col<22?row*64+col+14:2048;
        if(row<29)return row*64+col+16;
        if(layout==1)return 29*64+col+(row==29?9:16);
    }
    return row*64+col+(col<17?8:16);
}
/* Bonus interiors are152 original pixels. Two160px panels preserve every
 * numeral/label pixel by narrowing only their8px borders to4px. Presence is
 * semantic: the same numeral glyphs elsewhere must keep their own layout. */
static unsigned bonus_panel_mask(const stage_view *v)
{
    unsigned mask=0;
    /* The original final bonus tick enters CLEAR at12/4/0 while retaining
     * the same boxes until map, next round or ending takes over. */
    if(v->phase!=STAGE_BONUS && !(v->phase==STAGE_CLEAR && v->globals &&
       v->globals[0]==0x12 && v->globals[1]==4 && v->globals[2]==0))return 0;
    for(unsigned player=0;player<2;++player)
    {
        unsigned left=970+23*player,right=left+20,bottom=left+320;
        if((le16(v->chars+2*left)&32767)==0x0a8e &&
           (le16(v->chars+2*right)&32767)==0x0a8e &&
           (le16(v->chars+2*bottom)&32767)==0x0a9e &&
           (le16(v->chars+2*(bottom+20))&32767)==0x0a9e &&
           v->attrs[left]==0x65 && v->attrs[right]==0xe5 &&
           v->attrs[bottom]==0x65 && v->attrs[bottom+20]==0xe5)mask|=1u<<player;
    }
    return mask;
}
static unsigned bonus_awarded(const stage_view *v)
{return v->globals && (v->globals[1]!=2 || v->globals[2]>=4);}
static void prepare_bonus_mapping(unsigned mask)
{
    static const uint8_t edge[4]={0,2,5,7};
    for(unsigned x=0;x<320;++x)
    {
        unsigned local=x%160,source=80+184*(x/160);
        viewx[640+x]=(mask&(1u<<(x/160)))?(uint16_t)(source+
            (local<4?edge[local]:local<156?local+4:160u+edge[local-156])):viewx[x];
    }
    prepare_background_mode(2);
}
static void update_background(const stage_view *v)
{
    unsigned i,j,y,pixels_changed=0;int backdrop_changed;
    int name=v->phase==STAGE_FRONTEND && v->scene_index==73;
    unsigned scene_index=name?v->globals[0x7c]:v->scene_index;
    unsigned phase=name?v->globals[0x7d]:v->phase;
    int map=phase==STAGE_MAP || scene_index==54;
    int tutorial=scene_index==55 || scene_index==56;
    unsigned name_hud=name || v->records_mask,panic=v->globals && v->globals[0x1a]==2;
    unsigned hud_layout=name_hud?(panic?4:1):!map && (v->player_mask&2)?(panic?3:2):0;
    int hud_changed=background_name_layout!=hud_layout;
    background_name_layout=hud_layout;
    unsigned bonus_mask=bonus_panel_mask(v);
    background_bonus_awarded=bonus_mask && bonus_awarded(v);
    int bonus_layout_changed=bonus_mask!=background_bonus_mask;
    if(bonus_layout_changed)
    {
        /* Mapping changes affect only these six panel rows. Repaint their
         * entire footprint, including pixels beyond the old sampled box,
         * without decoding or rewriting the rest of the playfield. */
        background_bonus_mask=bonus_mask;
        if(bonus_mask)prepare_bonus_mapping(bonus_mask);
    }
    pixels_changed=(unsigned)bonus_layout_changed;
    if(map && viewx[640]!=background_origin(v))
    {
        unsigned origin=background_origin(v);
        for(i=0;i<320;++i)viewx[640+i]=(uint16_t)(origin+i);
        prepare_background_mode(2);first_background=1;
    }
    /* A map uses its own 1:1 window on every row, including the label area.
     * A scene switch invalidates the previous coordinate mapping. */
    if(previous_scene!=v->scene_index){first_background=1;previous_scene=v->scene_index;}
    if(tutorial && first_background)
    {
        /* The original22-cell instruction box fits at its full176-pixel
         * width. Anchor tutorial1 to its old right edge and tutorial2 to
         * its old left edge, covering every former sampled box pixel.
         * Reuse the map's coordinate/run storage only on the six box rows;
         * the full footprint also restores cleared and shrinking panels. */
        unsigned left=scene_index==55?111:13,source=scene_index==55?232:80;
        for(i=0;i<320;++i)viewx[640+i]=(uint16_t)(i>=left && i<left+176?source+i-left:viewx[i]);
        prepare_background_mode(2);
    }
    /* The stage publishes every tile/palette mutation through revisions.
     * Static gameplay needs no 2048-cell comparison. Mapping and owner
     * changes above still force their complete repaint before this gate. */
    if(!first_background && !hud_changed && !bonus_layout_changed &&
       previous_tile_revision==v->tile_revision &&
       previous_palette_revision==v->palette_revision)return;
    backdrop_changed=first_background;
    for(i=0;i<128;++i)bank_changed[i]=0;
    if(first_background || previous_palette_revision!=v->palette_revision)
    {
        unsigned map_blue=phase==STAGE_MAP?le16(v->palette+2*0x23d):0;
        for(i=0;i<2048;++i)
        {
            unsigned at=i*2;
            if(first_background || previous_palette[at]!=v->palette[at] || previous_palette[at+1]!=v->palette[at+1])
            {
                unsigned rgb=le16(v->palette+at);
                /* Only map font/arrow banks use117 as an opaque blue
                 * matte. Blend it with the map blue, preserving geography,
                 * original glyph pens and every other foreground color. */
                if(phase==STAGE_MAP && i>=0x400 && rgb==0x117)rgb=map_blue;
                colors[i]=rc_rgb555(rgb);bank_changed[i/16]=1;
                previous_palette[at]=v->palette[at];previous_palette[at+1]=v->palette[at+1];
                if(!i)backdrop_changed=1;
            }
        }
        previous_palette_revision=v->palette_revision;
    }
    for(i=0;i<16;++i)if(bank_changed[i])
    {
        volatile uint16_t *lut=(volatile uint16_t *)(JO_VDP1_VRAM+cluts[i]*8);
        lut[0]=0;for(j=0;j<15;++j)lut[j+1]=colors[i*16+j];
    }
    for(i=0;i<2048;++i)
    {
        unsigned at=i*2;
        dirty[i]=(uint8_t)(first_background || backdrop_changed || (hud_changed && i>=27*64 && i<31*64) || bank_changed[v->attrs[i]&127] ||
            previous_attrs[i]!=v->attrs[i] || previous_chars[at]!=v->chars[at] || previous_chars[at+1]!=v->chars[at+1]);
        pixels_changed|=dirty[i];
        if(dirty[i])
        {previous_attrs[i]=v->attrs[i];previous_chars[at]=v->chars[at];previous_chars[at+1]=v->chars[at+1];}
    }
    if(!pixels_changed){previous_tile_revision=v->tile_revision;first_background=0;return;}
    for(y=0;y<240;y+=8)
    {
        unsigned reflow=y>=208?hud_layout:0;
        unsigned bonus_row=bonus_mask && y>=112 && y<160;
        unsigned mode=reflow?1:(bonus_row || map || (tutorial && y>=16 && y<64))?2:(y>=208?1:0);
        unsigned music_serviced=0;
        const uint16_t *xm=viewx+mode*320;
        unsigned sy=viewy[y],tile_row=(sy>>3)*64;
        for(i=0;i<(reflow?40:background_run_count[mode]);++i)
        {
            const background_run *run=&background_runs[mode][i];
            unsigned x=reflow?i*8:run->begin,end=reflow?x+8:run->end;
            unsigned tile=reflow?hud_source_tile(i,sy>>3,reflow):tile_row+run->tile;
            if(tile==2048)
            {
                if(first_background || backdrop_changed || hud_changed)
                    for(unsigned yy=y;yy<y+8;++yy)for(unsigned xx=x;xx<end;++xx)
                        background_target[yy*JO_VDP2_WIDTH+xx]=colors[0];
                continue;
            }
            if(dirty[tile] || (bonus_layout_changed && y>=112 && y<160))
            {
                if(!music_serviced){jo_audio_service_music();music_serviced=1;}
                unsigned attr=v->attrs[tile],code=le16(v->chars+tile*2)&32767;
                int offset=glyph_offset(code);
                if(offset>=0)
                {
                    unsigned yy,xx,block=(unsigned)offset/4096,slot,position,empty=8;
                    for(position=0;position<8;++position){
                        slot=glyph_order[position];
                        if(glyph_blocks[slot]==block)break;
                        if(glyph_blocks[slot]==~0u && empty==8)empty=position;
                    }
                    if(position==8)position=empty<8?empty:7;
                    slot=glyph_order[position];
                    if(glyph_blocks[slot]!=block)
                    {
                        /* One row can cross several cold glyph blocks. The
                         * row refill above alone cannot cover their decode
                         * cost while the VBlank consumer keeps playing. */
                        jo_audio_service_music();
                        /* A failed decode may already have overwritten the
                         * previous block. Publish a valid tag only on success. */
                        glyph_blocks[slot]=~0u;
                        if(!rc_glyph_block_decode(glyphs,ART_GLYPH_PACKED_BYTES,block,source_scratch+slot*4096,4096))
                        {fail(RT_FAULT_GLYPH,code);return;}
                        glyph_blocks[slot]=block;
                    }
                    for(unsigned recent=position;recent>0;--recent)glyph_order[recent]=glyph_order[recent-1];
                    glyph_order[0]=(uint8_t)slot;
                    const uint8_t *glyph=source_scratch+slot*4096+((unsigned)offset&4095);
                    /* Native columns/blocks keep their original collision
                     * footprint; local pixels preserve authored6px/7px art. */
                    /* Exactly AA8/AA9/AB8/AB9 belong to the ice palette19.
                     * Other native terrain remains palette13; frontend and
                     * maps retain their existing original ice glyphs. */
                    unsigned ice=(code&~0x11u)==0xaa8u;
                    const uint8_t *native=!map && (attr&127)==(ice?19u:13u) &&
                        (!ice || v->phase!=STAGE_FRONTEND)?
                        r10_terrain_pixels(code,end-x):JO_NULL;
                    if(native)glyph=native;
                    for(yy=y;yy<y+8 && yy<240;++yy)
                    {
                        volatile uint16_t *dest=background_target+yy*JO_VDP2_WIDTH;
                        for(xx=x;xx<end;++xx)
                        {
                            unsigned gx=native || reflow?xx-x:xm[xx]&7,gy=reflow?yy-y:viewy[yy]&7,p,pen;
                            if(attr&128)gx=(native?end-x:8)-1-gx;
                            if(!map && !(bonus_row && (bonus_mask&(1u<<(xx/160)))) &&
                               (code==0x0acb || code==0x0adb))gx=rc_background_glyph_x(code,gx);
                            p=glyph[gy*4+gx/2];pen=(gx&1)?p&15:p>>4;
                            dest[xx]=colors[pen==15?0:(attr&127)*16+pen];
                        }
                    }
                }
            }
        }
    }
    previous_tile_revision=v->tile_revision;first_background=0;
}
/* Frontend tiles use the original palette and glyph bytes on a dedicated
 * native layout. Title art and its animated sprite letters share endpoint
 * projection; all menu text and panels retain their original eight pixels. */
static void update_frontend_background(const stage_view *v)
{
    /* One5/6 source grid across tile boundaries, matching the sampled
     * actor textures. Source starts are0,2,4 modulo6. Local8/width sampling
     * would discard every tile's last column/row and alter settled borders. */
    static const uint8_t title_samples[3][8]={
        {1,2,3,4,5,7,0,0},
        {0,1,2,3,5,6,7,0},
        {0,1,3,4,5,6,7,0}
    };
    static const uint8_t native_samples[8]={0,1,2,3,4,5,6,7};
    /* Gameplay can replace any physical slot or invalidate the scratch.
     * Search its current tags; the shared order is only an eviction hint. */
    unsigned screen=v->scene_index-64,credits=frontend_credits(),i,j,painted=0;
    unsigned title_mask=v->globals?(v->globals[0x70]&3u):0;
    unsigned backdrop=frontend_layout_backdrop(screen);
    int clear;
    if(previous_scene!=v->scene_index || previous_frontend_credits!=credits)
    {first_background=1;previous_scene=v->scene_index;previous_frontend_credits=credits;}
    clear=first_background;
    for(i=0;i<128;++i)bank_changed[i]=0;
    if(first_background || previous_palette_revision!=v->palette_revision)
    {
        for(i=0;i<2048;++i)
        {
            unsigned at=i*2;
            if(first_background || previous_palette[at]!=v->palette[at] || previous_palette[at+1]!=v->palette[at+1])
            {
                unsigned rgb=le16(v->palette+at);
                /* The settled star's opaque119 matte must join the009
                 * title field, just as the moving SUPER actors already do. */
                if((screen==2 || screen==3 || screen==6 || screen==FRONTEND_HOME) && i==0x26d && rgb==0x119)
                    rgb=le16(v->palette+2*backdrop);
                /* The SELECT GAME blink hides its heading with bank44.
                 * Use the menu's surrounding gray instead of a black box. */
                if((screen==4 || screen==5) && i>=0x440 && i<0x450 && !rgb)
                    rgb=le16(v->palette+2*backdrop);
                colors[i]=rc_rgb555(rgb);bank_changed[i/16]=1;
                previous_palette[at]=v->palette[at];previous_palette[at+1]=v->palette[at+1];
                if(i==backdrop)clear=1;
            }
        }
        previous_palette_revision=v->palette_revision;
    }
    for(i=0;i<16;++i)if(bank_changed[i])
    {
        volatile uint16_t *lut=(volatile uint16_t *)(JO_VDP1_VRAM+cluts[i]*8);
        lut[0]=0;for(j=0;j<15;++j)lut[j+1]=colors[i*16+j];
    }
    if(clear)
    {
        for(unsigned y=0;y<240;++y)
        {
            if(!(y&7))jo_audio_service_music();
            volatile uint16_t *dest=background_target+y*JO_VDP2_WIDTH;
            for(unsigned x=0;x<320;++x){
                unsigned color=colors[backdrop];
                if(screen==FRONTEND_CENTER){
                    color=0x8000;
                    if(!(x%16)||!(y%16))color=0xbdef;
                    if(x==8||x==311||y==8||y==231)color=0x83ff;
                    if(x==160||y==120)color=0x801f;
                }
                dest[x]=(uint16_t)color;
            }
        }
    }
    if(screen==FRONTEND_CENTER){first_background=0;return;}
    for(i=0;i<2048;++i)
    {
        unsigned at=i*2,attr=v->attrs[i];int x,y,w,h;
        if(!clear && !bank_changed[attr&127] && !bank_changed[0] &&
           previous_attrs[i]==attr && previous_chars[at]==v->chars[at] && previous_chars[at+1]==v->chars[at+1])continue;
        previous_attrs[i]=(uint8_t)attr;previous_chars[at]=v->chars[at];previous_chars[at+1]=v->chars[at+1];
        if(!frontend_layout_cell(screen,i,credits,&x,&y,&w,&h))continue;
        unsigned code=le16(v->chars+at)&32767;
        if(screen==FRONTEND_CENTER && code==0x70e)continue;
        int offset=glyph_offset(code);
        if(offset<0)return;
        unsigned block=(unsigned)offset/4096,slot,position,empty=8;
        for(position=0;position<8;++position)
        {
            slot=glyph_order[position];
            if(glyph_blocks[slot]==block)break;
            if(glyph_blocks[slot]==~0u && empty==8)empty=position;
        }
        if(position==8)position=empty<8?empty:7;
        slot=glyph_order[position];
        /* Dirty panels need not contain source column0 or32. Count actual
         * visible work, and refill before every cold4KiB decode: otherwise a
         * sparse confirmation blink can spend many fields without service. */
        if(!(painted++&31u) || glyph_blocks[slot]!=block)jo_audio_service_music();
        if(glyph_blocks[slot]!=block)
        {
            glyph_blocks[slot]=~0u;
            if(!rc_glyph_block_decode(glyphs,ART_GLYPH_PACKED_BYTES,block,source_scratch+slot*4096,4096))
            {fail(RT_FAULT_GLYPH,code);return;}
            glyph_blocks[slot]=block;
        }
        for(unsigned recent=position;recent>0;--recent)glyph_order[recent]=glyph_order[recent-1];
        glyph_order[0]=(uint8_t)slot;
        const uint8_t *glyph=source_scratch+slot*4096+((unsigned)offset&4095);
        const uint8_t *sample_x=w==8?native_samples:title_samples[(i%64+1)%3];
        const uint8_t *sample_y=h==8?native_samples:title_samples[(i/64+2)%3];
        for(int dy=0;dy<h;++dy)if(y+dy>=0 && y+dy<240)
        {
            volatile uint16_t *dest=background_target+(y+dy)*JO_VDP2_WIDTH;
            unsigned gy=sample_y[dy];
            for(int dx=0;dx<w;++dx)if(x+dx>=0 && x+dx<320)
            {
                unsigned gx=sample_x[dx];if(attr&128)gx=7-gx;
                unsigned p=glyph[gy*4+gx/2],pen=gx&1?p&15:p>>4;
                dest[x+dx]=colors[pen==15?0:(attr&127)*16+pen];
            }
        }
    }
    if((screen==2 || screen==3 || screen==6 || screen==FRONTEND_HOME) &&
       (clear || title_mask!=previous_frontend_title_mask ||
        bank_changed[0x20] || bank_changed[0x21] || bank_changed[0x22] ||
        bank_changed[0x23] || bank_changed[0x24])){
        for(unsigned y=112;y<192;++y){
            if(!(y&7))jo_audio_service_music();
            volatile uint16_t *dest=background_target+y*JO_VDP2_WIDTH;
            for(unsigned x=0;x<320;++x){
                int color=frontend_title_native_background(title_mask,(int)x,(int)y);
                dest[x]=colors[color<0?backdrop:(unsigned)color];
            }
        }
    }
    previous_frontend_title_mask=title_mask;first_background=0;
}
int rt_init_art(const stage_view *v)
{
    uint8_t *p;unsigned i,j;jo_color lut[16];static const uint8_t blank[32]={0};
    fault_flags=fault_detail=fault_first_flags=cache_count=video_ready=0;
    fault_code=0;ready=0;previous_scene=~0u;
    /* The frontend exists before video initialization. Preserve its first
     * error before a later reservation or asset operation can fail. */
    if(v->fault)rt_report_fault_code(RT_FAULT_SIMULATION,v->fault);
    invalidate_glyph_blocks();
    background_target=(volatile uint16_t *)JO_VDP2_VRAM_A0;
    rendered_scene_revision=v->scene_revision;
    for(i=0;i<2048;++i)cell_ids[i]=-1;
    for(i=0;i<25;++i)item_ids[i]=-1;
    demo_prompt_id=pause_prompt_id=-1;demo_prompt_clut=pause_prompt_clut=0;
    for(i=0;i<FRONTEND_TITLE_CACHE_COUNT;++i)frontend_title_cells[i]=0;
    for(i=0;i<8;++i)pang_actor_texture[i]=-1;
    previous_frontend_credits=~0u;previous_frontend_title_mask=~0u;scene_invalidated=0;
    for(i=0;i<ART_AUTHORED_COUNT;++i)auth_ids[i]=-1;
    for(i=0;i<4;++i)for(j=0;j<6;++j)ladder_ids[i][j]=-1;
    prepare_authored_order();
    if(!__jo_vdp2_reserve_nbg1_bitmap()){fail(RT_FAULT_ASSET,1);return 0;}
    if(!jo_vdp2_enable_nbg1_double_buffer()){fail(RT_FAULT_ASSET,6);return 0;}
    video_ready=1;
    if(fault_flags){
        /* Only the reserved bitmaps are needed for the automatic fault
         * panel; a failed frontend must not load further art or audio. */
        jo_vdp2_zoom_nbg1f(1.0f,1.0f);jo_vdp2_move_nbg1(0,0);
        jo_set_displayed_screens(JO_NBG1_SCREEN|JO_SPRITE_SCREEN);
        return 0;
    }
    /* Keep the wraparound area black when the user shifts the 320x240 image. */
    {
        volatile uint16_t *hidden=jo_vdp2_get_nbg1_hidden_bitmap();
        for(unsigned y=0;y<256;++y)for(unsigned x=y<240?320:0;x<512;++x){
            background_target[y*512+x]=0x8000;hidden[y*512+x]=0x8000;
        }
    }
    for(i=0;i<16;++i)
    {
        int id;lut[0]=0;for(j=0;j<15;++j)lut[j+1]=rc_rgb555(le16(v->palette+2*(16*i+j)));
        id=jo_sprite_add_4bits_lut(blank,8,8,lut);
        if(id<0){fail(RT_FAULT_TEXTURE,1);return 0;}cluts[i]=__jo_sprite_lut[id];
    }
    /* One resident512-byte block holds the second scene's16 CLUTs. VDP1
     * repeats its previous table in1-cycle mode during a slow hidden-bitmap
     * repaint, so those commands must retain their original colors too.
     * This storage texture is never drawn; existing draw calls select CLUTs. */
    {
        int id=__jo_sprite_add_4bits(texture_scratch,128,8,cluts[0]);
        if(id<0){fail(RT_FAULT_TEXTURE,2);return 0;}
        for(i=0;i<16;++i)spare_cluts[i]=(uint16_t)(__jo_sprite_def[id].adr+i*4);
    }
    /* A separate CLUT for immutable calibration glyphs. */
    {
        int lut_id=__jo_sprite_add_4bits((const uint8_t *)lut,8,8,cluts[0]);
        if(lut_id<0){fail(RT_FAULT_TEXTURE,12300);return 0;}
        center_lut=__jo_sprite_def[lut_id].adr;
        for(unsigned code=0;code<128;code++)center_glyph_ids[code]=-1;
    }
    /* PAUSED is resident from initialization: entering pause performs no
     * allocation, CD access or bitmap mutation.  Store its CLUT in an
     * ordinary32-byte VDP1 block, as the renderer already does for spare
     * scene palettes, instead of consuming a seventeenth inline CLUT slot. */
    {
        int pause_lut_sprite=__jo_sprite_add_4bits((const uint8_t *)r14_pause_prompt_clut,8,8,cluts[0]);
        if(pause_lut_sprite<0){fail(RT_FAULT_TEXTURE,12200);return 0;}
        pause_prompt_clut=__jo_sprite_def[pause_lut_sprite].adr;
        pause_prompt_id=(int16_t)__jo_sprite_add_4bits(
            r14_pause_prompt,R14_PAUSE_PROMPT_WIDTH,R14_PAUSE_PROMPT_HEIGHT,
            pause_prompt_clut);
        if(pause_prompt_id<0){fail(RT_FAULT_TEXTURE,12201);return 0;}
    }
    boot_diag_mark(21);cells=load_art("CELLH4.BIN",ART_CELL_HEAD_BYTES);if(!cells)return 0;
    boot_diag_mark(22);cell_tail=load_art("CELLT4.BIN",ART_CELL_TAIL_BYTES);if(!cell_tail)return 0;
    if(!rc_cell_catalog_valid_split(cells,ART_CELL_HEAD_BYTES,ART_CELL_TAIL_BYTES)){fail(RT_FAULT_ASSET,10);return 0;}
    /* The immutable source patterns are also graphics memory. Keeping the
     * 160KiB head in VDP1 frees ordinary WRAM for all Tour music prefixes and
     * the stream buffer. Ten valid256x128 storage textures are never drawn.
     * Their addresses are contiguous by Jo's ordinary texture allocator. */
    {
        uint8_t *dictionary=(uint8_t *)jo_malloc(4096);if(!dictionary){fail(RT_FAULT_ASSET,12);return 0;}
        for(unsigned n=0;n<4096;++n)dictionary[n]=cells[n];
        if(ART_CELL_HEAD_BYTES!=4096+10*16384){jo_free(dictionary);fail(RT_FAULT_ASSET,13);return 0;}
        cell_patterns=JO_NULL;
        for(unsigned n=0;n<10;++n){
            int id=__jo_sprite_add_4bits(cells+4096+n*16384,256,128,cluts[0]);
            if(id<0){jo_free(dictionary);fail(RT_FAULT_TEXTURE,7000+n);return 0;}
            const uint8_t *address=(const uint8_t *)(JO_VDP1_VRAM+__jo_sprite_def[id].adr*8);
            if(!n)cell_patterns=address;
            else if(address!=cell_patterns+n*16384){jo_free(dictionary);fail(RT_FAULT_ASSET,14);return 0;}
        }
        jo_free(cells);cells=dictionary;
    }
    boot_diag_mark(23);p=load_art("AUTH4.BIN",ART_AUTHORED_BYTES);if(!p)return 0;
    for(i=0;i<ART_AUTHORED_COUNT;++i)
    {
        const ArtAuthoredRecord *a=&art_authored_records[i];
        /* Visible ladders use the world-sampled ladder_ids cells in draw_ladder. */
        if(a->role==STAGE_ROLE_LADDER)continue;
        auth_ids[i]=__jo_sprite_add_4bits(p+a->offset,a->width,a->height,cluts[a->palette_bank]);
        if(auth_ids[i]<0)fail(RT_FAULT_TEXTURE,2048+i);
    }
    jo_free(p);if(fault_flags)return 0;
    boot_diag_mark(24);glyph_index=load_art("CHARMDF.BIN",ART_GLYPH_MAP_BYTES);if(!glyph_index)return 0;
    if(!rc_glyph_catalog_valid(glyph_index,ART_GLYPH_MAP_BYTES,ART_GLYPH_BYTES))
    {fail(RT_FAULT_ASSET,9);return 0;}
    boot_diag_mark(25);glyphs=load_art("CHARDF.BIN",ART_GLYPH_PACKED_BYTES);if(!glyphs)return 0;
    if(!center_prepare_glyphs())return 0;
    if(!rc_glyph_blocks_valid(glyphs,ART_GLYPH_PACKED_BYTES,ART_GLYPH_BYTES,source_scratch,sizeof source_scratch))
    {fail(RT_FAULT_ASSET,11);return 0;}
    boot_diag_mark(26);p=load_art("VIEWX.BIN",1280);if(!p)return 0;
    for(i=0;i<640;++i)viewx[i]=be16(p+i*2);
    jo_free(p);p=load_art("VIEWY.BIN",480);if(!p)return 0;
    for(i=0;i<240;++i)viewy[i]=be16(p+i*2);
    jo_free(p);prepare_background_runs();if(fault_flags)return 0;
    jo_vdp2_zoom_nbg1f(1.0f,1.0f);jo_vdp2_move_nbg1(0,0);
    jo_set_displayed_screens(JO_NBG1_SCREEN|JO_SPRITE_SCREEN);
    first_background=1;
    if(v->phase==STAGE_FRONTEND)update_frontend_background(v);else update_background(v);
    ready=1;return fault_flags==0;
}
static void draw_texture(int id,int left,int top,int depth,unsigned lut)
{
    jo_vdp1_sprite_position p;jo_vdp1_sprite_attribute a;
    if(id<0)return;
    p.x=jo_int2fixed(left+screen_offset_x-160+__jo_sprite_def[id].width/2);
    p.y=jo_int2fixed(top+screen_offset_y-120+__jo_sprite_def[id].height/2);
    p.z=jo_int2fixed(depth);p.scale_x=p.scale_y=JO_FIXED_1;
    a.texture_id=id;a.draw_mode=JO_VDP1_END_CODE_DISABLE|JO_VDP1_COLOR_MODE_16_LOOKUP;
    a.color=lut;a.gouraud_table=JO_VDP1_NO_GOURAUD;a.direction=JO_VDP1_FUNCTION_SPRITE;
    __jo_vdp1_draw_sprite(&p,&a,0);
}
/* The calibration HUD is screen-fixed: do not feed it through draw_texture,
 * whose coordinates intentionally include the user's game-screen offset. */
static const uint8_t *center_glyph(unsigned code)
{
    int offset=glyph_offset(code);if(offset<0)return 0;
    unsigned block=(unsigned)offset/4096,position,slot;
    for(position=0;position<8;position++)if(glyph_blocks[glyph_order[position]]==block)break;
    if(position==8)position=7;
    slot=glyph_order[position];
    if(glyph_blocks[slot]!=block){
        glyph_blocks[slot]=~0u;
        if(!rc_glyph_block_decode(glyphs,ART_GLYPH_PACKED_BYTES,block,source_scratch+slot*4096,4096))
        {fail(RT_FAULT_GLYPH,code);return 0;}
        glyph_blocks[slot]=block;
    }
    for(unsigned n=position;n>0;n--)glyph_order[n]=glyph_order[n-1];
    glyph_order[0]=(uint8_t)slot;
    return source_scratch+slot*4096+((unsigned)offset&4095);
}
static int center_prepare_glyphs(void)
{
    static const char chars[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-";
    for(unsigned i=0;chars[i];i++){
        unsigned code=(unsigned char)chars[i];const uint8_t *pixels=center_glyph(code);if(!pixels)return 0;
        int id=__jo_sprite_add_4bits(pixels,8,8,center_lut);
        if(id<0){fail(RT_FAULT_TEXTURE,12301);return 0;}
        center_glyph_ids[code]=(int16_t)id;
    }
    return 1;
}
static void draw_center_help(const stage_view *v)
{
    static const unsigned rows[4]={3,22,25,27};
    volatile uint16_t *lut=(volatile uint16_t *)(JO_VDP1_VRAM+center_lut*8);
    for(unsigned pen=0;pen<16;pen++)lut[pen]=pen==15 || colors[0x270+pen]==colors[0x20d]?JO_COLOR_Black:colors[0x270+pen];
    jo_sprite_draw_rectangle(80,8,160,24,JO_COLOR_Black,110);
    jo_sprite_draw_rectangle(24,160,272,64,JO_COLOR_Black,110);
    /* Glyphs never change after upload. A previous VDP1 table may still use
     * them while this frame selects different digit IDs and positions. */
    for(unsigned line=0;line<4;line++)for(unsigned col=0;col<32;col++){
        unsigned code=le16(v->chars+(rows[line]*64+col+16)*2)&32767;
        if(code==0x70e)continue;
        if(code>=128 || center_glyph_ids[code]<0){fail(RT_FAULT_GLYPH,code);return;}
        jo_vdp1_sprite_position pos;jo_vdp1_sprite_attribute attr;
        pos.x=jo_int2fixed((int)col*8+32+4-160);pos.y=jo_int2fixed((int)rows[line]*8-8+4-120);
        pos.z=jo_int2fixed(100);pos.scale_x=pos.scale_y=JO_FIXED_1;
        attr.texture_id=center_glyph_ids[code];attr.draw_mode=JO_VDP1_END_CODE_DISABLE|JO_VDP1_COLOR_MODE_16_LOOKUP;
        attr.color=center_lut;attr.gouraud_table=JO_VDP1_NO_GOURAUD;attr.direction=JO_VDP1_FUNCTION_SPRITE;
        __jo_vdp1_draw_sprite(&pos,&attr,0);
    }
}

static int single_cell(unsigned code)
{
    /* Keep the exact approved CELL15 pixels, but upload a fallback only when
     * used. CELLS.N4 is already resident for compound sprites. Textures are
     * immutable after allocation, including while a previous frame displays. */
    if(code>=2048){fail(RT_FAULT_OBJECT,code);return -2;}
    if(cell_ids[code]!=-1)return cell_ids[code];
    const uint8_t *source=rc_cell_pixels_split(
        cell_patterns,cells,cell_tail,ART_CELL_HEAD_PATTERNS,code);
    for(unsigned y=0;y<16;++y)
    {
        const uint8_t *s=source+y*8;
        uint8_t *d=texture_scratch+y*8;
        /* The approved sample formula keeps columns0..6,8..15. Pack the
         * same fifteen pens and a transparent final column directly. */
        d[0]=s[0];d[1]=s[1];d[2]=s[2];
        d[3]=(uint8_t)((s[3]&0xf0)|(s[4]>>4));
        d[4]=(uint8_t)((s[4]<<4)|(s[5]>>4));
        d[5]=(uint8_t)((s[5]<<4)|(s[6]>>4));
        d[6]=(uint8_t)((s[6]<<4)|(s[7]>>4));
        d[7]=(uint8_t)(s[7]<<4);
    }
    if(!any_pixels(texture_scratch,128)){cell_ids[code]=-2;return -2;}
    int id=__jo_sprite_add_4bits(texture_scratch,16,16,cluts[0]);
    if(id<0){fail(RT_FAULT_TEXTURE,code);cell_ids[code]=-2;return -2;}
    cell_ids[code]=(int16_t)id;return id;
}
static int frontend_texture(unsigned code,unsigned phase_x,unsigned phase_y,unsigned w,unsigned h,int sampled)
{
    uint32_t key=(code<<6)+(sampled?phase_y*6+phase_x:36)+1;
    unsigned slot=(key*2654435761u)>>(32-9);
    for(unsigned n=0;n<FRONTEND_TITLE_CACHE_COUNT;++n)
    {
        uint32_t *entry=&frontend_title_cells[slot];
        if((*entry>>12)==key)return (int)(*entry&4095u)-3;
        if(!*entry)
        {
            const uint8_t *source=rc_cell_pixels_split(cell_patterns,cells,cell_tail,ART_CELL_HEAD_PATTERNS,code);
            if(sampled && frontend_title_native_cell(code,phase_x,phase_y,w,h,texture_scratch)){}
            else if(sampled)
            {
                unsigned xs[14],ys[14];
                for(unsigned x=0;x<w;++x)xs[x]=frontend_layout_title_sample(phase_x,x);
                for(unsigned y=0;y<h;++y)ys[y]=frontend_layout_title_sample(phase_y,y);
                for(unsigned byte=0;byte<16*h/2;++byte)texture_scratch[byte]=0;
                for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
                {
                    unsigned sx=xs[x],packed=source[ys[y]*8+sx/2],pen=sx&1?packed&15:packed>>4;
                    if(x&1)texture_scratch[y*8+x/2]|=(uint8_t)pen;
                    else texture_scratch[y*8+x/2]=(uint8_t)(pen<<4);
                }
            }
            else for(unsigned byte=0;byte<128;++byte)texture_scratch[byte]=source[byte];
            int id=any_pixels(texture_scratch,16*h/2)?__jo_sprite_add_4bits(texture_scratch,16,h,cluts[0]):-2;
            if(id==-1 || id>=4093){fail(RT_FAULT_TEXTURE,8000+code);return -1;}
            *entry=(key<<12)|(unsigned)(id+3);return id;
        }
        slot=(slot+1)&(FRONTEND_TITLE_CACHE_COUNT-1);
    }
    fail(RT_FAULT_TEXTURE,9000+code);return -1;
}
static int title_project_x(int value)
{
    int product=value*5;
    return product>=0?product/6:-((-product+5)/6);
}
static void draw_pang_actor(unsigned actor,const uint8_t *p,unsigned depth,unsigned palette)
{
    static const int anchors[4]={112,200,304,392};
    if(pang_actor_texture[actor]<0){
        int left,top;unsigned width,height;
        if(!frontend_title_native_raster(actor,texture_scratch,sizeof texture_scratch,
                                         &left,&top,&width,&height))
        {fail(RT_FAULT_TEXTURE,12320+actor);return;}
        int id=__jo_sprite_add_4bits(texture_scratch,width,height,cluts[0]);
        if(id<0){fail(RT_FAULT_TEXTURE,12330+actor);return;}
        pang_actor_texture[actor]=(int16_t)id;
        pang_actor_left[actor]=(int16_t)left;pang_actor_top[actor]=(int16_t)top;
    }
    int id=pang_actor_texture[actor];
    int ox=(int)(le16(p+24)&511u),oy=(int)(le16(p+26)&255u);
    int base_y=oy-152+pang_actor_top[actor];
    int ref_x=title_project_x(anchors[actor/2]-64);
    int width=__jo_sprite_def[id].width,height=__jo_sprite_def[id].height;
    /* Each original hardware cell wraps independently. Draw neighboring wrap
     * copies when necessary, while keeping the complete Mega Drive shape. */
    for(int py=-1;py<=1;py++)for(int px=-1;px<=1;px++){
        int x=title_project_x(ox+px*512-64)-ref_x+pang_actor_left[actor];
        int y=base_y+py*256;
        int visible_x=x+screen_offset_x,visible_y=y+screen_offset_y;
        if(visible_x<320 && visible_x+width>0 && visible_y<240 && visible_y+height>0)
            draw_texture(id,x,y,(int)depth,cluts[palette]);
    }
}
static void draw_frontend(const stage_view *v)
{
    unsigned screen=v->scene_index-64;
    unsigned pang_groups=0;
    if(v->object_count>128){fail(RT_FAULT_OBJECT,v->object_count);return;}
    for(unsigned i=v->object_count;i>0;--i)
    {
        const uint8_t *p=v->objects+(i-1)*32;
        unsigned code=p[0]+((unsigned)(p[1]&0xe0)<<3),palette=p[1]&15;
        int sx=p[3]+((p[1]&16)?256:0),sy=((p[2]+8)&255)-8,x,y,w,h;
        int id,sampled=(screen==2 || screen==3 || screen==6 || screen==FRONTEND_HOME) && p[19]==0 && p[20] && p[21]==2;
        if(sampled)
        {
            int actor=frontend_title_native_actor(le16(p+22));
            if(actor>=0 && p[20]<=8){
                unsigned bit=1u<<(p[20]-1);
                if(!(pang_groups&bit)){
                    pang_groups|=bit;
                    draw_pang_actor((unsigned)actor,p,500+i-1,palette);
                }
                continue;
            }
            unsigned px,py;
            int visible=frontend_layout_title_object(p,&x,&y,&w,&h,&px,&py);
            if(visible<0){fail(RT_FAULT_OBJECT,5000+i);return;}
            if(!visible)continue;
            id=frontend_texture(code,px,py,(unsigned)w,(unsigned)h,1);
        }
        else
        {
            if(!frontend_layout_sprite(screen,sx,sy,p[19],&x,&y,&w,&h))continue;
            id=frontend_texture(code,0,0,16,16,0);
        }
        if(id<0)continue;
        jo_vdp1_sprite_position pos;jo_vdp1_sprite_attribute attr;
        pos.x=jo_int2fixed(x+screen_offset_x-160)+(sampled?16:w)*(JO_FIXED_1/2);
        pos.y=jo_int2fixed(y+screen_offset_y-120)+h*(JO_FIXED_1/2);
        pos.z=jo_int2fixed(500+i-1);
        pos.scale_x=sampled?JO_FIXED_1:w*(JO_FIXED_1/16);
        pos.scale_y=sampled?JO_FIXED_1:h*(JO_FIXED_1/16);
        attr.texture_id=id;attr.draw_mode=JO_VDP1_END_CODE_DISABLE|JO_VDP1_COLOR_MODE_16_LOOKUP;
        attr.color=cluts[palette];attr.gouraud_table=JO_VDP1_NO_GOURAUD;attr.direction=JO_VDP1_FUNCTION_SPRITE;
        /* Endpoint projection can yield 14x13 or 13x14. The uniform API
         * ignores scale_y and would overlap or leave gaps between cells. */
        __jo_vdp1_draw_sprite_hv(&pos,&attr,0);
    }
}
/* Ladders are world geometry. Sample each original cell on the same inverse
 * grid as the bitmap, including the original odd-X ladder in Tour35. A fixed
 * four-code/six-phase cache bounds VRAM and avoids whole-length mask variants.
 * Padded transparent columns never cover the adjacent cell's world pixels. */
static void draw_ladder(unsigned group)
{
    const rc_group *g=&scene.groups[group];uint32_t mask;
    if(!rc_ladder_mask(&scene,group,&mask))
    {fail(RT_FAULT_OBJECT,g->source_code);return;}
    for(unsigned n=scene.object_count;n>0;--n)
    {
        const rc_object *o=&scene.objects[n-1];
        if(o->group!=group)continue;
        unsigned row=(unsigned)(o->y-g->top)/16,col=(unsigned)(o->x-g->left)/16;
        if(mask&(1u<<(row*2+col)))continue;
        unsigned code=o->code,index=(code==0x206?0:code==0x207?1:code==0x20e?2:3);
        int phase=(o->x-64)%6;if(phase<0)phase+=6;
        int id=ladder_ids[index][phase];
        if(id<0)
        {
            const uint8_t *source=rc_cell_pixels_split(cell_patterns,cells,cell_tail,ART_CELL_HEAD_PATTERNS,code);
            unsigned origin=(unsigned)rc_world_boundary_x(64+phase);
            unsigned width=(unsigned)rc_world_boundary_x(80+phase)-origin;
            for(unsigned i=0;i<128;++i)texture_scratch[i]=0;
            for(unsigned y=0;y<16;++y)for(unsigned x=0;x<width;++x)
            {
                unsigned sx=(6*(origin+x)+3)/5-(unsigned)phase;
                unsigned packed=source[y*8+sx/2],pen=sx&1?packed&15:packed>>4;
                unsigned at=y*8+x/2;
                if(x&1)texture_scratch[at]|=(uint8_t)pen;
                else texture_scratch[at]=(uint8_t)(pen<<4);
            }
            id=__jo_sprite_add_4bits(texture_scratch,16,16,cluts[0]);
            if(id<0){fail(RT_FAULT_TEXTURE,5000+index*6+(unsigned)phase);return;}
            ladder_ids[index][phase]=(int16_t)id;
        }
        draw_texture(id,rc_world_boundary_x(o->x),o->y-8,500+g->first,cluts[g->palette]);
    }
}
/* First ROM platform frames are rectangular solid-pen10 flashes. Project
 * that verified occupied rectangle on the world's grid without uploading
 * position-phase texture variants. */
static int draw_platform_flash(unsigned group)
{
    const rc_group *g=&scene.groups[group];
    int left=32767,top=32767,right=-32768,bottom=-32768;
    unsigned occupied=0;
    if(g->role!=STAGE_ROLE_PLATFORM || g->source_code<0x3a2 || g->source_code>0x3ac)return 0;
    for(unsigned i=0;i<scene.object_count;++i)
    {
        const rc_object *o=&scene.objects[i];
        if(o->group!=group)continue;
        const uint8_t *cell=rc_cell_pixels_split(cell_patterns,cells,cell_tail,ART_CELL_HEAD_PATTERNS,o->code);
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)
        {
            unsigned packed=cell[y*8+x/2],pen=x&1?packed&15:packed>>4;
            if(!pen)continue;
            if(pen!=11)return 0;
            int sx=o->x+(int)x,sy=o->y+(int)y;
            if(sx<left)left=sx;
            if(sx>=right)right=sx+1;
            if(sy<top)top=sy;
            if(sy>=bottom)bottom=sy+1;
            ++occupied;
        }
    }
    /* Do not bridge transparent gaps or accept a merely similar sprite. */
    if(!occupied || occupied!=(unsigned)((right-left)*(bottom-top)))return 0;
    int native_left=rc_world_boundary_x(left),native_right=rc_world_boundary_x(right);
    jo_sprite_draw_rectangle(native_left+screen_offset_x,top-8+screen_offset_y,native_right-native_left,bottom-top,
        colors[g->palette*16+10],500+g->first);
    return 1;
}
static void draw_platform_fragments(unsigned index)
{
    const rc_group *g=&scene.groups[index];
    uint32_t key[2];unsigned i,width,height;
    int phase=(g->left-64)%6;if(phase<0)phase+=6;
    /* Original platform origins are even and stay fixed during the effect.
     * Do not silently round a malformed origin to another sampling phase. */
    if(phase&1){fail(RT_FAULT_OBJECT,11000+g->source_code);return;}
    rc_group_key(&scene,index,key);
    for(i=0;i<cache_count;++i)if(cache[i].native==3 &&
        cache[i].key[0]==key[0] && cache[i].key[1]==key[1])break;
    if(i==cache_count)
    {
        int first=-1;
        if(i>=CACHE_COUNT){fail(RT_FAULT_TEXTURE,3000+i);return;}
        invalidate_glyph_blocks();
        if(!rc_compose_platform_split(&scene,index,cell_patterns,cells,cell_tail,ART_CELL_HEAD_PATTERNS,
            source_scratch,texture_scratch,sizeof texture_scratch,&width,&height))
        {fail(RT_FAULT_OBJECT,g->source_code);return;}
        unsigned bytes=width*height/2;
        for(unsigned variant=0;variant<3;++variant)
        {
            int id=__jo_sprite_add_4bits(texture_scratch+variant*bytes,width,height,cluts[0]);
            if(id<0 || (variant && id!=first+(int)variant))
            {fail(RT_FAULT_TEXTURE,11000+i);return;}
            if(!variant)first=id;
        }
        /* Jo uploads synchronously and appends IDs. Publish only when the
         * entire immutable triplet is resident; later draws never rewrite it. */
        cached_group *c=&cache[cache_count++];
        c->key[0]=key[0];c->key[1]=key[1];c->native=3;c->sprite=first;
        c->width=width;c->height=height;c->anchor=0;
    }
    draw_texture(cache[i].sprite+phase/2,rc_world_boundary_x(g->left),g->top-8,500+g->first,cluts[g->palette]);
}
static void draw_group(unsigned index,int map,int icon_shift)
{
    const rc_group *g=&scene.groups[index];unsigned i,anchor,width,height;
    /* These original messages fit within320px without deleting columns.
     * Gameplay anchors remain projected; glyph geometry stays at1:1. */
    int native=map || g->role==STAGE_ROLE_TIMEOUT ||
        g->role==STAGE_ROLE_GAMEOVER || g->role==STAGE_ROLE_CONTINUE;
    if(!map && draw_platform_flash(index))return;
    if(!map && g->role==STAGE_ROLE_PLATFORM){draw_platform_fragments(index);return;}
    if(!map && g->role==STAGE_ROLE_LADDER){draw_ladder(index);return;}
    if(!map && g->role==STAGE_ROLE_ITEM && g->width==16 && g->height==16 && g->count==1){
        int item=r10_item_index(g->source_code);
        if(item>=0){
            if(item_ids[item]<0){
                int sprite=__jo_sprite_add_4bits(r10_item_pixels(g->source_code),16,16,cluts[0]);
                if(sprite<0){fail(RT_FAULT_TEXTURE,12000u+(unsigned)item);return;}
                item_ids[item]=(int16_t)sprite;
            }
            draw_texture(item_ids[item],rc_screen_x(g->left+8)-7,g->top-8,500+g->first,cluts[g->palette]);
            return;
        }
    }
    int id=-1,left=0,top=g->top-8;
    /* Map sprites use original cells on the same 1:1 grid as their map.
     * Gameplay continues to use the reviewed authored resources. */
    i=map?ART_AUTHORED_COUNT:authored_match(g,0);
    if(i<ART_AUTHORED_COUNT)
    {
        const ArtAuthoredRecord *a=&art_authored_records[i];
        id=auth_ids[i];
        if(g->role==STAGE_ROLE_WEAPON){left=rc_screen_x(g->left)-a->anchor_x;top-=a->anchor_y;}
        else{left=rc_screen_x(g->left+g->width/2)-a->anchor_x;top+=g->height/2-a->anchor_y;}
        draw_texture(id,left,top,500+g->first,cluts[g->palette]);return;
    }
    if(!map && g->width==16 && g->height==16 && g->count==1)
    {id=single_cell(g->source_code);left=rc_screen_x(g->left+8)-7;}
    else
    {
        uint32_t key[2];rc_group_key(&scene,index,key);
        for(i=0;i<cache_count;++i)if(cache[i].native==(unsigned)native && cache[i].key[0]==key[0] && cache[i].key[1]==key[1])break;
        if(i==cache_count)
        {
            cached_group *c;int composed;
            if(i>=CACHE_COUNT){fail(RT_FAULT_TEXTURE,3000+i);return;}
            /* Composition reuses the glyph bytes, including on failure. */
            invalidate_glyph_blocks();
            composed=rc_compose_split(&scene,index,cell_patterns,cells,cell_tail,ART_CELL_HEAD_PATTERNS,source_scratch,texture_scratch,sizeof(texture_scratch),&width,&height,&anchor,native);
            if(!composed)
            {fail(RT_FAULT_OBJECT,g->source_code);return;}
            c=&cache[cache_count++];c->key[0]=key[0];c->key[1]=key[1];
            c->width=width;c->height=height;c->anchor=anchor;c->native=(uint8_t)native;
            c->sprite=any_pixels(texture_scratch,width*height/2)?__jo_sprite_add_4bits(texture_scratch,width,height,cluts[0]):-1;
            if(c->sprite<0 && any_pixels(texture_scratch,width*height/2))fail(RT_FAULT_TEXTURE,4000+i);
        }
        id=cache[i].sprite;
        /* rc_collect admits one overwide source: the screen-filling Lucky Bonus. */
        if(g->width>RC_MAX_WIDTH)left=0;
        else left=(map?rc_map_screen_x(g->left+g->width/2):rc_screen_x(g->left+g->width/2))-cache[i].anchor;
    }
    draw_texture(id,left+icon_shift,top,500+g->first,cluts[g->palette]);
}
static void draw_demo_prompt(const stage_view *v)
{
    if(!stage_is_demo() || (v->tick&63u)>=48u)return;
    if(demo_prompt_id<0){
        /* Keep the caption independent of gameplay palette animation. The
         * small storage texture owns its CLUT just like spare scene banks. */
        int lut=__jo_sprite_add_4bits((const uint8_t *)r10_start_prompt_clut,8,8,cluts[0]);
        if(lut<0){fail(RT_FAULT_TEXTURE,12100);return;}
        demo_prompt_clut=__jo_sprite_def[lut].adr;
        int id=__jo_sprite_add_4bits(r10_start_prompt,R10_START_PROMPT_WIDTH,8,demo_prompt_clut);
        if(id<0){fail(RT_FAULT_TEXTURE,12101);return;}
        demo_prompt_id=(int16_t)id;
    }
    draw_texture(demo_prompt_id,(320-R10_START_PROMPT_WIDTH)/2,184,100,demo_prompt_clut);
}
void rt_draw_stage(const stage_view *v)
{
    unsigned i;int switched=0;
    int name=v->phase==STAGE_FRONTEND && v->scene_index==73;
    if(name && !v->globals){fail(RT_FAULT_OBJECT,73);return;}
    unsigned scene_index=name?v->globals[0x7c]:v->scene_index;
    unsigned phase=name?v->globals[0x7d]:v->phase;
    int map=phase==STAGE_MAP || scene_index==54;if(!ready)return;
    if(scene_invalidated || rendered_scene_revision!=v->scene_revision ||
       (map && viewx[640]!=background_origin(v)))
    {
        jo_color *hidden=jo_vdp2_get_nbg1_hidden_bitmap();
        if(!hidden){fail(RT_FAULT_ASSET,7);return;}
        /* The pending-scene gate prevents a second scene handoff before
         * this one is displayed. Reuse the old bank only after that commit;
         * first_background below repopulates every new scene CLUT. */
        for(i=0;i<16;++i){uint16_t old=cluts[i];cluts[i]=spare_cluts[i];spare_cluts[i]=old;}
        background_target=hidden;first_background=1;switched=1;
    }
    else if((!map && v->phase!=STAGE_FRONTEND &&
             (bonus_panel_mask(v)!=background_bonus_mask ||
              (background_bonus_mask && !background_bonus_awarded && bonus_awarded(v)))) ||
            (v->phase==STAGE_FRONTEND && !name && v->globals &&
             (scene_index==66 || scene_index==67 || scene_index==70) &&
             (v->globals[0x70]&3u)!=previous_frontend_title_mask))
    {
        /* A newly appearing score panel changes six rows and their native
         * mapping. Painting those rows in the displayed bitmap exposed each
         * intermediate row while glyph decode/CD service crossed VBlanks.
         * Preserve the visible frame; patch the other bitmap and publish it
         * with the completed sprite table, just as at a scene boundary.
         * No new allocation and no full background decode are needed. */
        jo_color *hidden=jo_vdp2_get_nbg1_hidden_bitmap();
        if(!hidden){fail(RT_FAULT_ASSET,7);return;}
        for(unsigned y=0;y<240;++y){
            if(!(y&7))jo_audio_service_music();
            for(unsigned x=0;x<320;++x)hidden[y*JO_VDP2_WIDTH+x]=background_target[y*JO_VDP2_WIDTH+x];
        }
        for(i=0;i<16;++i){
            volatile uint16_t *src=(volatile uint16_t *)(JO_VDP1_VRAM+cluts[i]*8);
            volatile uint16_t *dst=(volatile uint16_t *)(JO_VDP1_VRAM+spare_cluts[i]*8);
            for(unsigned color=0;color<16;++color)dst[color]=src[color];
            uint16_t old=cluts[i];cluts[i]=spare_cluts[i];spare_cluts[i]=old;
        }
        background_target=hidden;switched=1;
    }
    if(v->phase==STAGE_FRONTEND && !name){
        update_frontend_background(v);draw_frontend(v);
        if(scene_index==64+FRONTEND_CENTER)draw_center_help(v);
    }
    else{
    update_background(v);
    if(!rc_collect(&scene,v->objects,v->object_count)){fail(RT_FAULT_OBJECT,v->object_count);return;}
    int icon_shift=phase==STAGE_MAP?96-(int)background_origin(v):
        (scene_index==54 && stage_ending_page()==0?8:0);
    for(i=scene.group_count;i>0;--i)draw_group(i-1,map,icon_shift);
    if(v->phase!=STAGE_FRONTEND)draw_demo_prompt(v);
    }
    if(switched && !fault_flags)
    {
        /* The scene becomes visible only when this VDP1 frame is displayed.
         * A slow full repaint never changes the current visible bitmap. */
        if(!jo_vdp2_queue_nbg1_bitmap((jo_color *)background_target,__jo_vdp1_frame_tag()))
        {fail(RT_FAULT_ASSET,8);return;}
        rendered_scene_revision=v->scene_revision;
        scene_invalidated=0;
    }
}
void rt_invalidate_scene(void){scene_invalidated=1;previous_scene=~0u;first_background=1;}
int rt_scene_pending(void){return ready && jo_vdp2_nbg1_bitmap_pending();}
int rt_scene_presented_vblank(unsigned *field)
{return ready && jo_vdp2_get_nbg1_presented_vblank(field);}
void rt_report_fault_code(unsigned flags,int code)
{
    if(!flags)return;
    if(!fault_flags){fault_first_flags=flags;fault_code=code;fault_detail=(unsigned)code;}
    fault_flags|=flags;
}
void rt_report_fault(unsigned flags){rt_report_fault_code(flags,0);}
unsigned rt_fault_first_flags(void){return fault_first_flags;}
int rt_fault_code(void){return fault_code;}
int rt_video_ready(void){return video_ready!=0;}
unsigned rt_fault_flags(void){return fault_flags;}
unsigned rt_fault_detail(void){return fault_detail;}
static void draw_number(unsigned value,int x,int y)
{
    static const uint8_t digits[10][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
    unsigned divisor=10000,d,row,col,i;value%=100000;
    for(i=0;i<5;++i){d=value/divisor;value%=divisor;divisor/=10;
        for(row=0;row<5;++row)for(col=0;col<3;++col)if(digits[d][row]&(4>>col))
            jo_sprite_draw_rectangle(x+i*4+col,y+row,1,1,JO_COLOR_White,110);}
}
void rt_draw_fault(void)
{
    if(!fault_flags)return;
    jo_sprite_draw_rectangle(0,232,320,8,JO_COLOR_Red,120);
    draw_number(fault_flags,4,234);draw_number(fault_detail,32,234);
}
void rt_draw_pause(void)
{
    /* Resident immutable texture: no stage, bitmap, cache or allocator writes. */
    if(pause_prompt_id<0){fail(RT_FAULT_TEXTURE,12202);return;}
    draw_texture(pause_prompt_id,(320-(int)R14_PAUSE_PROMPT_WIDTH)/2,
                 (240-(int)R14_PAUSE_PROMPT_HEIGHT)/2-8,85,pause_prompt_clut);
}
