#include <exec/types.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/execbase.h>
#include <exec/errors.h>
#include <devices/trackdisk.h>

#include <proto/exec.h>

#include "emmc.h"

void EMMC_Open(struct IORequest * io asm("a1"), LONG unitNumber asm("d0"), ULONG flags asm("d1"))
{
    struct EMMCBase *EMMCBase = (struct EMMCBase *)io->io_Device;
    struct ExecBase *SysBase = EMMCBase->emmc_SysBase;
    (void)flags;

    if (EMMCBase->emmc_Verbose > 1)
    {
        bug("[brcm-emmc] Open(%08lx, %lx, %ld)\n", (ULONG)io, unitNumber, flags);
    }

    /* Do not continue if unit number does not fit */
    if (unitNumber >= EMMCBase->emmc_UnitCount || (unitNumber == 0 && EMMCBase->emmc_HideUnit0)) {
        io->io_Error = TDERR_BadUnitNum;
        return;
    }

    /*
        Do whatever necessary to open given unit number with flags, set NT_REPLYMSG if 
        opening device shall complete with success, set io_Error otherwise
    */
    io->io_Error = 0;

    /* Get unit based on unit number */
    struct EMMCUnit *u = EMMCBase->emmc_Units[unitNumber];

    /* Increase open counter of the unit */
    u->su_Unit.unit_OpenCnt++;

    /* Increase global open coutner of the device */
    EMMCBase->emmc_Device.dd_Library.lib_OpenCnt++;
    EMMCBase->emmc_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

    io->io_Unit = &u->su_Unit;
    io->io_Unit->unit_flags |= UNITF_ACTIVE;
    io->io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    /* In contrast to normal library there is no need to return anything */
}
