/* Independent MODE1/2352 structure, EDC, P/Q parity checker. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t edc_table[256];
static unsigned char forward[256],backward[256];
static void tables(void) {
    for(unsigned i=0;i<256;i++) {
        unsigned j=(i<<1)^((i&128)?0x11d:0);
        forward[i]=(unsigned char)j;backward[i^j]=(unsigned char)i;
        uint32_t e=i;
        for(unsigned n=0;n<8;n++)e=(e>>1)^((e&1)?0xd8018001u:0);
        edc_table[i]=e;
    }
}
static void parity(const unsigned char *data,unsigned major_count,unsigned minor_count,
                   unsigned major_mult,unsigned minor_inc,unsigned char *dest) {
    unsigned size=major_count*minor_count;
    for(unsigned major=0;major<major_count;major++) {
        unsigned index=(major>>1)*major_mult+(major&1);
        unsigned char a=0,b=0;
        for(unsigned minor=0;minor<minor_count;minor++) {
            unsigned char v=data[index];index+=minor_inc;if(index>=size)index-=size;
            a^=v;b^=v;a=forward[a];
        }
        a=backward[forward[a]^b];dest[major]=a;dest[major+major_count]=a^b;
    }
}
static unsigned bcd(unsigned n){return (n/10)*16+n%10;}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    tables();unsigned char s[2352],p[172],q[104];unsigned sectors=0,bad=0;size_t got;
    while((got=fread(s,1,sizeof s,f))) {
        if(got!=sizeof s){fprintf(stderr,"Partial sector\n");fclose(f);return 1;}
        unsigned errors=0,lba=sectors+150;
        errors|=s[0]!=0 || s[11]!=0 || s[15]!=1;
        for(unsigned i=1;i<11;i++)errors|=s[i]!=255;
        errors|=s[12]!=bcd(lba/(60*75)) || s[13]!=bcd((lba/75)%60) || s[14]!=bcd(lba%75);
        uint32_t edc=0;
        for(unsigned i=0;i<2064;i++)edc=(edc>>8)^edc_table[(edc^s[i])&255];
        for(unsigned i=0;i<4;i++)errors|=s[2064+i]!=(unsigned char)(edc>>(8*i));
        for(unsigned i=2068;i<2076;i++)errors|=s[i]!=0;
        parity(s+12,86,24,2,86,p);parity(s+12,52,43,86,88,q);
        errors|=memcmp(p,s+2076,sizeof p)!=0 || memcmp(q,s+2248,sizeof q)!=0;
        if(errors && bad++<8)fprintf(stderr,"Invalid MODE1 sector %u\n",sectors);
        sectors++;
    }
    int io=ferror(f);fclose(f);
    printf("{\"sectors\":%u,\"bad_sectors\":%u,\"io_error\":%d}\n",sectors,bad,io);
    return !sectors || bad || io;
}
