/*
 * PORTABLE_TSM.C - application-space timer-service manager.
 *
 * Replaces the TSM that used to live in the DOS API (removed in API v20/21).
 * Services are now dispatched from the TSR0 hardware timer callback, which the
 * runtime raises on core0 from the high-frequency timer path (see
 * tsr_callback.h).  This turns the port's audio/timer services into real,
 * periodic, preemptive work instead of cooperative TSM_Yield dispatch - which
 * is what removes the old burst/underrun behaviour of Covox/DSS/SB.
 *
 * Concurrency note: because services now preempt the foreground from a real
 * core0 IRQ, foreground code that shares state with them must guard it with
 * DisableInterrupts()/RestoreInterrupts() (which PORTABLE_INTERRUPT.C now
 * implements as real PRIMASK cpsid).  The AudioLib already does this around its
 * voice list; game-side Sound[]/SoundOwner bookkeeping should do the same.
 */

#include "tsm.h"
#include "dos_yield.h"
#include "tsr_callback.h"

/* Drains voice-stop callbacks deferred out of the TSR0 IRQ (see MULTIVOC.C). */
extern void MV_RunPendingCallbacks(void);
/* Native, IRQ-safe part of the audio service: clears consumed DMA pages. */
extern void MV_ServiceVocClose(void);

#include <stdint.h>
#include <string.h>

#define NATIVE_TSM_SLOTS 16

typedef struct
{
    int    (*service)(void);
    uint32_t period_us;
    uint32_t next_us;
    volatile unsigned char in_use;
    volatile unsigned char paused;
    volatile unsigned char skip_late;
} tsm_slot_t;

static volatile tsm_slot_t g_slots[NATIVE_TSM_SLOTS];
static volatile int        g_dispatching;
static tsr_callback_t       g_prev_tsr0;

static volatile uint32_t   g_tick;             /* raw TSR0 tick counter          */
static volatile uint32_t   g_now_us;           /* TSR0-driven microsecond clock  */
static volatile uint32_t   g_frac_q16;         /* fractional-us accumulator (Q16)*/
static uint32_t            g_us_per_tick_q16;  /* calibrated tick period (Q16 us)*/
static int                 g_installed;

/* ---- real interrupt mask: blocks the core0 TSR0 IRQ (ARMv6-M/v8-M) ---- */
static inline uint32_t tsm_irq_save(void)
{
    uint32_t pm;
    __asm volatile ("mrs %0, primask\n\tcpsid i" : "=r"(pm) :: "memory");
    return pm;
}
static inline void tsm_irq_restore(uint32_t pm)
{
    __asm volatile ("msr primask, %0" :: "r"(pm) : "memory");
}

/* ---- Cooperative dispatch (runs from TSM_Yield, in application context).
   The mixer touches voice fields that the foreground rewrites live during
   combat (MV_SetPitch / MV_Pan3D / MV_SetVoiceVolume, none of them masked),
   so it must NOT run from the TSR0 IRQ or it reads half-updated voice state
   and faults core0.  Dispatching here keeps it sequential with those edits. */
static void tsm_dispatch(void)
{
    uint32_t now;
    int id;

    if (g_dispatching)
        return;
    now = g_now_us;
    g_dispatching = 1;
    for (id = 0; id < NATIVE_TSM_SLOTS; ++id)
    {
        volatile tsm_slot_t *s = &g_slots[id];
        unsigned catchup = 0;

        if (!s->in_use || s->paused || !s->service)
            continue;

        if (s->skip_late)
        {
            if ((int32_t)(now - s->next_us) >= 0)
            {
                uint32_t late    = now - s->next_us;
                uint32_t periods = late / s->period_us + 1u;
                s->next_us += periods * s->period_us;
                s->service();
            }
            continue;
        }

        while ((int32_t)(now - s->next_us) >= 0)
        {
            s->next_us += s->period_us;
            s->service();
            if (++catchup == 256)
            {
                if ((int32_t)(now - s->next_us) >= 0)
                    s->next_us = now + s->period_us;
                break;
            }
        }
    }
    g_dispatching = 0;
}

/* ---- TSR0 callback: advance the microsecond clock ONLY, then chain.
   No service dispatch here (see tsm_dispatch above): the clock stays reliable
   without the app yielding, but heavy/racy mixing runs cooperatively. ---- */
static void tsm_tsr0(void)
{
    g_tick++;
    g_frac_q16 += g_us_per_tick_q16;
    g_now_us   += (g_frac_q16 >> 16);
    g_frac_q16 &= 0xFFFFu;

    /* Native page-close keeps the DMA from replaying stale audio even while
       the foreground is blocked (disk I/O) and the cooperative mixer stalls. */
    MV_ServiceVocClose();

    if (g_prev_tsr0)
        g_prev_tsr0();
}

/* TSR0 runs at a fixed 44100 Hz on core0 (hardware repeating timer in the
   emulator).  Hardcode the tick period instead of calibrating: a mis-measured
   rate makes the cooperative dispatch fire the mixer at the wrong cadence and
   the DMA replays stale pages (audible echo).  1e6*2^16 / 44100 us/tick. */
#define TSM_TSR0_HZ 44100u

static void tsm_set_rate(void)
{
    g_us_per_tick_q16 =
        (uint32_t)(((uint64_t)1000000u << 16) / (uint64_t)TSM_TSR0_HZ);
    if (g_us_per_tick_q16 == 0)
        g_us_per_tick_q16 = 1;
}

void TSM_Install(int rate)
{
    (void)rate;
    memset((void *)g_slots, 0, sizeof(g_slots));
    g_dispatching = 0;

    if (!g_installed)
    {
        g_tick = 0;
        g_now_us = 0;
        g_frac_q16 = 0;
        tsm_set_rate();
        g_prev_tsr0 = set_tsr0_callback(tsm_tsr0);
        g_installed = 1;
    }
}

static int tsm_new_service_common(int (*service)(void), int rate, int priority,
                                  int pause, int skip_late)
{
    int id;
    uint32_t pm;

    (void)priority;
    if (!service || rate <= 0 || rate > 1000000)
        return -1;
    if (!g_installed)
        TSM_Install(rate);

    pm = tsm_irq_save();
    for (id = 0; id < NATIVE_TSM_SLOTS; ++id)
        if (!g_slots[id].in_use)
            break;
    if (id == NATIVE_TSM_SLOTS)
    {
        tsm_irq_restore(pm);
        return -1;
    }
    g_slots[id].service   = service;
    g_slots[id].period_us = 1000000u / (uint32_t)rate;
    if (g_slots[id].period_us == 0)
        g_slots[id].period_us = 1;
    g_slots[id].next_us   = g_now_us + g_slots[id].period_us;
    g_slots[id].paused    = pause ? 1 : 0;
    g_slots[id].skip_late = skip_late ? 1 : 0;
    g_slots[id].in_use    = 1;
    tsm_irq_restore(pm);
    return id;
}

int TSM_NewService(int (*service)(void), int rate, int priority, int pause)
{
    return tsm_new_service_common(service, rate, priority, pause, 0);
}

int TSM_NewServiceSkipLate(int (*service)(void), int rate, int priority, int pause)
{
    return tsm_new_service_common(service, rate, priority, pause, 1);
}

void TSM_DelService(int id)
{
    uint32_t pm;
    if (id < 0 || id >= NATIVE_TSM_SLOTS)
        return;
    pm = tsm_irq_save();
    memset((void *)&g_slots[id], 0, sizeof(g_slots[id]));
    tsm_irq_restore(pm);
}

void TSM_PauseService(int id)
{
    if (id >= 0 && id < NATIVE_TSM_SLOTS && g_slots[id].in_use)
        g_slots[id].paused = 1;
}

void TSM_ResumeService(int id)
{
    uint32_t pm;
    if (id < 0 || id >= NATIVE_TSM_SLOTS)
        return;
    pm = tsm_irq_save();
    if (g_slots[id].in_use)
    {
        g_slots[id].paused  = 0;
        g_slots[id].next_us = g_now_us + g_slots[id].period_us;
    }
    tsm_irq_restore(pm);
}

void TSM_Remove(void)
{
    uint32_t pm = tsm_irq_save();
    memset((void *)g_slots, 0, sizeof(g_slots));
    g_dispatching = 0;
    tsm_irq_restore(pm);
    /* TSR0 hook stays installed; g_prev_tsr0 keeps the emulated PIT/IRQ0 path. */
}

/*
 * TSM_Yield no longer dispatches services (the TSR0 IRQ does).  It remains a
 * cooperative service point for the emulator and returns the microsecond clock.
 */
void TSM_Yield(void)
{
    tsm_dispatch();
    MV_RunPendingCallbacks();
    (void)dos_yield();
}

uint32_t TSM_YieldTime(void)
{
    return dos_yield();
}

uint32_t TSM_CurrentTime(void)
{
    return g_now_us;
}
