#ifndef _COMMON_UUID_H_
#define _COMMON_UUID_H_

#include <exec/types.h>

typedef struct
{
    ULONG time_low;
    UWORD time_mid;
    UWORD time_hi_and_version;
    UBYTE clock_seq_hi_and_reserved;
    UBYTE clock_seq_low;
    UBYTE node[6];
} uuid_t;

#endif // _COMMON_UUID_H_
