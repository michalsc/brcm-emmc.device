#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>
#include <inline/alib.h>

#include <common/uuid.h>

#include <stdint.h>

#include "emmc.h"
#include "findtoken.h"
#include "mbox.h"

#define GPT_AMIGA_RIGID_DISK_BLOCK \
    { LE32(0x3F82EEBC), LE16(0x87C9), LE16(0x4097), \
    0x81, 0x65, { 0x89, 0xD6, 0x54, 0x05, 0x57, 0xC0 }}

struct MBRPart {
    UBYTE mbr_Boot;
    UBYTE mbr_CHSStart[3];
    UBYTE mbr_Type;
    UBYTE mbr_CHSEnd[3];
    ULONG mbr_FirstSector;
    ULONG mbr_SectorCount;
};

struct GPTPart {
    UBYTE gpt_Signature[8];
    ULONG gpt_Revision;
    ULONG gpt_HeaderSize;
    ULONG gpt_CRC32;
    ULONG gpt_Reserved;
    ULONG gpt_SelfLocLo;
    ULONG gpt_SelfLocHi;
    ULONG gpt_BackupLocLo;
    ULONG gpt_BackupLocHi;
    ULONG gpt_FirstSectorLo;
    ULONG gpt_FirstSectorHi;
    ULONG gpt_LastSectorLo;
    ULONG gpt_LastSectorHi;
    uuid_t gpt_DiskGUID;
    ULONG gpt_PartTableLocLo;
    ULONG gpt_PartTableLocHi;
    ULONG gpt_PartCount;
    ULONG gpt_PartItemSize;
    ULONG gpt_PartTableCRC32;
};

struct GPTPartition {
    uuid_t gpa_TypeGUID;
    uuid_t gpa_UniqueID;
    ULONG gpa_FirstSectorLo;
    ULONG gpa_FirstSectorHi;
    ULONG gpa_LastSectorLo;
    ULONG gpa_LastSectorHi;
    ULONG gpa_AttrLo;
    ULONG gpa_AttrHi;
    UWORD gpa_Label[36];
};

ULONG crc32(UBYTE *message, ULONG length)
{
    int i, j;
    ULONG byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    while (length--)
    {
        byte = message[i]; // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--)
        { // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
        i = i + 1;
    }
    return ~crc;
}

int check_gpt_support(struct EMMCBase *EMMCBase)
{
    UBYTE *block = NULL;
    struct ExecBase *SysBase = EMMCBase->emmc_SysBase;
    int result = 0;
    int has_mbr = 0;
    int protective_mbr = 0;
    int has_gpt = 0;
    const uuid_t uuid_rdb = GPT_AMIGA_RIGID_DISK_BLOCK;

    /* Get total block count. At this stage Unit 0 **must** exist */
    ULONG blockCount = EMMCBase->emmc_Units[0]->su_BlockCount;

    block = AllocMem(4096, MEMF_ANY);

    if (block != NULL)
    {
        /* Read block 0 of the device */
        if (emmc_read(block, 512, 0, EMMCBase))
        {
            /* Check signature */
            if (block[510] == 0x55 && block[511] == 0xaa) {
                /* Check if MBR table exists and consists only of protective MBR */
                struct MBRPart *part = (struct MBRPart *)&block[446];

                /*
                    Protective MBR has only one primary partition with type 0xEE, starting at sector one
                    Other partitions are not defined (type 0)
                */
                if (part[0].mbr_Type == 0xee && 
                    part[1].mbr_Type == 0 && 
                    part[2].mbr_Type == 0 && 
                    part[3].mbr_Type == 0)
                {
                    if (LE32(part[0].mbr_FirstSector) == 1 && LE32(part[0].mbr_SectorCount) == blockCount - 1) {
                        protective_mbr = 1;
                    }
                }

                /* Signature is valid so we have MBR anyway */
                has_mbr = 1;
            }
        }

        if (EMMCBase->emmc_Verbose) {
            if (has_mbr) {
                bug("[brcm-emmc] MBR signature is valid\n");

                if (protective_mbr) {
                    bug("[brcm-emmc] Protective MBR detected\n");
                }
            }
        }

        /* Read block 1 of the device */
        if (emmc_read(block, 512, 1, EMMCBase))
        {
            struct GPTPart *gpt = (struct GPTPart *)block;
            ULONG crc = LE32(gpt->gpt_CRC32);

            /* Check if the signature equals "EFI PART" - 8 bytes */
            if ((*(ULONG *)&gpt->gpt_Signature[0] == 0x45464920) &&
                (*(ULONG *)&gpt->gpt_Signature[4] == 0x50415254))
            {
                if (EMMCBase->emmc_Verbose) {
                    bug("[brcm-emmc] GPT signature is valid, CRC32=%08lx\n", crc);
                }

                /* Set CRC32 field to zero when calculating the checksum */
                gpt->gpt_CRC32 = 0;
                ULONG calculated_crc = crc32(block, LE32(gpt->gpt_HeaderSize));

                if (EMMCBase->emmc_Verbose) {
                    bug("[brcm-emmc] Calculated CRC32=%08lx\n", calculated_crc);
                }

                if (crc == calculated_crc)
                {
                    /* the CRC32 of GPT header is OK, load the partition list */
                    ULONG tableSize = LE32(gpt->gpt_PartCount) * LE32(gpt->gpt_PartItemSize);
                    ULONG tableSizeAligned = (tableSize + 511) & ~511;
                    char *part = AllocMem(tableSizeAligned, MEMF_ANY);

                    if (EMMCBase->emmc_Verbose) {
                        bug("[brcm-emmc] GPT partition table size %ld bytes with capacity of %ld entries\n", tableSize, LE32(gpt->gpt_PartCount));
                    }

                    if (part)
                    {
                        uint64_t lba = LE32(gpt->gpt_PartTableLocLo);
                        lba |= (uint64_t)LE32(gpt->gpt_PartTableLocHi) << 32;

                        if (emmc_read(part, tableSizeAligned, lba, EMMCBase))
                        {
                            ULONG crc = crc32(part, tableSize);

                            if (EMMCBase->emmc_Verbose) {
                                bug("[brcm-emmc] Calculated CRC32 of GPT partition table=%08lx, expected=%08lx\n", crc, LE32(gpt->gpt_PartTableCRC32));
                            }

                            if (crc == LE32(gpt->gpt_PartTableCRC32))
                            {
                                has_gpt = 1;

                                for (ULONG i=0; i < LE32(gpt->gpt_PartCount); i++) {
                                    struct GPTPartition *p = (struct GPTPartition *)(part + i * LE32(gpt->gpt_PartItemSize));

                                    if (p->gpa_TypeGUID.time_low == 0 && p->gpa_TypeGUID.time_mid == 0 && p->gpa_TypeGUID.time_hi_and_version == 0 &&
                                        p->gpa_TypeGUID.clock_seq_hi_and_reserved == 0 && p->gpa_TypeGUID.clock_seq_low == 0 &&
                                        p->gpa_TypeGUID.node[0] == 0 && p->gpa_TypeGUID.node[1] == 0 && p->gpa_TypeGUID.node[2] == 0 &&
                                        p->gpa_TypeGUID.node[3] == 0 && p->gpa_TypeGUID.node[4] == 0 && p->gpa_TypeGUID.node[5] == 0)
                                    {
                                        /* Unused entry, skip */
                                        continue;
                                    }

                                    if (EMMCBase->emmc_Verbose) {
                                        uuid_t id = p->gpa_TypeGUID;
                                        bug("[brcm-emmc] Partition%ld: type 0x%08lx-%04lx-%04lx-%02lx%02lx-%02lx%02lx%02lx%02lx%02lx%02lx, start 0x%08lx%08lx, end 0x%08lx%08lx\n", i, 
                                            LE32(id.time_low), LE16(id.time_mid), LE16(id.time_hi_and_version), id.clock_seq_hi_and_reserved, id.clock_seq_low,
                                            id.node[0], id.node[1], id.node[2], id.node[3], id.node[4], id.node[5],
                                            LE32(p->gpa_FirstSectorHi), LE32(p->gpa_FirstSectorLo), LE32(p->gpa_LastSectorHi), LE32(p->gpa_LastSectorLo));
                                    }

                                    if (p->gpa_TypeGUID.time_low == uuid_rdb.time_low &&
                                        p->gpa_TypeGUID.time_mid == uuid_rdb.time_mid &&
                                        p->gpa_TypeGUID.time_hi_and_version == uuid_rdb.time_hi_and_version &&
                                        p->gpa_TypeGUID.clock_seq_hi_and_reserved == uuid_rdb.clock_seq_hi_and_reserved &&
                                        p->gpa_TypeGUID.clock_seq_low == uuid_rdb.clock_seq_low &&
                                        p->gpa_TypeGUID.node[0] == uuid_rdb.node[0] &&
                                        p->gpa_TypeGUID.node[1] == uuid_rdb.node[1] &&
                                        p->gpa_TypeGUID.node[2] == uuid_rdb.node[2] &&
                                        p->gpa_TypeGUID.node[3] == uuid_rdb.node[3] &&
                                        p->gpa_TypeGUID.node[4] == uuid_rdb.node[4] &&
                                        p->gpa_TypeGUID.node[5] == uuid_rdb.node[5])
                                    {
                                        if (EMMCBase->emmc_Verbose) {
                                            bug("[brcm-emmc] Partition%ld seems to be Amiga Rigid Disk Block, adding as a device unit\n", i);
                                        }
                                        
                                        if (p->gpa_FirstSectorHi == 0 && p->gpa_LastSectorHi == 0)
                                        {
                                            struct EMMCUnit *unit = AllocMem(sizeof(struct EMMCUnit), MEMF_PUBLIC | MEMF_CLEAR);
                                            unit->su_StartBlock = LE32(p->gpa_FirstSectorLo);
                                            unit->su_BlockCount = LE32(p->gpa_LastSectorLo) - unit->su_StartBlock + 1;
                                            unit->su_Base = EMMCBase;
                                            unit->su_UnitNum = EMMCBase->emmc_UnitCount;
                                            
                                            EMMCBase->emmc_Units[EMMCBase->emmc_UnitCount++] = unit;
                                        }
                                        else
                                        {
                                            bug("[brcm-emmc] Partition%ld is too big for 32-bit sector numbers, skipping\n", i);
                                        }
                                    }
                                }
                            }
                        }

                        FreeMem(part, tableSizeAligned);
                    }
                }
            }
        }

        FreeMem(block, 4096);
    }

    return has_gpt && protective_mbr;
}
