#include <jo/jo.h>
#include "include/hardware_diag.h"
#include "audio/audio.h"

enum { DIAG_WIDTH=320, DIAG_HEIGHT=240, DIAG_STRIDE=512,
       DIAG_X=8, DIAG_Y=16, DIAG_COLS=22, DIAG_CHARS=44,
       DIAG_WORD_BUDGET=896 };

volatile rt_hw_diag_status rt_hw_diag_state
    __attribute__((used,externally_visible));

static unsigned diag_ready, diag_z_held, diag_drawn, diag_ended;
static unsigned diag_words_left, diag_cursor, diag_repaint_seen;
static char diag_text[DIAG_CHARS];
volatile rt_fault_report rt_hw_diag_fault_state
    __attribute__((used,externally_visible));
static volatile unsigned fault_latched;
static unsigned fault_cursor;
static char diag_fault_text[RT_FAULT_REPORT_ROWS*RT_FAULT_REPORT_COLS];

/* 5x7 rows: hexadecimal, I M P S W O T H, dash, blank. No asset or palette. */
static const unsigned char diag_font[][7]={
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
    {14,4,4,4,4,4,14},{17,27,21,21,17,17,17},
    {30,17,17,30,16,16,16},{15,16,16,14,1,1,30},
    {17,17,17,21,21,21,10},{14,17,17,17,17,17,14},
    {31,4,4,4,4,4,4},{17,17,17,31,17,17,17},
    {0,0,0,31,0,0,0},{0,0,0,0,0,0,0},
    {14,17,16,23,17,17,15},{16,16,16,16,16,16,31},
    {17,25,21,19,17,17,17},{30,17,17,30,20,18,17},
    {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
    {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},
    {17,18,20,24,20,18,17}
};

static unsigned diag_glyph(unsigned char c)
{
    if(c>='0' && c<='9')return c-'0';
    if(c>='A' && c<='F')return c-'A'+10;
    switch(c){
    case 'I':return 16;case 'M':return 17;case 'P':return 18;
    case 'S':return 19;case 'W':return 20;case 'O':return 21;
    case 'T':return 22;case 'H':return 23;case '-':return 24;
    case 'G':return 26;case 'L':return 27;case 'N':return 28;
    case 'R':return 29;case 'U':return 30;case 'V':return 31;
    case 'X':return 32;case 'Y':return 33;
    case 'K':return 34;
    default:return 25;
    }
}

static void diag_pixel(int x,int y,unsigned short color)
{
    volatile unsigned short *a=(volatile unsigned short *)JO_VDP2_VRAM_A0;
    volatile unsigned short *b=(volatile unsigned short *)JO_VDP2_VRAM_B0;
    unsigned offset;
    if((unsigned)x>=DIAG_WIDTH || (unsigned)y>=DIAG_HEIGHT || diag_words_left<2)return;
    offset=(unsigned)y*DIAG_STRIDE+(unsigned)x;
    a[offset]=color;b[offset]=color;diag_words_left-=2;
}

static void diag_paint_char(unsigned char c,int x,int y)
{
    static const unsigned char masks[12]={16,16,8,8,4,4,2,2,1,1,0,0};
    const unsigned char *glyph;
    unsigned dx,dy,bits;
    if(x<=-12 || x>=DIAG_WIDTH || y<=-16 || y>=DIAG_HEIGHT)return;
    glyph=diag_font[diag_glyph(c)];
    for(dy=0;dy<16;++dy){
        bits=dy<14?glyph[dy>>1]:0;
        for(dx=0;dx<12;++dx)
            diag_pixel(x+(int)dx,y+(int)dy,(bits&masks[dx])?0xffff:0x8000);
    }
}

static void diag_beacon(unsigned short color)
{
    unsigned x,y;
    for(y=16;y<20;++y)for(x=296;x<304;++x)diag_pixel((int)x,(int)y,color);
}

static void diag_hex(unsigned at,unsigned digits,unsigned value)
{
    static const char hex[]="0123456789ABCDEF";
    while(digits){--digits;diag_text[at+digits]=hex[value&15];value>>=4;}
}

static void diag_capture_cd(void);
static void fault_label(unsigned row,unsigned col,const char *text)
{
    while(*text && col<RT_FAULT_REPORT_COLS)
        diag_fault_text[row*RT_FAULT_REPORT_COLS+col++]=*text++;
}
static void fault_number(unsigned row,unsigned col,unsigned digits,uint32_t value,unsigned base)
{
    static const char numbers[]="0123456789ABCDEF";
    while(digits){--digits;diag_fault_text[row*RT_FAULT_REPORT_COLS+col+digits]=numbers[value%base];value/=base;}
}
void rt_hw_diag_latch_fault(const rt_fault_report *report)
{
    if(fault_latched || !report || !report->flags)return;
    rt_hw_diag_fault_state.flags=report->flags;
    rt_hw_diag_fault_state.first_flags=report->first_flags;
    rt_hw_diag_fault_state.code=report->code;
    rt_hw_diag_fault_state.in_game=report->in_game;
    rt_hw_diag_fault_state.mode=report->mode;
    rt_hw_diag_fault_state.stage=report->stage;
    rt_hw_diag_fault_state.scene=report->scene;
    rt_hw_diag_fault_state.phase=report->phase;
    rt_hw_diag_fault_state.tick=report->tick;
    rt_hw_diag_fault_state.vdp_free=report->vdp_free;
    for(unsigned i=0;i<sizeof(diag_fault_text);++i)diag_fault_text[i]=' ';
    fault_label(0,0,"SUPER PANG R10R12 STOP");
    fault_label(1,0,"FLAGS 00000000 FIRST 00");
    fault_number(1,6,8,report->flags,16);fault_number(1,21,2,report->first_flags,16);
    fault_label(2,0,"CODE  0000000000");
    diag_fault_text[2*RT_FAULT_REPORT_COLS+5]=report->code<0?'-':' ';
    uint32_t magnitude=report->code<0?0u-(uint32_t)report->code:(uint32_t)report->code;
    fault_number(2,6,10,magnitude,10);
    fault_label(3,0,report->in_game?(report->mode?"OWNER GAME MODE PANIC":"OWNER GAME MODE TOUR"):"OWNER FRONTEND");
    fault_label(4,0,"STAGE -- SCENE 000");
    if(report->in_game)fault_number(4,6,2,report->stage+1,10);
    fault_number(4,15,3,report->scene,10);
    fault_label(5,0,"PHASE 00 TICK 00000000");
    fault_number(5,6,2,report->phase,10);fault_number(5,14,8,report->tick,16);
    fault_label(6,0,"VDP BYTES 000000");fault_number(6,10,6,report->vdp_free,10);
    /* Snapshot before cleanup can issue a new CD command. A terminal stream
     * failure owns its saved command: a prior recoverable preparation error
     * or a later live command must not replace the actual stream cause. */
    diag_capture_cd();
    const jo_audio_music_stream_status *stream=report->code==AUDIO_ERR_STREAM?
        jo_audio_get_music_stream_status():0;
    unsigned cd_error=rt_hw_diag_state.error_latched;
    fault_label(7,0,(stream||cd_error)?"CD ERR 00 STATUS 0000":"CD CMD 00 STATUS 0000");
    fault_number(7,7,2,stream?stream->cd_command:
        cd_error?rt_hw_diag_state.error_command:rt_hw_diag_state.cd_command,16);
    fault_number(7,17,4,stream?stream->cd_status:
        cd_error?rt_hw_diag_state.error_status:rt_hw_diag_state.cd_status,16);
    if(stream){
        fault_label(8,0,"PCM 000 STREAM  000000");
        fault_number(8,4,3,audio_get_stream_command(),10);
        diag_fault_text[8*RT_FAULT_REPORT_COLS+15]=stream->result<0?'-':' ';
        uint32_t result=stream->result<0?0u-(uint32_t)stream->result:(uint32_t)stream->result;
        fault_number(8,16,6,result,10);
        fault_label(9,0,"FAD 000000 POS 00000000");
        fault_number(9,4,6,stream->fad,10);fault_number(9,15,8,stream->cursor,10);
    }else{
        /* Preserve the preparation cause before audio_stop releases its source.
         * -207 alone could previously mean file, header, read or allocation. */
        const jo_audio_music_prepare_status *prep=jo_audio_get_music_prepare_status();
        fault_label(8,0,"PCM 000 STEP 0 TRY 0");
        fault_number(8,4,3,audio_get_prepare_command(),10);
        if(prep){
            fault_number(8,13,1,prep->error,10);fault_number(8,19,1,prep->attempts,10);
            fault_label(9,0,"READ  000000 OF 000000");
            diag_fault_text[9*RT_FAULT_REPORT_COLS+5]=prep->read_result<0?'-':' ';
            uint32_t read=prep->read_result<0?0u-(uint32_t)prep->read_result:(uint32_t)prep->read_result;
            fault_number(9,6,6,read,10);fault_number(9,16,6,prep->requested_bytes,10);
        }
    }
    fault_label(10,0,"PLEASE PHOTO THIS SCREEN");
    fault_label(11,0,"A B C START TO BIOS");
    fault_cursor=0;
    /* Publish only after every character is complete; IRQ never waits on main. */
    __asm__ volatile("" ::: "memory");
    fault_latched=1;
    rt_hw_diag_state.enabled=1;
}

static void diag_capture_cd(void)
{
    const volatile jo_fs_cd_status *s=jo_fs_get_cd_status();
    if(!s)return;
    /* The producer publishes an inactive slot before changing its index.
     * Main cannot resume while this IRQ reads it: no seqlock or retry here. */
    rt_hw_diag_state.cd_commands=s->commands;
    rt_hw_diag_state.cd_waits=s->waits;
    rt_hw_diag_state.cd_rejects=s->rejects;
    rt_hw_diag_state.cd_timeouts=s->timeouts;
    rt_hw_diag_state.cd_phase=s->phase;
    rt_hw_diag_state.cd_command=s->command;
    rt_hw_diag_state.cd_status=s->status;
    rt_hw_diag_state.cd_wait_mask=s->wait_mask;
    rt_hw_diag_state.cd_wait_age=s->phase?rt_hw_diag_state.vblank-s->wait_started:0;
    rt_hw_diag_state.cd_max_wait_vblanks=s->max_wait_vblanks;
    /* Keep the first observed error record once a terminal counter exists.
     * The live phase always takes precedence on screen while waiting. */
    if(!rt_hw_diag_state.error_latched && (s->timeouts || s->rejects)){
        rt_hw_diag_state.error_command=s->last_error_command;
        rt_hw_diag_state.error_status=s->last_error_status;
        rt_hw_diag_state.error_phase=s->last_error_phase;
        rt_hw_diag_state.error_mask=s->last_error_mask;
        rt_hw_diag_state.error_hirq=s->last_error_hirq;
        rt_hw_diag_state.error_vblank=s->last_error_vblank;
        rt_hw_diag_state.error_latched=1;
    }
}

static void diag_snapshot_text(void)
{
    static const char labels[]="I0000 M0000 P0 C00 S00A000 W00 E0 O0000 T00 ";
    unsigned i,status,age;
    int error=rt_hw_diag_state.error_latched && !rt_hw_diag_state.cd_phase;
    for(i=0;i<DIAG_CHARS;++i)diag_text[i]=labels[i];
    diag_hex(1,4,rt_hw_diag_state.irq_started);
    diag_hex(7,4,rt_hw_diag_state.main_completed);
    diag_hex(13,1,error?rt_hw_diag_state.error_phase:rt_hw_diag_state.cd_phase);
    diag_hex(16,2,error?rt_hw_diag_state.error_command:rt_hw_diag_state.cd_command);
    status=error?rt_hw_diag_state.error_status:rt_hw_diag_state.cd_status;
    if(status==0xffff)diag_text[20]=diag_text[21]='-';
    else diag_hex(20,2,status);
    age=rt_hw_diag_state.cd_wait_age;
    diag_hex(23,3,age>0xfff?0xfff:age);
    diag_hex(28,2,error?rt_hw_diag_state.error_mask:rt_hw_diag_state.cd_wait_mask);
    diag_hex(32,1,rt_hw_diag_state.edsr&3);
    diag_hex(35,4,error?rt_hw_diag_state.error_hirq:rt_hw_diag_state.copr);
    diag_hex(41,2,rt_hw_diag_state.cd_timeouts);
    if(error){diag_text[12]='F';diag_text[34]='H';}
}

void rt_hw_diag_init(void)
{
    volatile uint32_t *words=(volatile uint32_t *)&rt_hw_diag_state;
    unsigned i;
    for(i=0;i<sizeof(rt_hw_diag_state)/sizeof(*words);++i)words[i]=0;
    rt_hw_diag_state.cd_status=0xffff;
    diag_z_held=1;diag_drawn=diag_ended=0;
    diag_words_left=diag_cursor=diag_repaint_seen=0;
    fault_latched=fault_cursor=0;
    volatile uint32_t *fault_words=(volatile uint32_t *)&rt_hw_diag_fault_state;
    for(i=0;i<sizeof(rt_hw_diag_fault_state)/sizeof(*fault_words);++i)fault_words[i]=0;
    diag_ready=1;
}

void rt_hw_diag_enable_video(void)
{
    rt_hw_diag_state.video_ready=1;
}

void rt_hw_diag_vblank_begin(void)
{
    if(!diag_ready)return;
    ++rt_hw_diag_state.irq_started;
    rt_hw_diag_state.vblank=jo_core_get_vblank_count();
    diag_drawn=diag_ended=0;diag_words_left=DIAG_WORD_BUDGET;
    if(rt_hw_diag_state.video_ready){
        rt_hw_diag_state.edsr=JO_VDP1_EDSR;
        rt_hw_diag_state.copr=JO_VDP1_COPR;
        rt_hw_diag_state.lopr=JO_VDP1_LOPR;
    }
}

void rt_hw_diag_vblank_draw(void)
{
    unsigned pressed=0,n;
    jo_gamepad_type type;
    if(!diag_ready || diag_drawn)return;
    diag_drawn=1;
    type=jo_get_input_type(0);
    if(type==JoRegularGamepad || type==JoNightsPad)pressed=jo_is_pad1_key_pressed(JO_KEY_Z);
    if(pressed && !diag_z_held && rt_hw_diag_state.video_ready && !fault_latched){
        rt_hw_diag_state.enabled=!rt_hw_diag_state.enabled;
        if(!rt_hw_diag_state.enabled)++rt_hw_diag_state.repaint_generation;
        diag_cursor=0;
    }
    diag_z_held=pressed;
    /* Record terminal CD evidence even when the optional overlay is off. */
    if(rt_hw_diag_state.video_ready)diag_capture_cd();
    if(!rt_hw_diag_state.enabled || !rt_hw_diag_state.video_ready)return;
    diag_beacon((rt_hw_diag_state.vblank&16)?0x83ff:0x81ef);
    if(fault_latched){
        for(n=0;n<2;++n){
            unsigned col=fault_cursor%RT_FAULT_REPORT_COLS,row=fault_cursor/RT_FAULT_REPORT_COLS;
            diag_paint_char((unsigned char)diag_fault_text[fault_cursor],8+(int)col*12,24+(int)row*16);
            if(++fault_cursor==sizeof(diag_fault_text))fault_cursor=0;
        }
        return;
    }
    if(!diag_cursor)diag_snapshot_text();
    for(n=0;n<2;++n){
        unsigned col=diag_cursor,row=0;
        if(col>=DIAG_COLS){col-=DIAG_COLS;row=1;}
        diag_paint_char((unsigned char)diag_text[diag_cursor],
                        DIAG_X+(int)col*12,DIAG_Y+(int)row*16);
        if(++diag_cursor==DIAG_CHARS)diag_cursor=0;
    }
}

void rt_hw_diag_vblank_end(void)
{
    unsigned words;
    if(!diag_ready || diag_ended)return;
    diag_ended=1;
    if(diag_drawn && rt_hw_diag_state.enabled && rt_hw_diag_state.video_ready)
        diag_beacon((rt_hw_diag_state.vblank&16)?0x83e0:0x81e0);
    ++rt_hw_diag_state.irq_completed;
    words=DIAG_WORD_BUDGET-diag_words_left;
    rt_hw_diag_state.words_last=words;
    if(words>rt_hw_diag_state.words_max)rt_hw_diag_state.words_max=words;
}

void rt_hw_diag_frame_begin(void){++rt_hw_diag_state.main_started;}
void rt_hw_diag_frame_end(void){++rt_hw_diag_state.main_completed;}

int rt_hw_diag_repaint_needed(void)
{
    unsigned generation=rt_hw_diag_state.repaint_generation;
    if(generation==diag_repaint_seen)return 0;
    diag_repaint_seen=generation;
    return 1;
}
