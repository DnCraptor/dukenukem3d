/* TODO: native backends for AudioLib devices not yet ported. */
#include "standard.h"
#include "awe32.h"
#include "gusmidi.h"
#include "pas16.h"
#include "sndscape.h"

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
