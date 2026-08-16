#include "INTERRUP.H"
#include "dos-api.h"
#include "dos.h"
#include "cpu.h"

unsigned long DisableInterrupts(void)
{
    CPU *cpu = get_PC()->cpu;
    unsigned long flags = cpu->flags.bits.IF ? 0x200ul : 0ul;
    _disable();
    return flags;
}

void RestoreInterrupts(unsigned long flags)
{
    if (flags & 0x200ul)
        _enable();
    else
        _disable();
}
