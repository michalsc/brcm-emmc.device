#include <exec/types.h>
#include <exec/libraries.h>

#include "emmc.h"

ULONG EMMC_Expunge(struct EMMCBase * EMMCBase asm("a6"))
{
    /* This is rom based device. no expunge here */
    if (EMMCBase->emmc_Device.dd_Library.lib_OpenCnt > 0)
    {
        EMMCBase->emmc_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
    }

    return 0;
}
