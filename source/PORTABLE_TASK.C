/* Cooperative native replacement for Apogee TASK_MAN.
 *
 * The DOS build drove these callbacks from PIT/INT 8. Native EZ applications
 * execute synchronously on emulator core 0, so the runtime TSM service drives
 * them at cooperative yield points instead. */
#include <stdlib.h>
#include "task_man.h"
#include <tsm.h>

#define PORTABLE_TS_SLOTS 16

typedef struct portable_ts_slot
{
    task *ptr;
    int tsm_id;
} portable_ts_slot;

static portable_ts_slot portable_ts_slots[PORTABLE_TS_SLOTS];
static int portable_ts_installed;

volatile int TS_InInterrupt = 0;

static void portable_ts_run(unsigned index)
{
    task *ptr = portable_ts_slots[index].ptr;

    if (!ptr || !ptr->active || !ptr->TaskService)
        return;

    TS_InInterrupt = 1;
    ptr->TaskService(ptr);
    TS_InInterrupt = 0;
}

#define TS_WRAPPER(n) \
    static int portable_ts_service_##n(void) \
    { \
        portable_ts_run(n); \
        return 0; \
    }

TS_WRAPPER(0)
TS_WRAPPER(1)
TS_WRAPPER(2)
TS_WRAPPER(3)
TS_WRAPPER(4)
TS_WRAPPER(5)
TS_WRAPPER(6)
TS_WRAPPER(7)
TS_WRAPPER(8)
TS_WRAPPER(9)
TS_WRAPPER(10)
TS_WRAPPER(11)
TS_WRAPPER(12)
TS_WRAPPER(13)
TS_WRAPPER(14)
TS_WRAPPER(15)

#undef TS_WRAPPER

static int (*const portable_ts_wrappers[PORTABLE_TS_SLOTS])(void) =
{
    portable_ts_service_0,  portable_ts_service_1,
    portable_ts_service_2,  portable_ts_service_3,
    portable_ts_service_4,  portable_ts_service_5,
    portable_ts_service_6,  portable_ts_service_7,
    portable_ts_service_8,  portable_ts_service_9,
    portable_ts_service_10, portable_ts_service_11,
    portable_ts_service_12, portable_ts_service_13,
    portable_ts_service_14, portable_ts_service_15
};

static int portable_ts_find_task(task *ptr)
{
    int i;

    for (i = 0; i < PORTABLE_TS_SLOTS; ++i)
        if (portable_ts_slots[i].ptr == ptr)
            return i;
    return -1;
}

static int portable_ts_find_free(void)
{
    int i;

    for (i = 0; i < PORTABLE_TS_SLOTS; ++i)
        if (!portable_ts_slots[i].ptr)
            return i;
    return -1;
}

static task *portable_ts_schedule(void (*Function)(task *), int rate,
                                  int priority, void *data, int skip_late)
{
    task *ptr;
    int slot;
    int id;

    if (!Function || rate <= 0)
        return NULL;

    if (!portable_ts_installed)
    {
        TSM_Install(rate);
        portable_ts_installed = 1;
    }

    slot = portable_ts_find_free();
    if (slot < 0)
        return NULL;

    ptr = (task *)malloc(sizeof(*ptr));
    if (!ptr)
        return NULL;

    ptr->next = NULL;
    ptr->prev = NULL;
    ptr->TaskService = Function;
    ptr->data = data;
    ptr->rate = rate;
    ptr->count = 0;
    ptr->priority = priority;
    ptr->active = 0;

    portable_ts_slots[slot].ptr = ptr;
    if (skip_late)
        id = TSM_NewServiceSkipLate(portable_ts_wrappers[slot], rate, priority, 1);
    else
        id = TSM_NewService(portable_ts_wrappers[slot], rate, priority, 1);
    if (id < 0)
    {
        portable_ts_slots[slot].ptr = NULL;
        free(ptr);
        return NULL;
    }

    portable_ts_slots[slot].tsm_id = id;
    return ptr;
}

task *TS_ScheduleTask(void (*Function)(task *), int rate,
                      int priority, void *data)
{
    return portable_ts_schedule(Function, rate, priority, data, 0);
}

task *TS_ScheduleTaskSkipLate(void (*Function)(task *), int rate,
                              int priority, void *data)
{
    return portable_ts_schedule(Function, rate, priority, data, 1);
}

int TS_Terminate(task *ptr)
{
    int slot = portable_ts_find_task(ptr);

    if (slot < 0)
        return TASK_Warning;

    TSM_DelService(portable_ts_slots[slot].tsm_id);
    portable_ts_slots[slot].ptr = NULL;
    portable_ts_slots[slot].tsm_id = -1;
    free(ptr);
    return TASK_Ok;
}

void TS_Dispatch(void)
{
    int i;

    for (i = 0; i < PORTABLE_TS_SLOTS; ++i)
    {
        task *ptr = portable_ts_slots[i].ptr;

        if (ptr && !ptr->active)
        {
            ptr->active = 1;
            TSM_ResumeService(portable_ts_slots[i].tsm_id);
        }
    }

    TSM_Yield();
}

void TS_SetTaskRate(task *ptr, int rate)
{
    int slot;
    int id;
    int was_active;

    if (!ptr || rate <= 0)
        return;

    slot = portable_ts_find_task(ptr);
    if (slot < 0)
        return;

    was_active = ptr->active;
    TSM_DelService(portable_ts_slots[slot].tsm_id);
    id = TSM_NewService(portable_ts_wrappers[slot], rate, ptr->priority,
                        was_active ? 0 : 1);
    if (id < 0)
    {
        ptr->active = 0;
        portable_ts_slots[slot].tsm_id = -1;
        return;
    }

    portable_ts_slots[slot].tsm_id = id;
    ptr->rate = rate;
}

void TS_Shutdown(void)
{
    int i;

    for (i = 0; i < PORTABLE_TS_SLOTS; ++i)
    {
        if (portable_ts_slots[i].ptr)
        {
            TSM_DelService(portable_ts_slots[i].tsm_id);
            free(portable_ts_slots[i].ptr);
            portable_ts_slots[i].ptr = NULL;
            portable_ts_slots[i].tsm_id = -1;
        }
    }

    TSM_Remove();
    portable_ts_installed = 0;
    TS_InInterrupt = 0;
}

int TS_LockMemory(void)
{
    return TASK_Ok;
}

void TS_UnlockMemory(void)
{
}
