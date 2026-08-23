/* Native Covox Speech Thing backend for murm386.
 *
 * Covox is an 8-bit unsigned DAC on LPT2 data port (0x278).  Unlike the
 * Disney Sound Source it has no FIFO, status register, strobe protocol, DMA
 * or IRQ.  MultiVoc owns the ring buffer; this module only clocks bytes out.
 */
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>
#include "TASK_MAN.H"
#include "tsm.h"
#include "sound_hw.h"
#include "COVOX.H"

static int cvx_port = 0x278;
/* Covox is a latch, not a queued device.  Native TASK_MAN is cooperative,
 * so missed sample deadlines must be skipped rather than replayed in a burst.
 * The logical MultiVoc position follows elapsed emulator time; each actual
 * service point writes only the last sample that is current at that instant.
 */
#define CVX_TIME_DEN    1000000u
#define MONO_8BIT       0

static int cvx_sample_rate = 16000;

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
static uint32_t cvx_last_time_us;
static uint32_t cvx_time_fraction;
static unsigned char cvx_last_sample = 0x80;
static int cvx_error = CVX_Ok;

static unsigned char CVX_AdvanceSamples(uint32_t count)
{
    unsigned char sample = 0x80;

    while (count && cvx_playing && cvx_sound_ptr)
    {
        uint32_t step = count;

        if (step > (uint32_t)cvx_current_length)
            step = (uint32_t)cvx_current_length;

        /* Only the final sample of this skipped span can still be audible. */
        sample = (unsigned char)cvx_sound_ptr[step - 1];
        cvx_sound_ptr += step;
        cvx_current_length -= (int)step;
        count -= step;

        if (cvx_current_length == 0)
        {
            cvx_current_buffer += cvx_transfer_length;
            if (++cvx_buffer_num >= cvx_num_buffers)
            {
                cvx_buffer_num = 0;
                cvx_current_buffer = cvx_buffer_start;
            }

            cvx_sound_ptr = cvx_current_buffer;
            cvx_current_length = cvx_transfer_length;

            /* MultiVoc still has to advance/refill every consumed page even
             * though samples inside a late page are not written to Covox. */
            if (cvx_callback)
                cvx_callback();
        }
    }

    return sample;
}

static uint32_t CVX_SamplesDue(uint32_t elapsed)
{
    uint32_t whole_seconds = elapsed / CVX_TIME_DEN;
    uint32_t remainder_us = elapsed % CVX_TIME_DEN;
    uint32_t scaled = cvx_time_fraction +
                      remainder_us * (uint32_t)cvx_sample_rate;
    uint32_t due = whole_seconds * (uint32_t)cvx_sample_rate;

    due += scaled / CVX_TIME_DEN;
    cvx_time_fraction = scaled % CVX_TIME_DEN;
    return due;
}

static void CVX_Service(task *Task)
{
    uint32_t now;
    uint32_t elapsed;
    uint32_t due;

    (void)Task;

    if (!cvx_playing || !cvx_sound_ptr)
        return;

    /* TASK_MAN invokes this from a TSM dispatch.  Use the timestamp already
     * captured by that dispatch; never yield recursively from an audio
     * service callback. */
    now = TSM_CurrentTime();
    elapsed = now - cvx_last_time_us;
    cvx_last_time_us = now;

    due = CVX_SamplesDue(elapsed);
    if (due != 0)
        cvx_last_sample = CVX_AdvanceSamples(due);

    /* Covox is a latch.  Every real service call refreshes it once.
     * If no newer sample exists yet, repeat the last current value. */
    outp(cvx_port, cvx_last_sample);
}

void CVX_SetPort(int port)
{
    cvx_port = port;
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
    outp(cvx_port, 0x80);
    cvx_error = CVX_Ok;
    return CVX_Ok;
}

void CVX_Shutdown(void)
{
    CVX_StopPlayback();
    outp(cvx_port, 0x80);
}

void CVX_StopPlayback(void)
{
    if (cvx_timer)
    {
        TS_Terminate(cvx_timer);
        cvx_timer = NULL;
    }

    cvx_playing = 0;
    cvx_last_time_us = 0;
    cvx_time_fraction = 0;
    cvx_last_sample = 0x80;
    cvx_buffer_start = NULL;
    cvx_current_buffer = NULL;
    cvx_sound_ptr = NULL;
}

int CVX_BeginBufferedPlayback(char *buffer, int size, int divisions, int mixrate,
                              void (*callback)(void))
{
    if (!buffer || size <= 0 || divisions <= 0 || size < divisions || mixrate <= 0)
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
    cvx_sample_rate = mixrate;
    cvx_time_fraction = 0;
    cvx_last_sample = 0x80;
    cvx_last_time_us = TSM_YieldTime();
    cvx_playing = 1;

    cvx_timer = TS_ScheduleTaskSkipLate(CVX_Service, cvx_sample_rate, 1, NULL);
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
    return cvx_sample_rate;
}

int CVX_SetMixMode(int mode)
{
    (void)mode;
    return MONO_8BIT;
}
