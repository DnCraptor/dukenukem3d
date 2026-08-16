/* Native ELF/EZ applications are resident ARM code; DPMI locking is a no-op. */
#include "dpmi.h"

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
