/* TODO: native backends for AudioLib devices not yet ported. */
#include "standard.h"
#include "awe32.h"
#include "gusmidi.h"
#include "pas16.h"
#include "sndscape.h"
#include "sndsrc.h"
#include "guswave.h"

char *AWE32_ErrorString(int e) { (void)e; return "AWE32 is not supported yet."; }
int AWE32_Init(void) { return AWE32_Error; }
void AWE32_Shutdown(void) { }
void AWE32_NoteOff(int c,int k,int v){(void)c;(void)k;(void)v;}
void AWE32_NoteOn(int c,int k,int v){(void)c;(void)k;(void)v;}
void AWE32_PolyAftertouch(int c,int k,int p){(void)c;(void)k;(void)p;}
void AWE32_ChannelAftertouch(int c,int p){(void)c;(void)p;}
void AWE32_ControlChange(int c,int n,int v){(void)c;(void)n;(void)v;}
void AWE32_ProgramChange(int c,int p){(void)c;(void)p;}
void AWE32_PitchBend(int c,int l,int m){(void)c;(void)l;(void)m;}

char *GUS_ErrorString(int e) { (void)e; return "Gravis UltraSound is not supported yet."; }
int GUSMIDI_Init(void) { return GUS_Error; }
void GUSMIDI_Shutdown(void) { }
int GUSMIDI_LoadPatch(int prog) { (void)prog; return GUS_Error; }
void GUSMIDI_ReleasePatches(void) { }
void GUSMIDI_NoteOff(int c,int n,int v){(void)c;(void)n;(void)v;}
void GUSMIDI_NoteOn(int c,int n,int v){(void)c;(void)n;(void)v;}
void GUSMIDI_ControlChange(int c,int n,int v){(void)c;(void)n;(void)v;}
void GUSMIDI_ProgramChange(int c,int p){(void)c;(void)p;}
void GUSMIDI_PitchBend(int c,int l,int m){(void)c;(void)l;(void)m;}
void GUSMIDI_SetVolume(int v){(void)v;}
int GUSMIDI_GetVolume(void){return 0;}

unsigned int PAS_DMAChannel;
char *PAS_ErrorString(int e) { (void)e; return "Pro Audio Spectrum is not supported yet."; }
int PAS_SaveMusicVolume(void) { return PAS_Error; }
void PAS_RestoreMusicVolume(void) { }
void PAS_SetFMVolume(int v) { (void)v; }
int PAS_GetFMVolume(void) { return PAS_Error; }

int SOUNDSCAPE_DMAChannel;
int SOUNDSCAPE_ErrorCode = SOUNDSCAPE_HardwareError;
char *SOUNDSCAPE_ErrorString(int e) { (void)e; return "SoundScape is not supported yet."; }
int SOUNDSCAPE_GetMIDIPort(void) { return SOUNDSCAPE_Error; }


/* TODO: digital Pro Audio Spectrum backend. */
void PAS_SetPlaybackRate(unsigned rate) { (void)rate; }
unsigned PAS_GetPlaybackRate(void) { return 0; }
int PAS_SetMixMode(int mode) { (void)mode; return PAS_Error; }
void PAS_StopPlayback(void) { }
int PAS_GetCurrentPos(void) { return 0; }
int PAS_BeginBufferedPlayback(char *buffer, int size, int divisions,
    unsigned rate, int mode, void (*callback)(void))
{
    (void)buffer; (void)size; (void)divisions; (void)rate;
    (void)mode; (void)callback;
    return PAS_Error;
}
int PAS_BeginBufferedRecord(char *buffer, int size, int divisions,
    unsigned rate, int mode, void (*callback)(void))
{
    (void)buffer; (void)size; (void)divisions; (void)rate;
    (void)mode; (void)callback;
    return PAS_Error;
}
int PAS_SetPCMVolume(int volume) { (void)volume; return PAS_Error; }
int PAS_GetPCMVolume(void) { return PAS_Error; }
int PAS_GetCardInfo(int *bits, int *channels)
{
    if (bits) *bits = 0;
    if (channels) *channels = 0;
    return PAS_Error;
}
void PAS_SetCallBack(void (*func)(void)) { (void)func; }
int PAS_Init(void) { return PAS_CardNotFound; }
void PAS_Shutdown(void) { }
void PAS_UnlockMemory(void) { }
int PAS_LockMemory(void) { return PAS_Error; }

/* TODO: digital Ensoniq SoundScape backend. */
void SOUNDSCAPE_SetPlaybackRate(unsigned rate) { (void)rate; }
unsigned SOUNDSCAPE_GetPlaybackRate(void) { return 0; }
int SOUNDSCAPE_SetMixMode(int mode) { (void)mode; return SOUNDSCAPE_Error; }
void SOUNDSCAPE_StopPlayback(void) { }
int SOUNDSCAPE_GetCurrentPos(void) { return 0; }
int SOUNDSCAPE_BeginBufferedPlayback(char *buffer, int size, int divisions,
    unsigned rate, int mode, void (*callback)(void))
{
    (void)buffer; (void)size; (void)divisions; (void)rate;
    (void)mode; (void)callback;
    return SOUNDSCAPE_Error;
}
int SOUNDSCAPE_GetCardInfo(int *bits, int *channels)
{
    if (bits) *bits = 0;
    if (channels) *channels = 0;
    return SOUNDSCAPE_Error;
}
void SOUNDSCAPE_SetCallBack(void (*func)(void)) { (void)func; }
int SOUNDSCAPE_Init(void)
{
    SOUNDSCAPE_ErrorCode = SOUNDSCAPE_HardwareError;
    return SOUNDSCAPE_Error;
}
void SOUNDSCAPE_Shutdown(void) { }

/* TODO: Disney/Tandy Sound Source backend. */
char *SS_ErrorString(int e) { (void)e; return "Sound Source is not supported yet."; }
void SS_StopPlayback(void) { }
int SS_GetCurrentPos(void) { return 0; }
int SS_BeginBufferedPlayback(char *buffer, int size, int divisions,
    void (*callback)(void))
{
    (void)buffer; (void)size; (void)divisions; (void)callback;
    return SS_Error;
}
int SS_GetPlaybackRate(void) { return 0; }
int SS_SetMixMode(int mode) { (void)mode; return SS_Error; }
int SS_SetPort(int port) { (void)port; return SS_Error; }
void SS_SetCallBack(void (*func)(void)) { (void)func; }
int SS_Init(int soundcard) { (void)soundcard; return SS_NotFound; }
void SS_Shutdown(void) { }
void SS_UnlockMemory(void) { }
int SS_LockMemory(void) { return SS_Error; }

/* TODO: Gravis UltraSound digital-wave backend. */
char *GUSWAVE_ErrorString(int e) { (void)e; return "GUS digital audio is not supported yet."; }
int GUSWAVE_VoicePlaying(int handle) { (void)handle; return 0; }
int GUSWAVE_VoicesPlaying(void) { return 0; }
int GUSWAVE_Kill(int handle) { (void)handle; return GUSWAVE_Error; }
int GUSWAVE_KillAllVoices(void) { return GUSWAVE_Ok; }
int GUSWAVE_SetPitch(int handle, int pitch) { (void)handle; (void)pitch; return GUSWAVE_Error; }
int GUSWAVE_SetPan3D(int handle, int angle, int distance)
{ (void)handle; (void)angle; (void)distance; return GUSWAVE_Error; }
void GUSWAVE_SetVolume(int volume) { (void)volume; }
int GUSWAVE_GetVolume(void) { return 0; }
int GUSWAVE_VoiceAvailable(int priority) { (void)priority; return 0; }
int GUSWAVE_PlayVOC(char *sample, int pitch, int angle, int volume,
    int priority, unsigned long callbackval)
{
    (void)sample; (void)pitch; (void)angle; (void)volume;
    (void)priority; (void)callbackval;
    return GUSWAVE_Error;
}
int GUSWAVE_PlayWAV(char *sample, int pitch, int angle, int volume,
    int priority, unsigned long callbackval)
{
    (void)sample; (void)pitch; (void)angle; (void)volume;
    (void)priority; (void)callbackval;
    return GUSWAVE_Error;
}
int GUSWAVE_StartDemandFeedPlayback(
    void (*function)(char **ptr, unsigned long *length),
    int channels, int bits, int rate, int pitch, int angle,
    int volume, int priority, unsigned long callbackval)
{
    (void)function; (void)channels; (void)bits; (void)rate;
    (void)pitch; (void)angle; (void)volume; (void)priority;
    (void)callbackval;
    return GUSWAVE_Error;
}
void GUSWAVE_SetCallBack(void (*function)(unsigned long)) { (void)function; }
void GUSWAVE_SetReverseStereo(int setting) { (void)setting; }
int GUSWAVE_GetReverseStereo(void) { return 0; }
int GUSWAVE_Init(int numvoices) { (void)numvoices; return GUSWAVE_NotInstalled; }
void GUSWAVE_Shutdown(void) { }
