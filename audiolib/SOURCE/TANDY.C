/* Native Tandy/SN76489 PCM backend for murm386.
 *
 * The emulated Tandy device is a real SN76489 PSG, not the parallel-port
 * Tandy Sound Source handled by SNDSRC.C.  Use tone channel 0 with period 1;
 * murm386 holds that channel at a constant level for period <= 1, so changing
 * the 4-bit attenuation register provides a simple 4-bit DAC.
 */
#include <stdint.h>
#include <conio.h>
#include "TASK_MAN.H"
#include "tsm.h"
#include "sound_hw.h"
#include "TANDY.H"

#define TANDY_TIME_DEN 1000000u
#define MONO_8BIT      0

static int tandy_port = 0x0c0;
static int tandy_sample_rate = 8000;

static char *tandy_buffer_start;
static char *tandy_current_buffer;
static char *tandy_sound_ptr;
static int tandy_transfer_length;
static int tandy_current_length;
static int tandy_buffer_num;
static int tandy_num_buffers;
static int tandy_playing;
static task *tandy_timer;
static void (*tandy_callback)(void);
static uint32_t tandy_last_time_us;
static uint32_t tandy_time_fraction;
static int tandy_error = TANDY_Ok;

/* SN76489 attenuation levels used by murm386, scaled to 0..4080.
 * Entry 15 is silence. */
static const uint16_t tandy_levels[16] = {
    0x0ff0, 0x0cb0, 0x0a10, 0x0800,
    0x0650, 0x0500, 0x0400, 0x0330,
    0x0280, 0x0200, 0x0190, 0x0140,
    0x0100, 0x00c0, 0x00a0, 0x0000
};

static unsigned TANDY_Quantize(unsigned char sample)
{
    uint32_t target = ((uint32_t)sample * 0x0ff0u + 127u) / 255u;
    unsigned best = 15;
    uint32_t best_error = 0xffffffffu;
    unsigned i;

    for (i = 0; i < 16; ++i)
    {
        uint32_t level = tandy_levels[i];
        uint32_t error = (level > target) ? (level - target) : (target - level);
        if (error < best_error)
        {
            best_error = error;
            best = i;
        }
    }

    return best;
}

static void TANDY_WriteSample(unsigned char sample)
{
    outp(tandy_port, 0x90u | TANDY_Quantize(sample));
}

static unsigned char TANDY_AdvanceSamples(uint32_t count)
{
    unsigned char sample = 0x80;

    while (count && tandy_playing && tandy_sound_ptr)
    {
        uint32_t step = count;

        if (step > (uint32_t)tandy_current_length)
            step = (uint32_t)tandy_current_length;

        sample = (unsigned char)tandy_sound_ptr[step - 1];
        tandy_sound_ptr += step;
        tandy_current_length -= (int)step;
        count -= step;

        if (tandy_current_length == 0)
        {
            tandy_current_buffer += tandy_transfer_length;
            if (++tandy_buffer_num >= tandy_num_buffers)
            {
                tandy_buffer_num = 0;
                tandy_current_buffer = tandy_buffer_start;
            }

            tandy_sound_ptr = tandy_current_buffer;
            tandy_current_length = tandy_transfer_length;

            if (tandy_callback)
                tandy_callback();
        }
    }

    return sample;
}

static uint32_t TANDY_SamplesDue(uint32_t elapsed)
{
    uint32_t whole_seconds = elapsed / TANDY_TIME_DEN;
    uint32_t remainder_us = elapsed % TANDY_TIME_DEN;
    uint32_t scaled = tandy_time_fraction +
                      remainder_us * (uint32_t)tandy_sample_rate;
    uint32_t due = whole_seconds * (uint32_t)tandy_sample_rate;

    due += scaled / TANDY_TIME_DEN;
    tandy_time_fraction = scaled % TANDY_TIME_DEN;
    return due;
}

static void TANDY_Service(task *Task)
{
    uint32_t now;
    uint32_t elapsed;
    uint32_t due;

    (void)Task;

    if (!tandy_playing || !tandy_sound_ptr)
        return;

    now = TSM_CurrentTime();
    elapsed = now - tandy_last_time_us;
    tandy_last_time_us = now;

    due = TANDY_SamplesDue(elapsed);
    if (due != 0)
        TANDY_WriteSample(TANDY_AdvanceSamples(due));
}

static void TANDY_Silence(void)
{
    /* Tone 0 period = 1. */
    outp(tandy_port, 0x81);
    outp(tandy_port, 0x00);

    /* Mute all four PSG channels. */
    outp(tandy_port, 0x9f);
    outp(tandy_port, 0xbf);
    outp(tandy_port, 0xdf);
    outp(tandy_port, 0xff);
}

void TANDY_SetPort(int port)
{
    tandy_port = port;
}

char *TANDY_ErrorString(int error)
{
    if (error == TANDY_Error)
        error = tandy_error;

    switch (error)
    {
    case TANDY_Ok:      return "Tandy SN76489 ok.";
    case TANDY_Warning: return "Could not detect Tandy SN76489.";
    default:            return "Tandy SN76489 error.";
    }
}

int TANDY_Init(void)
{
    if ((sound_hw_mask() & SOUND_HW_TANDY) == 0)
    {
        tandy_error = TANDY_Warning;
        return TANDY_Warning;
    }

    TANDY_StopPlayback();
    TANDY_Silence();
    tandy_error = TANDY_Ok;
    return TANDY_Ok;
}

void TANDY_Shutdown(void)
{
    TANDY_StopPlayback();
    TANDY_Silence();
}

void TANDY_StopPlayback(void)
{
    if (tandy_timer)
    {
        TS_Terminate(tandy_timer);
        tandy_timer = 0;
    }

    tandy_playing = 0;
    tandy_last_time_us = 0;
    tandy_time_fraction = 0;
    tandy_buffer_start = 0;
    tandy_current_buffer = 0;
    tandy_sound_ptr = 0;

    TANDY_Silence();
}

int TANDY_BeginBufferedPlayback(char *buffer, int size, int divisions,
                                int mixrate, void (*callback)(void))
{
    if (!buffer || size <= 0 || divisions <= 0 || size < divisions || mixrate <= 0)
        return TANDY_Error;

    TANDY_StopPlayback();

    tandy_buffer_start = buffer;
    tandy_current_buffer = buffer;
    tandy_sound_ptr = buffer;
    tandy_transfer_length = size / divisions;
    tandy_current_length = tandy_transfer_length;
    tandy_buffer_num = 0;
    tandy_num_buffers = divisions;
    tandy_callback = callback;
    tandy_sample_rate = mixrate;
    tandy_time_fraction = 0;
    tandy_last_time_us = TSM_YieldTime();

    /* Prepare channel 0 as a constant-level source; channels 1/2/noise stay muted. */
    TANDY_Silence();
    tandy_playing = 1;

    tandy_timer = TS_ScheduleTaskSkipLate(TANDY_Service, tandy_sample_rate, 1, 0);
    if (!tandy_timer)
    {
        tandy_playing = 0;
        TANDY_Silence();
        return TANDY_Error;
    }

    TS_Dispatch();
    return TANDY_Ok;
}

int TANDY_GetCurrentPos(void)
{
    if (!tandy_playing || !tandy_sound_ptr || !tandy_current_buffer)
        return TANDY_Warning;

    return (int)(tandy_sound_ptr - tandy_current_buffer);
}

int TANDY_GetPlaybackRate(void)
{
    return tandy_sample_rate;
}

int TANDY_SetMixMode(int mode)
{
    (void)mode;
    return MONO_8BIT;
}
