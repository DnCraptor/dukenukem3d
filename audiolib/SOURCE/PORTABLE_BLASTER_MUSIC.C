/* Native music/mixer subset of BLASTER.C.  Digital DMA playback is ported separately. */
#include <dos.h>
#include <conio.h>
#include <string.h>
#include "dos-api.h"
#include "BLASTER.H"
#include "_BLASTER.H"

BLASTER_CONFIG BLASTER_Config =
    { UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED };
int BLASTER_DMAChannel = UNDEFINED;
int BLASTER_ErrorCode = BLASTER_Ok;

static int blaster_mixer_address = UNDEFINED;
static int blaster_mixer_type = UNDEFINED;
static int blaster_original_midi_left = 255;
static int blaster_original_midi_right = 255;
static int blaster_waveblaster_state = 0x0f;

static int blaster_upper(int c)
{
    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    return c;
}

static int blaster_env_name_eq(const char *entry, const char *name)
{
    while (*name && *entry)
    {
        if (blaster_upper((unsigned char)*entry) !=
            blaster_upper((unsigned char)*name))
            return 0;
        ++entry;
        ++name;
    }
    return *name == 0 && *entry == '=';
}

static const char *blaster_getenv_value(const char *name)
{
    union REGS inregs;
    union REGS outregs;
    unsigned short psp;
    unsigned short envseg;
    const unsigned char *psp_ptr;
    const char *p;

    memset(&inregs, 0, sizeof(inregs));
    memset(&outregs, 0, sizeof(outregs));
    inregs.h.ah = 0x62;
    int386(0x21, &inregs, &outregs);
    if (outregs.x.cflag)
        return NULL;

    psp = outregs.w.bx;
    psp_ptr = (const unsigned char *)dos_guest_far_ptr(psp, 0);
    envseg = (unsigned short)(psp_ptr[0x2c] | ((unsigned short)psp_ptr[0x2d] << 8));
    if (envseg == 0 || envseg == 0xffffu)
        return NULL;

    p = (const char *)dos_guest_far_ptr(envseg, 0);
    while (*p)
    {
        const char *q = p;
        if (blaster_env_name_eq(q, name))
        {
            while (*q != '=')
                ++q;
            return q + 1;
        }
        p += strlen(p) + 1;
    }
    return NULL;
}

static int blaster_is_hex(int c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static unsigned blaster_parse(const char *p, int base)
{
    unsigned value = 0;
    int digit;

    while (*p)
    {
        if (*p >= '0' && *p <= '9')
            digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f')
            digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F')
            digit = *p - 'A' + 10;
        else
            break;
        if (digit >= base)
            break;
        value = value * (unsigned)base + (unsigned)digit;
        ++p;
    }
    return value;
}

char *BLASTER_ErrorString(int error)
{
    if (error == BLASTER_Warning || error == BLASTER_Error)
        error = BLASTER_ErrorCode;
    switch (error)
    {
    case BLASTER_Ok:               return "Sound Blaster ok.";
    case BLASTER_EnvNotFound:      return "BLASTER environment variable not set.";
    case BLASTER_AddrNotSet:       return "Sound Blaster address not set.";
    case BLASTER_DMANotSet:        return "Sound Blaster 8-bit DMA channel not set.";
    case BLASTER_DMA16NotSet:      return "Sound Blaster 16-bit DMA channel not set.";
    case BLASTER_InvalidParameter: return "Invalid parameter in BLASTER environment variable.";
    case BLASTER_CardNotReady:     return "Sound Blaster not responding on selected port.";
    case BLASTER_NoMixer:          return "Sound Blaster mixer not available.";
    default:                       return "Unknown Sound Blaster error.";
    }
}

int BLASTER_GetEnv(BLASTER_CONFIG *config)
{
    const char *p;
    int parameter;

    config->Address = UNDEFINED;
    config->Type = UNDEFINED;
    config->Interrupt = UNDEFINED;
    config->Dma8 = UNDEFINED;
    config->Dma16 = UNDEFINED;
    config->Midi = UNDEFINED;
    config->Emu = UNDEFINED;

    p = blaster_getenv_value("BLASTER");
    if (p == NULL)
    {
        BLASTER_ErrorCode = BLASTER_EnvNotFound;
        return BLASTER_Error;
    }

    while (*p)
    {
        while (*p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;
        parameter = blaster_upper((unsigned char)*p++);
        if (!blaster_is_hex((unsigned char)*p))
        {
            BLASTER_ErrorCode = BLASTER_InvalidParameter;
            return BLASTER_Error;
        }

        switch (parameter)
        {
        case BlasterEnv_Address:    config->Address = blaster_parse(p, 16); break;
        case BlasterEnv_Interrupt:  config->Interrupt = blaster_parse(p, 10); break;
        case BlasterEnv_8bitDma:    config->Dma8 = blaster_parse(p, 10); break;
        case BlasterEnv_Type:       config->Type = blaster_parse(p, 10); break;
        case BlasterEnv_16bitDma:   config->Dma16 = blaster_parse(p, 10); break;
        case BlasterEnv_Midi:       config->Midi = blaster_parse(p, 16); break;
        case BlasterEnv_EmuAddress: config->Emu = blaster_parse(p, 16); break;
        default: break;
        }
        while (blaster_is_hex((unsigned char)*p))
            ++p;
    }
    return BLASTER_Ok;
}

int BLASTER_SetCardSettings(BLASTER_CONFIG config)
{
    BLASTER_Config = config;
    blaster_mixer_address = (int)config.Address;
    blaster_mixer_type = (int)config.Type;
    if (BLASTER_Config.Emu == (unsigned)UNDEFINED)
        BLASTER_Config.Emu = BLASTER_Config.Address + 0x400u;
    return BLASTER_Ok;
}

int BLASTER_GetCardSettings(BLASTER_CONFIG *config)
{
    if (BLASTER_Config.Address == (unsigned)UNDEFINED)
        return BLASTER_Warning;
    *config = BLASTER_Config;
    return BLASTER_Ok;
}

void BLASTER_WriteMixer(int reg, int data)
{
    if (blaster_mixer_address == UNDEFINED)
        return;
    outp(blaster_mixer_address + BLASTER_MixerAddressPort, reg);
    outp(blaster_mixer_address + BLASTER_MixerDataPort, data);
}

int BLASTER_ReadMixer(int reg)
{
    if (blaster_mixer_address == UNDEFINED)
        return 0xff;
    outp(blaster_mixer_address + BLASTER_MixerAddressPort, reg);
    return inp(blaster_mixer_address + BLASTER_MixerDataPort);
}

int BLASTER_CardHasMixer(void)
{
    return blaster_mixer_type == SBPro ||
           blaster_mixer_type == SBPro2 ||
           blaster_mixer_type == SB16;
}

int BLASTER_GetMidiVolume(void)
{
    int left;
    int right;

    switch (blaster_mixer_type)
    {
    case SBPro:
    case SBPro2:
        left = BLASTER_ReadMixer(MIXER_SBProMidi);
        right = (left & 0x0f) << 4;
        left &= 0xf0;
        return (left + right) / 2;
    case SB16:
        left = BLASTER_ReadMixer(MIXER_SB16MidiLeft);
        right = BLASTER_ReadMixer(MIXER_SB16MidiRight);
        return (left + right) / 2;
    default:
        BLASTER_ErrorCode = BLASTER_NoMixer;
        return BLASTER_Error;
    }
}

int BLASTER_SetMidiVolume(int volume)
{
    int data;
    if (volume < 0) volume = 0;
    if (volume > 255) volume = 255;

    switch (blaster_mixer_type)
    {
    case SBPro:
    case SBPro2:
        data = (volume & 0xf0) + (volume >> 4);
        BLASTER_WriteMixer(MIXER_SBProMidi, data);
        return BLASTER_Ok;
    case SB16:
        BLASTER_WriteMixer(MIXER_SB16MidiLeft, volume & 0xf8);
        BLASTER_WriteMixer(MIXER_SB16MidiRight, volume & 0xf8);
        return BLASTER_Ok;
    default:
        BLASTER_ErrorCode = BLASTER_NoMixer;
        return BLASTER_Error;
    }
}

void BLASTER_SaveMidiVolume(void)
{
    switch (blaster_mixer_type)
    {
    case SBPro:
    case SBPro2:
        blaster_original_midi_left = BLASTER_ReadMixer(MIXER_SBProMidi);
        break;
    case SB16:
        blaster_original_midi_left = BLASTER_ReadMixer(MIXER_SB16MidiLeft);
        blaster_original_midi_right = BLASTER_ReadMixer(MIXER_SB16MidiRight);
        break;
    }
}

void BLASTER_RestoreMidiVolume(void)
{
    switch (blaster_mixer_type)
    {
    case SBPro:
    case SBPro2:
        BLASTER_WriteMixer(MIXER_SBProMidi, blaster_original_midi_left);
        break;
    case SB16:
        BLASTER_WriteMixer(MIXER_SB16MidiLeft, blaster_original_midi_left);
        BLASTER_WriteMixer(MIXER_SB16MidiRight, blaster_original_midi_right);
        break;
    }
}

void BLASTER_SetupWaveBlaster(void)
{
    if (blaster_mixer_type == SB16)
    {
        blaster_waveblaster_state = BLASTER_ReadMixer(MIXER_DSP4xxISR_Enable);
        BLASTER_WriteMixer(MIXER_DSP4xxISR_Enable, MIXER_DisableMPU401Interrupts);
    }
}

void BLASTER_ShutdownWaveBlaster(void)
{
    if (blaster_mixer_type == SB16)
        BLASTER_WriteMixer(MIXER_DSP4xxISR_Enable, blaster_waveblaster_state);
}
