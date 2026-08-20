/* Portable replacements for Watcom #pragma aux helpers from PRAGMAS.H.
 * Keep the 32-bit two's-complement/fixed-point semantics used by Build. */
#include <dos.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <conio.h>
#include "dos_phys.h"

static long sar64_to_long(int64_t v, unsigned shift)
{
    if (shift == 0) return (long)v;
    return (long)(v >> shift);
}

long sqr(long a) { return (long)((uint32_t)a * (uint32_t)a); }
long scale(long a, long b, long c) { return (long)(((int64_t)a * (int64_t)b) / c); }
long mulscale(long a, long b, long shift) { return sar64_to_long((int64_t)a * (int64_t)b, (unsigned)shift); }
long mulscale1(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 1); }
long mulscale2(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 2); }
long mulscale4(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 4); }
long mulscale5(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 5); }
long mulscale6(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 6); }
long mulscale8(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 8); }
long mulscale9(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 9); }
long mulscale10(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 10); }
long mulscale11(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 11); }
long mulscale12(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 12); }
long mulscale13(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 13); }
long mulscale14(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 14); }
long mulscale15(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 15); }
long mulscale16(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 16); }
long mulscale17(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 17); }
long mulscale18(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 18); }
long mulscale19(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 19); }
long mulscale20(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 20); }
long mulscale21(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 21); }
long mulscale23(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 23); }
long mulscale24(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 24); }
long mulscale27(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 27); }
long mulscale28(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 28); }
long mulscale30(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 30); }
long mulscale31(long a, long b) { return sar64_to_long((int64_t)a * (int64_t)b, 31); }
long mulscale32(long a, long b) { return (long)(((int64_t)a * (int64_t)b) >> 32); }

static long dmulscale_n(long a, long b, long c, long d, unsigned shift)
{
    uint64_t u = (uint64_t)((int64_t)a * (int64_t)b);
    u += (uint64_t)((int64_t)c * (int64_t)d);
    return sar64_to_long((int64_t)u, shift);
}
long dmulscale(long a,long b,long c,long d,long shift) { return dmulscale_n(a,b,c,d,(unsigned)shift); }
long dmulscale2(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,2); }
long dmulscale3(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,3); }
long dmulscale6(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,6); }
long dmulscale8(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,8); }
long dmulscale9(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,9); }
long dmulscale10(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,10); }
long dmulscale12(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,12); }
long dmulscale14(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,14); }
long dmulscale15(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,15); }
long dmulscale16(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,16); }
long dmulscale17(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,17); }
long dmulscale18(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,18); }
long dmulscale24(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,24); }
long dmulscale25(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,25); }
long dmulscale28(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,28); }
long dmulscale32(long a,long b,long c,long d) { return dmulscale_n(a,b,c,d,32); }

static long tmulscale_n(long a,long b,long c,long d,long e,long f,unsigned shift)
{
    uint64_t u = (uint64_t)((int64_t)a * (int64_t)b);
    u += (uint64_t)((int64_t)c * (int64_t)d);
    u += (uint64_t)((int64_t)e * (int64_t)f);
    return sar64_to_long((int64_t)u, shift);
}
long tmulscale11(long a,long b,long c,long d,long e,long f) { return tmulscale_n(a,b,c,d,e,f,11); }

long divscale(long a, long b, long shift)
{
    return (long)(((int64_t)a * ((int64_t)1 << (unsigned)shift)) / b);
}
long divscale12(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 12)) / b); }
long divscale14(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 14)) / b); }
long divscale15(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 15)) / b); }
long divscale16(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 16)) / b); }
long divscale17(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 17)) / b); }
long divscale18(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 18)) / b); }
long divscale19(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 19)) / b); }
long divscale20(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 20)) / b); }
long divscale21(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 21)) / b); }
long divscale22(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 22)) / b); }
long divscale24(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 24)) / b); }
long divscale26(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 26)) / b); }
long divscale28(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 28)) / b); }
long divscale30(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 30)) / b); }
long divscale32(long a,long b) { return (long)(((int64_t)a * ((int64_t)1 << 32)) / b); }

long klabs(long a)
{
    uint32_t u = (uint32_t)a;
    if (u & 0x80000000u) u = 0u - u;
    return (long)(int32_t)u;
}
long ksgn(long a) { return (a > 0) - (a < 0); }
long sgn(long a) { return (a > 0) - (a < 0); }
long min(long a,long b) { return a < b ? a : b; }
long max(long a,long b) { return a > b ? a : b; }
long mul5(long a) { return (long)(int32_t)((uint32_t)a * 5u); }

void clearbuf(long dst, long count, long value)
{
    uint32_t *p = (uint32_t *)(uintptr_t)(uint32_t)dst;
    while (count-- > 0) *p++ = (uint32_t)value;
}

void clearbufbyte(long dst, long count, long value)
{
    uint8_t *p = (uint8_t *)(uintptr_t)(uint32_t)dst;
    uint32_t v = (uint32_t)value;

    /* Match the Watcom STOSB/STOSW/STOSD routine exactly.  The third
     * argument is a 32-bit fill pattern, not a memset-style byte value. */
    if (count <= 0) return;

    if (count < 4)
    {
        if (count & 1)
        {
            *p++ = (uint8_t)v;
            --count;
        }
        while (count >= 2)
        {
            p[0] = (uint8_t)v;
            p[1] = (uint8_t)(v >> 8);
            p += 2;
            count -= 2;
        }
        return;
    }

    if ((uintptr_t)p & 1u)
    {
        *p++ = (uint8_t)v;
        --count;
    }
    if ((uintptr_t)p & 2u)
    {
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p += 2;
        count -= 2;
    }

    while (count >= 4)
    {
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p[2] = (uint8_t)(v >> 16);
        p[3] = (uint8_t)(v >> 24);
        p += 4;
        count -= 4;
    }
    if (count & 2)
    {
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p += 2;
    }
    if (count & 1)
        *p = (uint8_t)v;
}

void copybuf(long src, long dst, long count)
{
    const uint32_t *s = (const uint32_t *)(uintptr_t)(uint32_t)src;
    uint32_t *d = (uint32_t *)(uintptr_t)(uint32_t)dst;
    while (count-- > 0) *d++ = *s++;
}

void copybufbyte(long src, long dst, long count)
{
    const uint8_t *s = (const uint8_t *)(uintptr_t)(uint32_t)src;
    uint8_t *d = (uint8_t *)(uintptr_t)(uint32_t)dst;
    while (count-- > 0) *d++ = *s++;
}

void copybufreverse(long src, long dst, long count)
{
    const uint8_t *s = (const uint8_t *)(uintptr_t)(uint32_t)src;
    uint8_t *d = (uint8_t *)(uintptr_t)(uint32_t)dst;
    while (count-- > 0) *d++ = *s--;
}

static int32_t asr16_32(uint32_t v)
{
    uint32_t hi = v >> 16;
    if (v & 0x80000000u) hi |= 0xffff0000u;
    return (int32_t)hi;
}

void qinterpolatedown16(long dst, long count, long value, long add)
{
    int32_t *d = (int32_t *)(uintptr_t)(uint32_t)dst;
    uint32_t v = (uint32_t)value;
    uint32_t inc = (uint32_t)add;
    while (count-- > 0)
    {
        *d++ = asr16_32(v);
        v += inc;
    }
}

void qinterpolatedown16short(long dst, long count, long value, long add)
{
    int16_t *d = (int16_t *)(uintptr_t)(uint32_t)dst;
    uint32_t v = (uint32_t)value;
    uint32_t inc = (uint32_t)add;
    while (count-- > 0)
    {
        *d++ = (int16_t)asr16_32(v);
        v += inc;
    }
}

void swapchar(long a,long b)
{ uint8_t *x=(uint8_t *)(uintptr_t)(uint32_t)a,*y=(uint8_t *)(uintptr_t)(uint32_t)b,t=*x; *x=*y; *y=t; }
void swapshort(long a,long b)
{ uint16_t *x=(uint16_t *)(uintptr_t)(uint32_t)a,*y=(uint16_t *)(uintptr_t)(uint32_t)b,t=*x; *x=*y; *y=t; }
void swaplong(long a,long b)
{ uint32_t *x=(uint32_t *)(uintptr_t)(uint32_t)a,*y=(uint32_t *)(uintptr_t)(uint32_t)b,t=*x; *x=*y; *y=t; }
void swapchar2(long a,long b,long stride)
{
    uint8_t *x=(uint8_t *)(uintptr_t)(uint32_t)a,*y=(uint8_t *)(uintptr_t)(uint32_t)b;
    uint8_t t=x[0]; x[0]=y[0]; y[0]=t;
    t=x[1]; x[1]=y[stride]; y[stride]=t;
}

void koutp(long port,long value) { outp((uint16_t)port,(uint8_t)value); }
void koutpw(long port,long value) { outpw((uint16_t)port,(uint16_t)value); }
long kinp(long port) { return (long)inp((uint16_t)port); }

long readpixel(long p) { return (long)*((uint8_t *)(uintptr_t)(uint32_t)p); }
void drawpixel(long p, long v) { *((uint8_t *)(uintptr_t)(uint32_t)p) = (uint8_t)v; }
void drawpixels(long p, long v) { *((uint16_t *)(uintptr_t)(uint32_t)p) = (uint16_t)v; }
void drawpixelses(long p, long v) { *((uint32_t *)(uintptr_t)(uint32_t)p) = (uint32_t)v; }
void printchrasm(long dst, long count, long value)
{
    uint16_t *d = (uint16_t *)(uintptr_t)(uint32_t)dst;
    while (count-- > 0) *d++ = (uint16_t)value;
}


/* Planar VGA helpers from the original Watcom PRAGMAS.H. */
void setcolor16(long color)
{
    outpw(0x3ce, (uint16_t)(((uint16_t)color << 8) | 0x00));
}

void vlin16first(long addr, long count)
{
    uint32_t p = (uint32_t)addr;
    uint8_t v;
    if (count <= 0) return;
    v = dos_phys_read8(p);
    while (count-- > 0)
    {
        dos_phys_write8(p, v);
        p += 80u;
    }
}

void vlin16(long addr, long count)
{
    uint32_t src = (uint32_t)addr;
    uint32_t dst = (uint32_t)addr;
    while (count-- > 0)
    {
        uint8_t v = dos_phys_read8(src);
        dos_phys_write8(dst, v);
        src += 80u;
        dst += 80u;
    }
}

void drawpixel16(long pixel)
{
    uint32_t p = (uint32_t)pixel;
    uint8_t mask = (uint8_t)(0x80u >> (p & 7u));
    uint32_t byte_addr = 0xA0000u + (p >> 3);
    uint8_t latch;

    outpw(0x3ce, (uint16_t)(((uint16_t)mask << 8) | 0x08));
    latch = dos_phys_read8(byte_addr);
    (void)latch;
    dos_phys_write8(byte_addr, 0x08);
}

void fillscreen16(long offset, long color, long pixels)
{
    uint32_t addr = 0xA0000u + (uint32_t)offset;
    long dwords;

    setcolor16(color);
    outpw(0x3ce, 0xff08u);
    dwords = pixels >> 5;
    while (dwords-- > 0)
    {
        dos_phys_write32(addr, 0x0000ff08u);
        addr += 4u;
    }
}

void limitrate(void)
{
    unsigned n;

    for (n = 0; n < 1024u && (inp(0x3da) & 8u); ++n) { }
    for (n = 0; n < 1024u && !(inp(0x3da) & 8u); ++n) { }
}

int setupmouse(void)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 0;
    int386(0x33, &in, &out);
    return out.w.ax & 0xffff;
}

void readmousexy(long *x, long *y)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 11;
    int386(0x33, &in, &out);
    if (x) *x = (short)out.w.cx;
    if (y) *y = (short)out.w.dx;
}

void readmousebstatus(long *status)
{
    union REGS in, out;
    memset(&in, 0, sizeof(in));
    in.w.ax = 3;
    int386(0x33, &in, &out);
    if (status) *status = out.w.bx & 7;
}
