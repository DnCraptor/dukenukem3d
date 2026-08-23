#ifndef __COVOX_H
#define __COVOX_H

#define CVX_Ok       0
#define CVX_Warning -1
#define CVX_Error   -2

char *CVX_ErrorString(int error);
int   CVX_Init(void);
void  CVX_SetPort(int port);
void  CVX_Shutdown(void);
void  CVX_StopPlayback(void);
int   CVX_BeginBufferedPlayback(char *buffer, int size, int divisions, int mixrate,
                                void (*callback)(void));
int   CVX_GetCurrentPos(void);
int   CVX_GetPlaybackRate(void);
int   CVX_SetMixMode(int mode);

#endif
