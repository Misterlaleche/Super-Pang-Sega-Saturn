#include "player_vertical.h"
#include "spang_player_control.h"
#include "spang_actor_motion.h"

static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put(uint8_t *p,unsigned value){p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8);}
static unsigned bw(const uint8_t *b,unsigned address){return word(b+(address-0x8000));}
static int animation(uint8_t *p,const uint8_t *b,unsigned address)
{
    if(address<0x8000 || address>0xbffb)return -710;
    put(p+6,address);p[8]=b[address-0x8000]&127;return 0;
}
static int pose(uint8_t *p,const uint8_t *b,unsigned table)
{return animation(p,b,bw(b,table+4*p[17]+2*p[33]));}
static int direction(uint8_t *p,const uint8_t *b,int left)
{p[32]=(uint8_t)(left?8:6);p[33]=(uint8_t)left;return pose(p,b,0xae00);}
static unsigned cell(unsigned x,unsigned y){return ((y&255)>>3)*64+((x&511)>>3);}
static unsigned lowadd(unsigned address,int delta)
{return (address&0x700)|((address+delta)&255);}

/* B060/B0B3: the carry-producing interval is asymmetric at the pixel edge:
 * ladderX-playerX is -10..9, so the allowed player range is x-9..x+10. */
static int ladder_entry(uint8_t *p,const uint8_t *l,int down)
{
    for(unsigned i=0;i<8 && l[i*16];++i){
        const uint8_t *a=l+i*16;
        if((unsigned)(uint16_t)(word(a+10)-word(p+10)-10)+20<65536)continue;
        unsigned feet=(uint8_t)(p[13]+16),top=a[13],bottom=a[12];
        if(down){if(feet<top || (feet!=top && feet>=bottom))continue;}
        else if((uint8_t)(p[13]-1)>=bottom || p[13]<top)continue;
        p[94]=a[1];put(p+10,word(a+10));return down && feet==top?2:1;
    }
    return 0;
}
/* B017: a ladder's broad top is support, including when not climbing. */
static int ladder_top(uint8_t *p,const uint8_t *l)
{
    for(unsigned i=0;i<8 && l[i*16];++i){
        const uint8_t *a=l+i*16;
        if((unsigned)(uint16_t)(word(a+10)-word(p+10)-19)+38<65536)continue;
        unsigned feet=(uint8_t)(p[13]+17);if(feet<a[13] || feet-a[13]>=8)continue;
        p[13]=(uint8_t)(a[13]-16);return 1;
    }
    return 0;
}
/* B1E6 checks support without moving Y, and owns the ordinary/ice flag. */
static int ground_floor(uint8_t *p,const uint8_t *g)
{
    int left=p[21]>=128;unsigned at=cell(word(p+10)+(left?-8:7),(uint8_t)(p[13]+16));
    unsigned value=g[at];
    if(left){
        if(!value)value=g[lowadd(at,1)];
        if(!value){value=g[lowadd(at,2)];if(value==253)value=0;}
    }else{
        if(value==253)value=0;
        if(!value)value=g[lowadd(at,-1)];
        if(!value)value=g[lowadd(at,-2)];
    }
    if(!value)return 0;
    p[92]=(uint8_t)(value==252);return 1;
}
/* B136 landing: reject a leading wall whose column extends above the feet,
 * then inspect the remaining two foot cells. Retain the Y fractional byte. */
static int falling_floor(uint8_t *p,const uint8_t *g)
{
    int left=p[33]!=0;unsigned feet=(uint8_t)(p[13]+15);
    unsigned at=cell(word(p+10)+(left?-8:7),feet),value=g[at];
    if(value)for(unsigned i=1;i<=4;++i)if(at<i*64 || g[at-i*64]){value=0;break;}
    if(!value)value=g[lowadd(at,left?1:-1)];
    if(!value)value=g[lowadd(at,left?2:-2)];
    if(!value)return 0;
    p[13]=(uint8_t)((feet&248)-16);return 1;
}
static void fall(uint8_t *p){p[22]=p[23]=0;p[3]=4;}
static void gravity(uint8_t *p)
{
    unsigned speed=(word(p+22)+0x30)&65535;if((speed>>8)>=7)speed=0x700;
    put(p+22,speed);put(p+12,word(p+12)+speed);
}
static void wall_correct(uint8_t *p)
{
    int left=p[21]>=128;unsigned edge=(word(p+10)+(left?-8:7))&65535;
    unsigned delta=((left?~edge:edge)&7)+1;
    if(delta>=3)return;
    put(p+10,word(p+10)+(left?(int)delta:-(int)delta));p[91]=p[92]=0;
}
static void ground_support(uint8_t *p,const uint8_t *g,const uint8_t *l)
{if(!ground_floor(p,g) && !ladder_top(p,l))fall(p);}
/* AF93/AFD5. ADD HL preserves Z from CP03: the tick which charges2->3
 * resets speed to the requested full speed. BIT7 preserves ADD's carry,
 * which feeds SBC and produces the original asymmetric one-unit changes.
 * A conventional signed acceleration/clamp is not byte-equivalent. */
static void ice_walk(uint8_t *p,int left)
{
    unsigned target=left?0xfe00u:0x0200u;
    unsigned charge=(uint8_t)(p[91]+1),v=word(p+20);
    p[91]=(uint8_t)(charge>=3?3:charge);
    if(charge==3)v=target;
    else if(v!=target){
        unsigned sum=v+(left?0xffc0u:0x0040u),carry=sum>>16;
        v=sum&65535;
        if(((v&0x8000)!=0)==left)
            v=v>=target+carry?target:(v-carry)&65535;
    }
    put(p+20,v);spang_actor_step_x(p);
}
/* AF45 coasts before the ordinary A5E9 wall/support checks. Its tail side
 * probe is itself observable at walls, so retain both original probes. */
static int ice_idle(uint8_t *p,const uint8_t *g)
{
    unsigned v=word(p+20);
    if(p[91]<3 || (!(v&0x8000) && v<32) || ((v&0x8000) && v+32>65535)){
        p[91]=p[20]=p[21]=0;return 0;
    }
    put(p+20,(v+(v&0x8000?32:-32))&65535);spang_actor_step_x(p);
    return spang_player_side(p,g)<0?-712:0;
}
static int ground_idle(uint8_t *p,const uint8_t *g,const uint8_t *l)
{
    if(p[92] && ice_idle(p,g))return -712;
    int r=spang_player_side(p,g);if(r<0)return -712;if(r)wall_correct(p);
    ground_support(p,g,l);return 0;
}
static int ground_walk(uint8_t *p,const uint8_t *g,const uint8_t *l,const uint8_t *b,int left,int animate)
{
    if(animate && spang_player_animate(p,b))return -713;
    if(p[92])ice_walk(p,left);
    else if(spang_player_walk(p,left?-1:1))return -711;
    int r=spang_player_side(p,g);if(r<0)return -712;
    if(r){
        /* A66C/A68A: opposite ice momentum resolves the actual wall,
         * rather than starting a step-up pose in the requested direction. */
        if((p[21]>=128)!=left){wall_correct(p);return 0;}
        p[32]=(uint8_t)(left?14:12);return pose(p,b,0xae08);
    }
    ground_support(p,g,l);return 0;
}
static void landing(uint8_t *p,const uint8_t *g,const uint8_t *l)
{if(falling_floor(p,g) || ladder_top(p,l)){p[3]=0;p[20]=p[21]=0;}}
static int climb_pose(uint8_t *p,const uint8_t *b,int delta)
{
    p[93]=(uint8_t)((p[93]+delta)&3);
    return animation(p,b,bw(b,(p[17]?0xae20:0xae18)+2*p[93]));
}
static int can_fire(const spang_weapons *w,const uint8_t *p)
{
    if((p[87]&7)==6){for(unsigned i=0;i<12;++i)if(!w->bullets[i][0])return 1;return 0;}
    unsigned used=(w->harpoons[0][0]!=0)+(w->harpoons[1][0]!=0);
    return (p[87]&7)==2?used<2:used==0;
}
static int input(uint8_t *p,uint8_t held,uint8_t previous,int edge,const uint8_t *b,const spang_weapons *w,uint8_t *sound)
{
    if(edge){if(p[35])*sound=9;else if(can_fire(w,p))p[32]=10;return 0;}
    if(p[3]!=2)return spang_player_input(p,held&~8u,previous&~8u,b);
    if(held&128)p[32]=2;
    else if(held&64)p[32]=4;
    else if(held&32){if(p[32]!=8)return direction(p,b,1);}
    else if(held&16){if(p[32]!=6)return direction(p,b,0);}
    else p[32]=0;
    return 0;
}
static int shooting(uint8_t *p,const uint8_t *b,const uint8_t *b2,spang_weapons *w,uint8_t *sound,int ladder)
{
    if(!p[4]){
        p[4]=2;p[86]=0;int r=ladder?animation(p,b,p[17]?0xaf03:0xae91):pose(p,b,0xae10);
        if(r)return r;
        r=spang_weapons_fire(w,p,b2);if(r<0)return -714;
        if(r)*sound=(uint8_t)((p[87]&7)==6?0:8);
    }else if(!--p[8]){
        p[32]=p[4]=0;
        if(ladder){p[93]=3;return climb_pose(p,b,1);}
        return pose(p,b,0xadf8);
    }
    return 0;
}
int player_vertical_step(uint8_t p[128],uint8_t held,uint8_t previous,int edge,
 const uint8_t g[2048],const uint8_t l[128],const uint8_t b[16384],
 const uint8_t b2[16384],spang_weapons *w,uint8_t *sound)
{
    if(!p || !g || !l || !b || !b2 || !w || !sound || p[17]>1 || p[33]>1 ||
       (p[3]!=0 && p[3]!=2 && p[3]!=4) || p[32]>14 || (p[32]&1))return -715;
    *sound=255;unsigned mode=p[3];
    if(mode==4)gravity(p);
    if((mode==4?p[32]!=10:p[32]<10) && input(p,held,previous,edge,b,w,sound))return -716;
    unsigned state=p[32];
    if(mode==0){
        if(state==0)return ground_idle(p,g,l);
        if(state==2){
            if(!ladder_entry(p,l,0))return ground_idle(p,g,l);
            p[3]=2;p[93]=0;return animation(p,b,bw(b,p[17]?0xae20:0xae18));
        }
        if(state==4){
            int entry=ladder_entry(p,l,1);if(!entry)return ground_idle(p,g,l);
            p[3]=2;
            if(entry==2){p[32]=14;p[13]+=8;return animation(p,b,bw(b,0xae28+2*p[17]));}
            p[32]=4;p[93]=3;return climb_pose(p,b,1);
        }
        if(state==6 || state==8)return ground_walk(p,g,l,b,state==8,1);
        if(state==10){int r=ground_idle(p,g,l);return r?r:shooting(p,b,b2,w,sound,0);}
        if(state==12 || state==14){
            int left=state==14;
            if(held&(left?16:32)){
                int r=direction(p,b,!left);if(r)return r;p[91]=p[92]=0;
                return ground_walk(p,g,l,b,!left,0);
            }
            if(!--p[8]){p[13]-=8;return direction(p,b,left);}
            return 0;
        }
    }else if(mode==2){
        if(!state)return 0;
        if(state==2 || state==4){
            if(p[94]<12 || p[94]>124 || (p[94]&15)!=12 || !l[p[94]-12])return -717;
            if(state==2){
                if(p[13]<l[p[94]+1]){
                    p[20]=p[21]=0;p[32]=12;p[13]=(uint8_t)(l[p[94]+1]-8);
                    return animation(p,b,bw(b,0xae28+2*p[17]));
                }
                put(p+12,word(p+12)-0x180);
                if(!--p[8])return climb_pose(p,b,1);
            }else{
                if((uint8_t)(p[13]+17)>=l[p[94]]){
                    p[20]=p[21]=p[3]=p[32]=0;int r=pose(p,b,0xadf8);if(r)return r;
                    if(!falling_floor(p,g))fall(p);
                    return 0;
                }
                put(p+12,word(p+12)+0x180);
                if(!--p[8])return climb_pose(p,b,-1);
            }
            return 0;
        }
        if(state==6 || state==8){fall(p);return 0;}
        if(state==10)return shooting(p,b,b2,w,sound,1);
        if(state==12){
            if(!--p[8]){p[13]-=8;p[3]=p[4]=p[32]=0;return pose(p,b,0xadf8);}
            return 0;
        }
        if(state==14){
            if(!--p[8]){p[13]+=8;p[32]=0;p[93]=3;return climb_pose(p,b,1);}
            return 0;
        }
    }else{
        if(state==0 || state==4){p[20]=p[21]=0;landing(p,g,l);return 0;}
        if(state==2){
            if(!ladder_entry(p,l,0)){landing(p,g,l);return 0;}
            p[3]=2;p[93]=0;return animation(p,b,bw(b,p[17]?0xae20:0xae18));
        }
        if(state==6 || state==8){
            if(spang_player_animate(p,b))return -713;
            int left=state==8;p[21]=(uint8_t)(left?255:1);
            unsigned x=p[9]|((unsigned)p[10]<<8)|((unsigned)p[11]<<16);
            x=(x+(left?-256:256))&0xffffff;p[9]=(uint8_t)x;put(p+10,x>>8);
            int r=spang_player_side(p,g);if(r<0)return -712;if(r)wall_correct(p);
            landing(p,g,l);return 0;
        }
        if(state==10){landing(p,g,l);return shooting(p,b,b2,w,sound,0);}
    }
    return -718;
}
