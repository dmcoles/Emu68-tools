#include <clib/alib_protos.h>
#include <exec/resident.h>
#include <exec/nodes.h>
#include <exec/devices.h>
#include <exec/errors.h>

#include <utility/tagitem.h>

#include <devices/sana2.h>
#include <devices/sana2specialstats.h>

#include <proto/exec.h>
#include <proto/utility.h>

#include <stdint.h>

#include "wifipi.h"

#define D(x) x

extern const char deviceEnd;
extern const char deviceName[];
extern const char deviceIdString[];
extern const uint32_t InitTable[];

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&deviceEnd,
    RTF_AUTOINIT | RTF_AFTERDOS,
    WIFIPI_VERSION,
    NT_DEVICE,
    WIFIPI_PRIORITY,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    (APTR)InitTable,
};

const char deviceName[] = "wifipi.device";
const char deviceIdString[] = VERSION_STRING;

static uint32_t WiFi_ExtFunc()
{
    return 0;
}

void NewList(struct List *lh)
{
    lh->lh_Head = (struct Node *)&(lh->lh_Tail);
    lh->lh_Tail = NULL;
    lh->lh_TailPred = (struct Node *)&(lh->lh_Head);
}

static BPTR WiFi_Expunge(struct WiFiBase * WiFiBase asm("a6"))
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    BPTR segList = 0;

    D(bug("[WiFi] WiFi_Expunge()\n"));

    /* If device's open count is 0, remove it from list and free memory */
    if (WiFiBase->w_Device.dd_Library.lib_OpenCnt == 0)
    {
        UWORD negSize = WiFiBase->w_Device.dd_Library.lib_NegSize;
        UWORD posSize = WiFiBase->w_Device.dd_Library.lib_PosSize;

        if (WiFiBase->w_UtilityBase != NULL)
        {
            CloseLibrary(WiFiBase->w_UtilityBase);
        }

        /* Return SegList so that DOS can unload the binary */
        segList = WiFiBase->w_SegList;

        /* Remove device node */
        Remove(&WiFiBase->w_Device.dd_Library.lib_Node);

        /* Free memory */
        FreeMem(WiFiBase->w_RequestOrig, 512);
        FreeMem((APTR)((ULONG)WiFiBase - (negSize + posSize)), sizeof(struct WiFiBase));
    }
    else
    {
        WiFiBase->w_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
    }

    D(bug("[WiFi] WiFi_Expunge() returns %08lx\n", segList));

    return segList;
}

static const ULONG rx_tags[] = {
    S2_CopyToBuff,
    0
};

static const ULONG tx_tags[] = {
    S2_CopyFromBuff,
    0
};

void WiFi_Open(struct IOSana2Req * io asm("a1"), LONG unitNumber asm("d0"), ULONG flags asm("d1"))
{
    struct WiFiBase *WiFiBase = (struct WiFiBase *)io->ios2_Req.io_Device;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct Library *UtilityBase = WiFiBase->w_UtilityBase;

    struct TagItem *tags;
    struct WiFiUnit *unit = WiFiBase->w_Unit;
    struct Opener *opener;
    BYTE error=0;
    int i;

    D(bug("[WiFi] WiFiOpen(%08lx, %ld, %lx)\n", (ULONG)io, unitNumber, flags));

    if (io->ios2_Req.io_Message.mn_Length < sizeof(struct IOSana2Req) || unitNumber != 0)
    {
        error = IOERR_OPENFAIL;
    }

    io->ios2_Req.io_Unit = NULL;
    tags = io->ios2_BufferManagement;
    io->ios2_BufferManagement = NULL;

    /* Device sharing */
    if (error == 0)
    {
        if (unit->wu_Unit.unit_OpenCnt != 0 && 
            ((unit->wu_Flags & IFF_SHARED) == 0 || (flags & SANA2OPF_MINE) != 0))
        {
            error = IOERR_UNITBUSY;
        }
    }    

    /* Set flags, alloc opener */
    if (error == 0)
    {
        opener = AllocMem(sizeof(struct Opener), MEMF_PUBLIC | MEMF_CLEAR);
        io->ios2_BufferManagement = opener;

        if (opener != NULL)
        {
            if ((flags & SANA2OPF_MINE) == 0)
                unit->wu_Flags |= IFF_SHARED;
            if ((flags & SANA2OPB_PROM) != 0)
                unit->wu_Flags |= IFF_PROMISC;
        }
        else
        {
            error = IOERR_OPENFAIL;
        }
    }

    /* No errors so far, increase open counters, handle buffer management etc */
    if (error == 0)
    {
        WiFiBase->w_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
        WiFiBase->w_Device.dd_Library.lib_OpenCnt++;
        unit->wu_Unit.unit_OpenCnt++;

        io->ios2_Req.io_Unit = &unit->wu_Unit;

        NewList(&opener->o_ReadPort.mp_MsgList);
        opener->o_ReadPort.mp_Flags = PA_IGNORE;

        for(int i = 0; rx_tags[i] != 0; i++)
            opener->o_RXFunc = (APTR)GetTagData(rx_tags[i], (ULONG)opener->o_RXFunc, tags);
        for(int i = 0; tx_tags[i] != 0; i++)
            opener->o_TXFunc = (APTR)GetTagData(tx_tags[i], (ULONG)opener->o_TXFunc, tags);

        opener->o_FilterHook = (APTR)GetTagData(S2_PacketFilter, 0, tags);

        Disable();
        AddTail((APTR)&unit->wu_Openers, (APTR)opener);
        Enable();

        /* Start unit here? */
    }

    io->ios2_Req.io_Error = error;
}

ULONG WiFi_Close(struct IOSana2Req * io asm("a1"))
{
    struct WiFiBase *WiFiBase = (struct WiFiBase *)io->ios2_Req.io_Device;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct WiFiUnit *u = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct Opener *opener = io->ios2_BufferManagement;

    D(bug("[WiFi] WiFi_Close(%08lx)\n", (ULONG)io));

    /* Stop unit? */

    // ...

    if (opener)
    {
        Disable();
        Remove((struct Node *)opener);
        Enable();

        FreeMem(opener, sizeof(struct Opener));
    }

    u->wu_Unit.unit_OpenCnt--;
    WiFiBase->w_Device.dd_Library.lib_OpenCnt--;

    if (WiFiBase->w_Device.dd_Library.lib_OpenCnt == 0)
    {
        if (WiFiBase->w_Device.dd_Library.lib_Flags & LIBF_DELEXP)
        {
            return WiFi_Expunge(WiFiBase);
        }
    }
    
    return 0;
}

static const uint32_t wifipi_functions[] = {
    (uint32_t)WiFi_Open,
    (uint32_t)WiFi_Close,
    (uint32_t)WiFi_Expunge,
    (uint32_t)WiFi_ExtFunc,
#if 0
    (uint32_t)WiFi_BeginIO,
    (uint32_t)WiFi_AbortIO,
#endif
    -1
};

const uint32_t InitTable[4] = {
    sizeof(struct WiFiBase), 
    (uint32_t)wifipi_functions, 
    0, 
    (uint32_t)WiFi_Init
};
