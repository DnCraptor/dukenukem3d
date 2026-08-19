#include <stdint.h>
#include <string.h>
#include "MULTIVOC.H"
typedef signed short VOLUME16[256];

extern char  *MV_MixDestination;
extern unsigned long MV_MixPosition;
extern short *MV_LeftVolume;
extern short *MV_RightVolume;
extern int    MV_SampleSize;
extern int    MV_RightChannelOffset;

static int clamp16(int v)
{
    if (v < -32768) return -32768;
    if (v > 32767) return 32767;
    return v;
}

static int clamp8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int volume8(const short *table, unsigned sample)
{
    const signed char *p = (const signed char *)table;
    return p[(sample & 255u) * 2u];
}

static int volume16_from_u8(const short *table, unsigned sample)
{
    return table[sample & 255u];
}

/* Exact decomposition used by MV_MIX16.ASM for signed 16-bit input. */
static int volume16_from_s16(const short *table, int16_t sample)
{
    uint16_t u = ((uint16_t)sample) ^ 0x8000u;
    unsigned lo = u & 255u;
    unsigned hi = u >> 8;
    const signed char *bytes = (const signed char *)table;
    return (int)bytes[lo * 2u + 1u] + (int)table[hi] + 0x80;
}

void ClearBuffer_DW(void *ptr, unsigned data, int length)
{
    uint8_t *p = (uint8_t *)ptr;
    uint32_t v = (uint32_t)data;
    while (length-- > 0)
    {
        memcpy(p, &v, sizeof(v));
        p += sizeof(v);
    }
}

static void mix_u8_to_u8(unsigned long position, unsigned long rate,
    char *start, unsigned long count, int stereo, int source16)
{
    uint8_t *dst = (uint8_t *)MV_MixDestination;
    const uint8_t *src = (const uint8_t *)start;
    unsigned long i;

    count &= ~1UL;

    for (i = 0; i < count; ++i)
    {
        unsigned idx = (unsigned)(position >> 16);
        unsigned sample;
        if (source16)
        {
            int16_t s;
            memcpy(&s, src + idx * 2u, sizeof(s));
            sample = (unsigned)(((int)s >> 8) + 128) & 255u;
        }
        else
            sample = src[idx];

        dst[0] = (uint8_t)clamp8((int)dst[0] + volume8(MV_LeftVolume, sample));
        if (stereo)
            dst[MV_RightChannelOffset] = (uint8_t)clamp8(
                (int)dst[MV_RightChannelOffset] + volume8(MV_RightVolume, sample));

        position += rate;
        dst += MV_SampleSize;
    }
    MV_MixDestination = (char *)dst;
    MV_MixPosition = position;
}

static void mix_u8_to_s16(unsigned long position, unsigned long rate,
    char *start, unsigned long count, int stereo)
{
    uint8_t *dst = (uint8_t *)MV_MixDestination;
    const uint8_t *src = (const uint8_t *)start;
    unsigned long i;

    count &= ~1UL;

    for (i = 0; i < count; ++i)
    {
        unsigned sample = src[position >> 16];
        int16_t d;
        int v;
        memcpy(&d, dst, sizeof(d));
        v = clamp16((int)d + volume16_from_u8(MV_LeftVolume, sample));
        d = (int16_t)v;
        memcpy(dst, &d, sizeof(d));

        if (stereo)
        {
            memcpy(&d, dst + MV_RightChannelOffset, sizeof(d));
            v = clamp16((int)d + volume16_from_u8(MV_RightVolume, sample));
            d = (int16_t)v;
            memcpy(dst + MV_RightChannelOffset, &d, sizeof(d));
        }
        position += rate;
        dst += MV_SampleSize;
    }
    MV_MixDestination = (char *)dst;
    MV_MixPosition = position;
}

static void mix_s16_to_s16(unsigned long position, unsigned long rate,
    char *start, unsigned long count, int stereo)
{
    uint8_t *dst = (uint8_t *)MV_MixDestination;
    const uint8_t *src = (const uint8_t *)start;
    unsigned long i;

    count &= ~1UL;

    for (i = 0; i < count; ++i)
    {
        int16_t s, d;
        int v;
        memcpy(&s, src + (position >> 16) * 2u, sizeof(s));
        memcpy(&d, dst, sizeof(d));
        v = clamp16((int)d + volume16_from_s16(MV_LeftVolume, s));
        d = (int16_t)v;
        memcpy(dst, &d, sizeof(d));

        if (stereo)
        {
            memcpy(&d, dst + MV_RightChannelOffset, sizeof(d));
            v = clamp16((int)d + volume16_from_s16(MV_RightVolume, s));
            d = (int16_t)v;
            memcpy(dst + MV_RightChannelOffset, &d, sizeof(d));
        }
        position += rate;
        dst += MV_SampleSize;
    }
    MV_MixDestination = (char *)dst;
    MV_MixPosition = position;
}

void MV_Mix8BitMono(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_u8_to_u8(p,r,s,n,0,0); }
void MV_Mix8BitStereo(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_u8_to_u8(p,r,s,n,1,0); }
void MV_Mix16BitMono(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_u8_to_s16(p,r,s,n,0); }
void MV_Mix16BitStereo(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_u8_to_s16(p,r,s,n,1); }
void MV_Mix8BitMono16(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_u8_to_u8(p,r,s,n,0,1); }
void MV_Mix8BitStereo16(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_u8_to_u8(p,r,s,n,1,1); }
void MV_Mix16BitMono16(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_s16_to_s16(p,r,s,n,0); }
void MV_Mix16BitStereo16(unsigned long p,unsigned long r,char *s,unsigned long n)
{ mix_s16_to_s16(p,r,s,n,1); }

void MV_8BitReverb(signed char *src, signed char *dest, VOLUME16 *volume, int count)
{
    const signed char *table = (const signed char *)volume;
    while (count-- > 0)
    {
        unsigned s = (uint8_t)*src++;
        *dest++ = (signed char)((uint8_t)table[s * 2u] + 0x80u);
    }
}

void MV_16BitReverb(char *src, char *dest, VOLUME16 *volume, int count)
{
    while (count-- > 0)
    {
        int16_t s, d;
        memcpy(&s, src, sizeof(s));
        d = (int16_t)volume16_from_s16((const short *)volume, s);
        memcpy(dest, &d, sizeof(d));
        src += 2;
        dest += 2;
    }
}

void MV_8BitReverbFast(signed char *src, signed char *dest, int count, int shift)
{
    unsigned bias = 0x80u - (0x80u >> shift);
    while (count-- > 0)
    {
        unsigned v = (uint8_t)*src++;
        unsigned sign = (v ^ 0x80u) >> 7;
        *dest++ = (signed char)((v >> shift) + bias + sign);
    }
}

void MV_16BitReverbFast(char *src, char *dest, int count, int shift)
{
    while (count-- > 0)
    {
        int16_t s;
        memcpy(&s, src, sizeof(s));
        s = (int16_t)(s >> shift);
        memcpy(dest, &s, sizeof(s));
        src += 2;
        dest += 2;
    }
}
