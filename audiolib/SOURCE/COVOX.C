/* Native Covox Speech Thing backend for murm386.
 *
 * Covox is an 8-bit unsigned DAC on LPT2 data port (0x278).  Unlike the
 * Disney Sound Source it has no FIFO, status register, strobe protocol, DMA
 * or IRQ.  MultiVoc owns the ring buffer; this module only clocks bytes out.
 */
#include <stdlib.h>
#include <conio.h>
#include "TASK_MAN.H"
#include "sound_hw.h"
#include "COVOX.H"

// TODO: read it from DUKE3D.CFG
#define CVX_PORT        0x278
/*
 * Native TASK_MAN is cooperative and catches up overdue callbacks.  Running
 * a raw Covox latch at 7 kHz therefore causes a callback storm whenever the
 * game reaches TSM_Yield() less often than every 143 us.  Keep the bring-up
 * backend at 1 kHz for now; unlike DSS there is no hardware FIFO to absorb
 * burst writes.  A full-rate native Covox needs host-side sample queuing.
 */
// TODO: read it from DUKE3D.CFG
#define CVX_SAMPLE_RATE 1000
#define MONO_8BIT       0

static char *cvx_buffer_start;
static char *cvx_current_buffer;
static char *cvx_sound_ptr;
static int cvx_transfer_length;
static int cvx_current_length;
static int cvx_buffer_num;
static int cvx_num_buffers;
static int cvx_playing;
static task *cvx_timer;
static void (*cvx_callback)(void);
static int cvx_error = CVX_Ok;

static void CVX_Service(task *Task)
{
    (void)Task;

    if (!cvx_playing || !cvx_sound_ptr)
        return;

    outp(CVX_PORT, (unsigned char)*cvx_sound_ptr++);

    if (--cvx_current_length == 0)
    {
        cvx_current_buffer += cvx_transfer_length;
        if (++cvx_buffer_num >= cvx_num_buffers)
        {
            cvx_buffer_num = 0;
            cvx_current_buffer = cvx_buffer_start;
        }

        cvx_sound_ptr = cvx_current_buffer;
        cvx_current_length = cvx_transfer_length;

        if (cvx_callback)
            cvx_callback();
    }
}

char *CVX_ErrorString(int error)
{
    if (error == CVX_Error)
        error = cvx_error;

    switch (error)
    {
    case CVX_Ok:      return "Covox ok.";
    case CVX_Warning: return "Could not detect Covox.";
    default:          return "Covox error.";
    }
}

int CVX_Init(void)
{
    if ((sound_hw_mask() & SOUND_HW_COVOX) == 0)
    {
        cvx_error = CVX_Warning;
        return CVX_Warning;
    }

    CVX_StopPlayback();
    outp(CVX_PORT, 0x80);
    cvx_error = CVX_Ok;
    return CVX_Ok;
}

void CVX_Shutdown(void)
{
    CVX_StopPlayback();
    outp(CVX_PORT, 0x80);
}

void CVX_StopPlayback(void)
{
    if (cvx_timer)
    {
        TS_Terminate(cvx_timer);
        cvx_timer = NULL;
    }

    cvx_playing = 0;
    cvx_buffer_start = NULL;
    cvx_current_buffer = NULL;
    cvx_sound_ptr = NULL;
}

int CVX_BeginBufferedPlayback(char *buffer, int size, int divisions,
                              void (*callback)(void))
{
    if (!buffer || size <= 0 || divisions <= 0 || size < divisions)
        return CVX_Error;

    CVX_StopPlayback();

    cvx_buffer_start = buffer;
    cvx_current_buffer = buffer;
    cvx_sound_ptr = buffer;
    cvx_transfer_length = size / divisions;
    cvx_current_length = cvx_transfer_length;
    cvx_buffer_num = 0;
    cvx_num_buffers = divisions;
    cvx_callback = callback;
    cvx_playing = 1;

    cvx_timer = TS_ScheduleTask(CVX_Service, CVX_SAMPLE_RATE, 1, NULL);
    if (!cvx_timer)
    {
        cvx_playing = 0;
        return CVX_Error;
    }

    TS_Dispatch();
    return CVX_Ok;
}

int CVX_GetCurrentPos(void)
{
    if (!cvx_playing || !cvx_sound_ptr || !cvx_current_buffer)
        return CVX_Warning;

    return (int)(cvx_sound_ptr - cvx_current_buffer);
}

int CVX_GetPlaybackRate(void)
{
    return CVX_SAMPLE_RATE;
}

int CVX_SetMixMode(int mode)
{
    (void)mode;
    return MONO_8BIT;
}
