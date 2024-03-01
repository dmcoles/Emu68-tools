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
    1,
    NT_UNKNOWN,
    120,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    Init,
};

const char deviceName[] = "Emu68 boot rom";
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

int DiagPoint(APTR boardBase asm("a0"), struct DiagArea *diagCopy asm("a2"), struct ConfigDev *configDev asm("a3"), struct ExecBase *SysBase asm("a6"))
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
    struct ExecBase *SysBase = *(struct ExecBase**)4UL;
    struct Resident *DosResident = FindResident("dos.library");
    void (*InitFunc)(struct Resident * asm("a0"), struct ExecBase * asm("a6")) = DosResident->rt_Init;
    InitFunc(DosResident, SysBase);
}
