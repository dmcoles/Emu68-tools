/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include <proto/exec.h>
#include <stdint.h>

#include <common/compiler.h>

#include "devicetree.h"



#define MANUFACTURER_ID     0x6d73
#define PRODUCT_ID          0x21
#define SERIAL_NUMBER       0x04f6403d 

extern UBYTE diag_start;
extern UBYTE rom_end;
extern UBYTE rom_start;
extern UBYTE ramcopy_end;
extern ULONG diag_offset;
extern const char deviceName[];
extern const char deviceIdString[];
void Init();

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&ramcopy_end,
    RTF_COLDSTART,
    DT_VERSION,
    NT_RESOURCE,
    DT_PRIORITY,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    Init,
};

const char deviceName[] = "devicetree.resource";
const char deviceIdString[] = VERSION_STRING;

const APTR patchListRAM[] = {
    (APTR)((intptr_t)&RomTag.rt_MatchTag),
    (APTR)((intptr_t)&RomTag.rt_EndSkip),
    (APTR)-1
};

const APTR patchListROM[] = {
    (APTR)&RomTag.rt_Init,
    (APTR)&RomTag.rt_Name,
    (APTR)&RomTag.rt_IdString,
    (APTR)-1
};

int DiagPoint(REGARG(APTR boardBase, "a0"), REGARG(struct DiagArea *diagCopy, "a2"), 
              REGARG(struct ConfigDev *configDev, "a3"), REGARG(struct ExecBase *SysBase, "a6"))
{
    const APTR *patch = &patchListRAM[0];
    ULONG offset = (ULONG)&diag_offset;

    /* Patch parts which reside in RAM only */
    while(*patch != (APTR)-1)
    {
        ULONG * address = (ULONG *)((intptr_t)*patch - offset + (ULONG)diagCopy);
        *address += (intptr_t)diagCopy - offset;
        patch++;
    }

    /* Patch parts which are in the ROM image */
    patch = &patchListROM[0];
    while(*patch != (APTR)-1)
    {
        ULONG * address = (ULONG *)((intptr_t)*patch - offset + (ULONG)diagCopy);
        *address += (intptr_t)boardBase;
        patch++;
    }

    return 1;
}

void BootPoint()
{

}
