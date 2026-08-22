#include <stdint.h>
#include <conio.h>
#include "BLASTER.H"
#include "_BLASTER.H"
#include "DMA.H"
#include "tsm.h"
extern int BLASTER_ErrorCode;
static unsigned sample_rate=BLASTER_DefaultSampleRate; static int mix_mode=MONO_8BIT; static int version; static int playing; static int service_id=-1; static void (*callback)(void);
static char *play_buffer; static int play_length; static int play_divisions; static int play_block; static int last_dma_block=-1;
static int packet_size(void){return (mix_mode&STEREO?2:1)*(mix_mode&SIXTEEN_BIT?2:1);}

/* Drive MultiVoc from the actual emulated 8237 position.  The previous port
   called the page callback from a wall-clock timer, so a delayed/coalesced
   TSM_Yield could advance the mixer independently of DMA. */
static int service(void)
{
    char *pos;
    int offset;
    int block;
    int steps;

    if(!playing || !callback || !play_buffer || play_block <= 0 || play_divisions <= 0)
        return 0;

    pos = DMA_GetCurrentPos(BLASTER_DMAChannel);
    if(!pos)
        return 0;

    offset = (int)(pos - play_buffer);
    if(offset < 0 || offset >= play_length)
        return 0;

    block = offset / play_block;
    if(block >= play_divisions)
        block = play_divisions - 1;

    if(last_dma_block < 0)
    {
        last_dma_block = block;
        return 0;
    }

    steps = (block - last_dma_block + play_divisions) % play_divisions;
    if(steps > 0)
    {
        /* MV_ServiceVoc() derives the mix page from the current DMA
           position itself.  When several DMA blocks elapsed between
           cooperative service points, replaying one callback per missed
           block makes every callback observe the same current position
           and remix the same page repeatedly. */
        last_dma_block = block;
        callback();
    }
    return 0;
}
#define BLASTER_IO_POLL_LIMIT 256u
#define BLASTER_IO_YIELD_INTERVAL 32u

int BLASTER_WriteDSP(unsigned v)
{
    unsigned n;

    for (n = 0; n < BLASTER_IO_POLL_LIMIT; ++n)
    {
        if (!(inp(BLASTER_Config.Address + BLASTER_WritePort) & 0x80))
        {
            outp(BLASTER_Config.Address + BLASTER_WritePort, v);
            return BLASTER_Ok;
        }
        if ((n & (BLASTER_IO_YIELD_INTERVAL - 1u)) ==
            (BLASTER_IO_YIELD_INTERVAL - 1u))
            TSM_Yield();
    }

    BLASTER_ErrorCode = BLASTER_CardNotReady;
    return BLASTER_Error;
}

int BLASTER_ReadDSP(void)
{
    unsigned n;

    for (n = 0; n < BLASTER_IO_POLL_LIMIT; ++n)
    {
        if (inp(BLASTER_Config.Address + BLASTER_DataAvailablePort) & 0x80)
            return inp(BLASTER_Config.Address + BLASTER_ReadPort);
        if ((n & (BLASTER_IO_YIELD_INTERVAL - 1u)) ==
            (BLASTER_IO_YIELD_INTERVAL - 1u))
            TSM_Yield();
    }

    BLASTER_ErrorCode = BLASTER_CardNotReady;
    return BLASTER_Error;
}

int BLASTER_ResetDSP(void)
{
    int value;

    outp(BLASTER_Config.Address + BLASTER_ResetPort, 1);
    outp(BLASTER_Config.Address + BLASTER_ResetPort, 0);
    TSM_Yield();

    value = BLASTER_ReadDSP();
    if (value == BLASTER_Ready)
        return BLASTER_Ok;

    BLASTER_ErrorCode = BLASTER_CardNotReady;
    return BLASTER_Error;
}
int BLASTER_GetDSPVersion(void){int a,b;if(BLASTER_WriteDSP(DSP_GetVersion)!=BLASTER_Ok)return BLASTER_Error;a=BLASTER_ReadDSP();b=BLASTER_ReadDSP();if(a<0||b<0)return BLASTER_Error;return (a<<8)|b;}
void BLASTER_SpeakerOn(void){BLASTER_WriteDSP(DSP_SpeakerOn);} void BLASTER_SpeakerOff(void){BLASTER_WriteDSP(DSP_SpeakerOff);}
void BLASTER_SetPlaybackRate(unsigned r){unsigned actual=r;if(mix_mode==MONO_8BIT){unsigned tc;if(actual<4000)actual=4000;if(actual>44100)actual=44100;tc=(unsigned)CalcTimeConstant(actual,1);BLASTER_WriteDSP(DSP_SetTimeConstant);BLASTER_WriteDSP(tc);actual=(unsigned)CalcSamplingRate(tc);}else if(version>=DSP_Version4xx){if(actual<5000)actual=5000;if(actual>44100)actual=44100;BLASTER_WriteDSP(DSP_Set_DA_Rate);BLASTER_WriteDSP(actual>>8);BLASTER_WriteDSP(actual&255);}else{unsigned ps=(unsigned)packet_size(),tc;if(actual<4000)actual=4000;if(actual>44100)actual=44100;tc=(unsigned)CalcTimeConstant(actual,ps);BLASTER_WriteDSP(DSP_SetTimeConstant);BLASTER_WriteDSP(tc);actual=(unsigned)CalcSamplingRate(tc)/ps;}sample_rate=actual;}
unsigned BLASTER_GetPlaybackRate(void){return sample_rate;}
int BLASTER_SetMixMode(int m){int maxm=version>=DSP_Version4xx?STEREO_16BIT:version>=DSP_Version3xx?STEREO_8BIT:MONO_8BIT;if(m<0||m>maxm)m=maxm;mix_mode=m;return m;}
void BLASTER_SetCallBack(void(*f)(void)){callback=f;}
int BLASTER_SetupDMABuffer(char*p,int n,int mode){int ch=(mix_mode&SIXTEEN_BIT)?(int)BLASTER_Config.Dma16:(int)BLASTER_Config.Dma8;if(ch==UNDEFINED){BLASTER_ErrorCode=(mix_mode&SIXTEEN_BIT)?BLASTER_DMA16NotSet:BLASTER_DMANotSet;return BLASTER_Error;}if(DMA_SetupTransfer(ch,p,n,mode)!=DMA_Ok){BLASTER_ErrorCode=BLASTER_DmaError;return BLASTER_Error;}BLASTER_DMAChannel=ch;return BLASTER_Ok;}
int BLASTER_GetCurrentPos(void)
{
    char *pos;
    if(!playing || !play_buffer) return BLASTER_Error;
    pos = DMA_GetCurrentPos(BLASTER_DMAChannel);
    if(!pos) return BLASTER_Error;
    return (int)(pos - play_buffer);
}
static void start_dsp(int block){int count;if(mix_mode==MONO_8BIT){count=block-1;BLASTER_WriteDSP(DSP_SetBlockLength);BLASTER_WriteDSP(count&255);BLASTER_WriteDSP((count>>8)&255);BLASTER_WriteDSP(DSP_8BitAutoInitMode);}else if(version>=DSP_Version4xx){int cmd=(mix_mode&SIXTEEN_BIT)?DSP_16BitDAC:DSP_8BitDAC;int md=(mix_mode&STEREO?DSP_StereoBit:0)|(mix_mode&SIXTEEN_BIT?DSP_SignedBit:0);count=(mix_mode&SIXTEEN_BIT)?block/2-1:block-1;BLASTER_WriteDSP(cmd);BLASTER_WriteDSP(md);BLASTER_WriteDSP(count&255);BLASTER_WriteDSP((count>>8)&255);}else{count=block-1;BLASTER_WriteDSP(DSP_SetBlockLength);BLASTER_WriteDSP(count&255);BLASTER_WriteDSP((count>>8)&255);BLASTER_WriteDSP(DSP_8BitAutoInitMode);} }
int BLASTER_BeginBufferedPlayback(char*p,int n,int div,unsigned rate,int mode,void(*cb)(void))
{
    int block;
    int block_hz;
    int service_hz;

    BLASTER_StopPlayback();
    BLASTER_SetMixMode(mode);
    if(BLASTER_SetupDMABuffer(p,n,DMA_AutoInitRead)!=BLASTER_Ok)return BLASTER_Error;
    BLASTER_SetPlaybackRate(rate);
    callback=cb;
    block=n/div;
    if(block<=0)return BLASTER_Error;

    play_buffer=p;
    play_length=n;
    play_divisions=div;
    play_block=block;
    last_dma_block=0;

    BLASTER_SpeakerOn();
    start_dsp(block);
    playing=1;

    block_hz=(int)(((uint64_t)sample_rate*(unsigned)packet_size()+block/2)/(unsigned)block);
    if(block_hz<1)block_hz=1;
    service_hz=block_hz*2;
    if(service_hz<140)service_hz=140;
    if(service_hz>2000)service_hz=2000;

    service_id=TSM_NewService(service,service_hz,1,0);
    if(service_id<0){BLASTER_StopPlayback();return BLASTER_Error;}
    return BLASTER_Ok;
}
int BLASTER_BeginBufferedRecord(char*p,int n,int d,unsigned r,int m,void(*cb)(void)){(void)p;(void)n;(void)d;(void)r;(void)m;(void)cb;return BLASTER_Error;}
void BLASTER_StopPlayback(void){if(service_id>=0){TSM_DelService(service_id);service_id=-1;}if(playing){BLASTER_WriteDSP((mix_mode&SIXTEEN_BIT)?DSP_Halt16bitTransfer:DSP_Halt8bitTransfer);if(BLASTER_DMAChannel>=0)DMA_EndTransfer(BLASTER_DMAChannel);}playing=0;callback=0;play_buffer=0;play_length=0;play_divisions=0;play_block=0;last_dma_block=-1;}
int BLASTER_GetVoiceVolume(void){int l,r;if(version>=DSP_Version4xx){l=BLASTER_ReadMixer(MIXER_SB16VoiceLeft);r=BLASTER_ReadMixer(MIXER_SB16VoiceRight);return(l+r)/2;}if(version>=DSP_Version3xx){l=BLASTER_ReadMixer(MIXER_SBProVoice);return((l&0xf0)+((l&15)<<4))/2;}BLASTER_ErrorCode=BLASTER_NoMixer;return BLASTER_Error;}
int BLASTER_SetVoiceVolume(int v){if(v<0)v=0;if(v>255)v=255;if(version>=DSP_Version4xx){BLASTER_WriteMixer(MIXER_SB16VoiceLeft,v&0xf8);BLASTER_WriteMixer(MIXER_SB16VoiceRight,v&0xf8);return BLASTER_Ok;}if(version>=DSP_Version3xx){BLASTER_WriteMixer(MIXER_SBProVoice,(v&0xf0)|(v>>4));return BLASTER_Ok;}BLASTER_ErrorCode=BLASTER_NoMixer;return BLASTER_Error;}
int BLASTER_GetCardInfo(int*b,int*c){if(!b||!c)return BLASTER_Error;*b=(version>=DSP_Version4xx)?16:8;*c=(version>=DSP_Version3xx)?2:1;return BLASTER_Ok;}
int BLASTER_Init(void){if(BLASTER_Config.Address==(unsigned)UNDEFINED){BLASTER_ErrorCode=BLASTER_AddrNotSet;return BLASTER_Error;}if(BLASTER_ResetDSP()!=BLASTER_Ok)return BLASTER_Error;version=BLASTER_GetDSPVersion();if(version<0)return BLASTER_Error;BLASTER_DMAChannel=UNDEFINED;playing=0;BLASTER_SetMixMode(MONO_8BIT);BLASTER_SetPlaybackRate(BLASTER_DefaultSampleRate);return BLASTER_Ok;}
void BLASTER_Shutdown(void){BLASTER_StopPlayback();BLASTER_SpeakerOff();BLASTER_ResetDSP();}
void BLASTER_EnableInterrupt(void){} void BLASTER_DisableInterrupt(void){} int BLASTER_LockMemory(void){return BLASTER_Ok;} void BLASTER_UnlockMemory(void){}
int BLASTER_DSP1xx_BeginPlayback(int n){(void)n;return BLASTER_Error;} int BLASTER_DSP2xx_BeginPlayback(int n){start_dsp(n);playing=1;return BLASTER_Ok;} int BLASTER_DSP4xx_BeginPlayback(int n){start_dsp(n);playing=1;return BLASTER_Ok;}
