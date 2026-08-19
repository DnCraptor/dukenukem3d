#include <stdint.h>
#include <conio.h>
#include "DMA.H"
#include "dos_mem.h"
#include "dos-api.h"

typedef struct { int valid,width,mask,mode,clear,page,address,length; } dma_port_t;
static const dma_port_t ports[8] = {
 {1,0,0x0a,0x0b,0x0c,0x87,0x00,0x01}, {1,0,0x0a,0x0b,0x0c,0x83,0x02,0x03},
 {0,0,0x0a,0x0b,0x0c,0x81,0x04,0x05}, {1,0,0x0a,0x0b,0x0c,0x82,0x06,0x07},
 {0,1,0xd4,0xd6,0xd8,0x8f,0xc0,0xc2}, {1,1,0xd4,0xd6,0xd8,0x8b,0xc4,0xc6},
 {1,1,0xd4,0xd6,0xd8,0x89,0xc8,0xca}, {1,1,0xd4,0xd6,0xd8,0x8a,0xcc,0xce}
};

/* Keep the programmed byte range.  On murm386 the count register is the
   reliable progress source (the working DOOM backend uses it as well), while
   rereading the current-address latch is not required for playback progress. */
static uint32_t dma_base[8];
static unsigned dma_bytes[8];

int DMA_ErrorCode = DMA_Ok;
char *DMA_ErrorString(int e){ if(e==DMA_Error)e=DMA_ErrorCode; return e==DMA_Ok?"DMA channel ok.":e==DMA_ChannelOutOfRange?"DMA channel out of valid range.":e==DMA_InvalidChannel?"Unsupported DMA channel.":"Unknown DMA error code."; }
int DMA_VerifyChannel(int c){ if(c<0||c>7){DMA_ErrorCode=DMA_ChannelOutOfRange;return DMA_Error;} if(!ports[c].valid){DMA_ErrorCode=DMA_InvalidChannel;return DMA_Error;} DMA_ErrorCode=DMA_Ok;return DMA_Ok; }
int DMA_SetupTransfer(int c,char *p,int len,int mode){ const dma_port_t *d; uint32_t a; unsigned sel,page,lo,hi,n;
 if(DMA_VerifyChannel(c)!=DMA_Ok||len<=0)return DMA_Error; a=dos_ptr_linear(p); if(a==UINT32_MAX)return DMA_Error; d=&ports[c]; sel=(unsigned)c&3u;
 if(d->width){page=(a>>16)&255u;lo=(a>>1)&255u;hi=(a>>9)&255u;n=((unsigned)len+1u)/2u-1u;}else{page=(a>>16)&255u;lo=a&255u;hi=(a>>8)&255u;n=(unsigned)len-1u;}
 outp(d->mask,4u|sel); outp(d->clear,0); outp(d->mode,(mode==DMA_SingleShotRead?0x48u:mode==DMA_SingleShotWrite?0x44u:mode==DMA_AutoInitRead?0x58u:0x54u)|sel);
 outp(d->address,lo);outp(d->address,hi);outp(d->page,page);outp(d->length,n&255u);outp(d->length,(n>>8)&255u);outp(d->mask,sel);
 dma_base[c]=a; dma_bytes[c]=(unsigned)len; return DMA_Ok; }
int DMA_EndTransfer(int c){const dma_port_t*d;unsigned sel;if(DMA_VerifyChannel(c)!=DMA_Ok)return DMA_Error;d=&ports[c];sel=(unsigned)c&3u;outp(d->mask,4u|sel);outp(d->clear,0);dma_bytes[c]=0;return DMA_Ok;}
int DMA_GetTransferCount(int c){const dma_port_t*d;unsigned n;if(DMA_VerifyChannel(c)!=DMA_Ok)return DMA_Error;d=&ports[c];outp(d->clear,0);n=inp(d->length);n|=(unsigned)inp(d->length)<<8;return d->width?(int)((n+1u)*2u):(int)(n+1u);}
char *DMA_GetCurrentPos(int c){ unsigned remaining,pos; if(DMA_VerifyChannel(c)!=DMA_Ok||dma_bytes[c]==0)return 0; remaining=(unsigned)DMA_GetTransferCount(c); if(remaining>dma_bytes[c])remaining=dma_bytes[c]; pos=(dma_bytes[c]-remaining)%dma_bytes[c]; return (char*)dos_guest_linear_ptr(dma_base[c]+pos); }
