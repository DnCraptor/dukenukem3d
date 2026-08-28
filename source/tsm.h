#ifndef __NATIVE_DOS_TSM_H__
#define __NATIVE_DOS_TSM_H__

/*
 * Application-space reimplementation of the timer-service manager.
 *
 * The DOS API no longer provides TSM (it was removed in API v20/21 in favour
 * of the raw tsr_callback.h hooks).  This header keeps the exact TSM surface
 * the port already consumes; the implementation in PORTABLE_TSM.C now drives
 * the services from the TSR0 hardware timer callback (core0) instead of from
 * cooperative TSM_Yield points.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     TSM_Install(int rate);
int      TSM_NewService(int (*service)(void), int rate, int priority, int pause);
int      TSM_NewServiceSkipLate(int (*service)(void), int rate, int priority, int pause);
void     TSM_DelService(int id);
void     TSM_PauseService(int id);
void     TSM_ResumeService(int id);
void     TSM_Remove(void);
void     TSM_Yield(void);
uint32_t TSM_YieldTime(void);
uint32_t TSM_CurrentTime(void);

#ifdef __cplusplus
}
#endif

#endif /* __NATIVE_DOS_TSM_H__ */
