#include <common/compiler.h>
#include <proto/exec.h>
#include "emmc.h"

static void putch(REGARG(UBYTE data, "d0"), REGARG(APTR ignore, "a3"))
{
    (void)ignore;

    *(UBYTE*)0xdeadbeef = data;
}

static void putch_rpc(REGARG(UBYTE data, "d0"), REGARG(APTR ignore, "a3"))
{
    (void)ignore;

    *(UBYTE*)0xdeadbeef = data;
    asm volatile("move.l 4.w, a6; jsr -516(a6)"::"d"(data):"a6");
}

void kprintf(REGARG(const char * msg, "a0"), REGARG(void * args, "a1"), REGARG(struct EMMCBase *EMMCBase, "a3")) 
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    if (EMMCBase != NULL && EMMCBase->emmc_UseRawPutChar) {
        RawDoFmt(msg, args, (APTR)putch_rpc, NULL);
    } else {
        RawDoFmt(msg, args, (APTR)putch, NULL);
    }
}
