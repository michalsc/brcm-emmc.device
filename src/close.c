#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/io.h>

#include "emmc.h"

ULONG EMMC_Close(struct IORequest * io asm("a1"))
{
    struct EMMCBase *EMMCBase = (struct EMMCBase *)io->io_Device;
    struct EMMCUnit *u = (struct EMMCUnit *)io->io_Unit;

    u->su_Unit.unit_OpenCnt--;
    EMMCBase->emmc_Device.dd_Library.lib_OpenCnt--;

    if (EMMCBase->emmc_Device.dd_Library.lib_OpenCnt == 0)
    {
        if (EMMCBase->emmc_Device.dd_Library.lib_Flags & LIBF_DELEXP)
        {
            return EMMC_Expunge(EMMCBase);
        }
    }
    
    return 0;
}
