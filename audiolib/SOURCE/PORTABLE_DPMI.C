/* Native ELF/EZ applications are resident ARM code; DPMI locking is a no-op. */
#include "dpmi.h"
#include <stdio.h>
#include <dos.h>
#include "dos_mem.h"

/*
 * AudioLib uses DPMI_GetDOSMemory() for ISA-DMA-visible mix buffers. Native
 * EZ code has no DPMI host, so use DOS conventional memory. The descriptor is
 * the DOS paragraph segment, which is sufficient for DPMI_FreeDOSMemory().
 */
int DPMI_GetDOSMemory(void **ptr, int *descriptor, unsigned length)
{
    void *p;
    uint16_t segment;

    if (ptr == 0 || descriptor == 0)
        return DPMI_Error;

    printf("DPMI: dos_alloc_low(%u)\n", length);
    p = dos_alloc_low((size_t)length);
    printf("DPMI: dos_alloc_low -> %p\n", p);
    if (p == 0)
        return DPMI_Error;

    segment = dos_ptr_segment(p);
    if (segment == 0)
        return DPMI_Error;

    printf("DPMI: segment=%04x\n", segment);
    *ptr = p;
    *descriptor = (int)segment;
    return DPMI_Ok;
}

int DPMI_FreeDOSMemory(int descriptor)
{
    union REGS regs = {0};
    struct SREGS sregs = {0};

    if (descriptor <= 0 || descriptor > 0xffff)
        return DPMI_Error;

    segread(&sregs);
    regs.h.ah = 0x49;
    sregs.es = (uint16_t)descriptor;
    int386x(0x21, &regs, &regs, &sregs);
    return regs.x.cflag ? DPMI_Error : DPMI_Ok;
}

int DPMI_LockMemory(void *address, unsigned length)
{
    (void)address;
    (void)length;
    return DPMI_Ok;
}

int DPMI_LockMemoryRegion(void *start, void *end)
{
    (void)start;
    (void)end;
    return DPMI_Ok;
}

int DPMI_UnlockMemory(void *address, unsigned length)
{
    (void)address;
    (void)length;
    return DPMI_Ok;
}

int DPMI_UnlockMemoryRegion(void *start, void *end)
{
    (void)start;
    (void)end;
    return DPMI_Ok;
}
