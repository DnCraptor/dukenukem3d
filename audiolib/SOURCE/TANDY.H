#ifndef __TANDY_H
#define __TANDY_H

#define TANDY_Ok       0
#define TANDY_Warning -1
#define TANDY_Error   -2

char *TANDY_ErrorString(int error);
int   TANDY_Init(void);
void  TANDY_SetPort(int port);
void  TANDY_Shutdown(void);
void  TANDY_StopPlayback(void);
int   TANDY_BeginBufferedPlayback(char *buffer, int size, int divisions,
                                  int mixrate, void (*callback)(void));
int   TANDY_GetCurrentPos(void);
int   TANDY_GetPlaybackRate(void);
int   TANDY_SetMixMode(int mode);

#endif
