#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <conio.h>
#include "dos_phys.h"
#include "tsm.h"

#include "ez.h"
#include "dos_api_version.h"

const native_ez_process_requirements __native_ez_process_requirements = {
    16u * 1024u, /* native ARM stack: max remaining frame is loadtmb() ~8 KiB */
    4u * 1024u,  /* guest DOS SS:SP stack for BIOS/IRQ servicing */
    DOS_API_VERSION
};

/* Watcom pragma helpers used by the released Build sources. */
void setvmode(long mode)
{
    union REGS regs = {0};
    regs.x.eax = (uint32_t)mode;
    int386(0x10, &regs, &regs);
}

void int5(void)
{
    union REGS regs = {0};
    int386(0x05, &regs, &regs);
}

void qlimitrate(void)
{
    while (inp(0x3da) & 1)
        TSM_Yield();
}

/* int386x() already preserves/restores the native application's x86 segments. */
void backupsegs(void) { }
void restoresegs(void) { }

/* MMX overlay only replaces scalar Build routines with x86 MMX code. */
long mmxoverlay(void)
{
    return 0;
}

void redblueblit(char *red, char *blue, long bytes)
{
    long i;

    for (i = 0; i + 3 < bytes; i += 4)
    {
        uint32_t r;
        uint32_t b;
        uint32_t out;

        memcpy(&r, red + i, sizeof(r));
        memcpy(&b, blue + i, sizeof(b));
        out = (b << 4) + r;
        dos_phys_write32(0xa0000u + (uint32_t)i, out);
    }
}

/* Return the largest currently available DOS block, in bytes. */
long Z_AvailHeap(void)
{
#ifdef ELF_MODE
    return (long)malloc_largest_block();
#else
    union REGS regs = {0};

    regs.h.ah = 0x48;
    regs.w.bx = 0xffff;
    int386(0x21, &regs, &regs);

    if (regs.x.cflag)
        return (long)((uint32_t)regs.w.bx << 4);

    /* Defensive: free an unexpectedly successful 0xffff-paragraph request. */
    {
        struct SREGS sregs = {0};
        uint16_t segment = regs.w.ax;

        regs.x.eax = 0;
        regs.h.ah = 0x49;
        sregs.es = segment;
        int386x(0x21, &regs, &regs, &sregs);
    }

    return (long)(0xffffu << 4);
#endif
}


/* Exact translations of MACT386.LIB mathutil.c. */
static long mact_abs32(long v)
{
    uint32_t u = (uint32_t)v;
    uint32_t m = (uint32_t)-(v < 0);
    return (long)((u ^ m) - m);
}

long FindDistance2D(long dx, long dy)
{
    long a = mact_abs32(dx);
    long b = mact_abs32(dy);
    long t, tmp;

    if (b == 0) return a;
    if (a == 0) return b;

    if (a < b)
    {
        tmp = a; a = b; b = tmp;
    }

    t = b + (b >> 1);
    return a - (a >> 5) - (a >> 7) + (t >> 2) + (t >> 6);
}

long FindDistance3D(long dx, long dy, long dz)
{
    long a = mact_abs32(dx);
    long b = mact_abs32(dy);
    long c = mact_abs32(dz);
    long tmp, sum;

    if (a < b)
    {
        tmp = a; a = b; b = tmp;
    }
    if (a < c)
    {
        tmp = a; a = c; c = tmp;
    }

    sum = b + c;
    return a - (a >> 4) + (sum >> 2) + (sum >> 3);
}
