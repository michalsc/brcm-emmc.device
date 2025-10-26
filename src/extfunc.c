#include <exec/types.h>

#include "emmc.h"

APTR EMMC_ExtFunc(struct EMMCBase * EMMCBase asm("a6"))
{
    return EMMCBase;
}
