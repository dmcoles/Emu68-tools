#include <clib/alib_protos.h>
#include <exec/resident.h>
#include <exec/nodes.h>
#include <exec/devices.h>
#include <exec/errors.h>

#include <utility/tagitem.h>

#include <devices/sana2.h>
#include <devices/sana2specialstats.h>
#include <devices/sana2wireless.h>
#include <devices/newstyle.h>

#if defined(__INTELLISENSE__)
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <clib/dos_protos.h>
#else
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/dos.h>
#endif

#include <stdint.h>

#include "wifipi.h"
#include "packet.h"
#include "brcm.h"

#define D(x) x
#define UNIT_STACK_SIZE (32768 / sizeof(ULONG))
#define UNIT_TASK_PRIORITY 10

void UnitTask(struct WiFiUnit *unit, struct Task *parent)
{
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct MsgPort *port;
    struct timerequest *tr;
    ULONG scanDelay = 10;
    ULONG sigset;

    D(bug("[WiFi.0] Unit task starting\n"));

    port = CreateMsgPort();
    tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));
    unit->wu_CmdQueue = CreateMsgPort();
    unit->wu_WriteQueue = CreateMsgPort();

    if (port == NULL || tr == NULL || unit->wu_CmdQueue == NULL || unit->wu_WriteQueue == NULL)
    {
        D(bug("[WiFi.0] Failed to create requested MsgPorts\n"));
        
        DeleteMsgPort(unit->wu_WriteQueue);
        DeleteMsgPort(unit->wu_CmdQueue);
        DeleteIORequest((struct IORequest *)tr);
        DeleteMsgPort(port);
        unit->wu_CmdQueue = NULL;
        return;
    }

    if (OpenDevice("timer.device", UNIT_VBLANK, &tr->tr_node, 0))
    {
        D(bug("[WiFi.0] Failed to open timer.device\n"));
        DeleteIORequest(&tr->tr_node);
        DeleteMsgPort(port);
        DeleteMsgPort(unit->wu_CmdQueue);
        unit->wu_CmdQueue = NULL;
        return;
    }

    unit->wu_Unit.unit_MsgPort.mp_SigTask = FindTask(NULL);
    unit->wu_Unit.unit_flags = PA_IGNORE;
    NewList(&unit->wu_Unit.unit_MsgPort.mp_MsgList);

    /* Let timer run every 10 seconds, this will trigger scan for networks */
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_sec = 1;
    tr->tr_time.tv_micro = 0;
    SendIO(&tr->tr_node);

    /* Signal parent that Unit task is up and running now */
    Signal(parent, SIGBREAKF_CTRL_F);
    
    do {
        sigset = Wait((1 << unit->wu_CmdQueue->mp_SigBit) |
                      (1 << port->mp_SigBit) |
                      SIGBREAKF_CTRL_C);
        
        // Handle periodic request
        if (sigset & (1 << port->mp_SigBit))
        {
            // Check if IO really completed. If yes, remove it from the queue
            if (CheckIO(&tr->tr_node))
            {
                WaitIO(&tr->tr_node);
            }




            // Restart timer request
            tr->tr_node.io_Command = TR_ADDREQUEST;
            tr->tr_time.tv_sec = 1;
            tr->tr_time.tv_micro = 0;
            SendIO(&tr->tr_node);
        }

        // IO queue got a new message
        if (sigset & (1 << unit->wu_CmdQueue->mp_SigBit))
        {
            struct IOSana2Req *io;
            
            // Drain command queue and process it
            while ((io = (struct IOSana2Req *)GetMsg(unit->wu_CmdQueue)))
            {
                // Requests are handled one after another while holding a lock
                ObtainSemaphore(&unit->wu_Lock);
                HandleRequest(io);
                ReleaseSemaphore(&unit->wu_Lock);
            }
        }

        if (sigset & (1 << unit->wu_WriteQueue->mp_SigBit))
        {
            struct IOSana2Req *io;
            
            // Drain Data write queue and process it
            while ((io = (struct IOSana2Req *)GetMsg(unit->wu_WriteQueue)))
            {
                /* ... */
            }
        }

        // If CtrlC is sent, abort timer request
        if (sigset & SIGBREAKF_CTRL_C)
        {
            // Abort timer IO
            AbortIO(&tr->tr_node);
            WaitIO(&tr->tr_node);
        }
    } while ((sigset & SIGBREAKF_CTRL_C) == 0);

    DeleteMsgPort(unit->wu_CmdQueue);
}

void StartUnit(struct WiFiUnit *unit)
{
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    if (WiFiBase->w_DosBase)
    {
        struct Library *DOSBase = (struct Library *)WiFiBase->w_DosBase;

        LONG confLen = 4;
        UBYTE buffer[4];

        /* Get the variable into a too small buffer. Required length will be returned in IoErr() */
        if (GetVar("SYS/Wireless.prefs", buffer, 4, GVF_BINARY_VAR) > 0)
        {
            ULONG requiredLength = IoErr();
            UBYTE *config = AllocPooled(WiFiBase->w_MemPool, requiredLength + 1);
            GetVar("SYS/Wireless.prefs", config, requiredLength + 1, GVF_BINARY_VAR);
            WiFiBase->w_NetworkConfigVar = config;
            WiFiBase->w_NetworkConfigLength = requiredLength;
            ParseConfig(WiFiBase);
        }
    }

    PacketGetVar(WiFiBase->w_SDIO, "cur_etheraddr", unit->wu_OrigEtherAddr, 6);

    D(bug("[WiFi.0] Ethernet addr: %02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n",
        unit->wu_OrigEtherAddr[0], unit->wu_OrigEtherAddr[1], unit->wu_OrigEtherAddr[2],
        unit->wu_OrigEtherAddr[3], unit->wu_OrigEtherAddr[4], unit->wu_OrigEtherAddr[5]));

    CopyMem(unit->wu_OrigEtherAddr, unit->wu_EtherAddr, 6);

    unit->wu_Flags |= IFF_STARTED;
}

void StartUnitTask(struct WiFiUnit *unit)
{
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    APTR entry = (APTR)UnitTask;
    struct Task *task;
    struct MemList *ml;
    ULONG *stack;
    static const char task_name[] = "WiFiPi Unit";

    D(bug("[WiFi] StartUnitTask\n"));

    // Get all memory we need for the receiver task
    task = AllocMem(sizeof(struct Task), MEMF_PUBLIC | MEMF_CLEAR);
    ml = AllocMem(sizeof(struct MemList) + sizeof(struct MemEntry), MEMF_PUBLIC | MEMF_CLEAR);
    stack = AllocMem(UNIT_STACK_SIZE * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);

    // Prepare mem list, put task and its stack there
    ml->ml_NumEntries = 2;
    ml->ml_ME[0].me_Un.meu_Addr = task;
    ml->ml_ME[0].me_Length = sizeof(struct Task);

    ml->ml_ME[1].me_Un.meu_Addr = &stack[0];
    ml->ml_ME[1].me_Length = UNIT_STACK_SIZE * sizeof(ULONG);

    // Set up stack
    task->tc_SPLower = &stack[0];
    task->tc_SPUpper = &stack[UNIT_STACK_SIZE];

    // Push ThisTask and SDIO on the stack
    stack = (ULONG *)task->tc_SPUpper;
    *--stack = (ULONG)FindTask(NULL);
    *--stack = (ULONG)unit;
    task->tc_SPReg = stack;

    task->tc_Node.ln_Name = (char *)task_name;
    task->tc_Node.ln_Type = NT_TASK;
    task->tc_Node.ln_Pri = UNIT_TASK_PRIORITY;

    NewMinList((struct MinList *)&task->tc_MemEntry);
    AddHead(&task->tc_MemEntry, &ml->ml_Node);

    D(bug("[WiFi] UnitTask starting\n"));

    unit->wu_Task = AddTask(task, entry, NULL);

    // Wait for a signal
    Wait(SIGBREAKF_CTRL_F);
}

static const UWORD WiFi_SupportedCommands[] = {
    //CMD_FLUSH,
    //CMD_READ,
    //CMD_UPDATE,
    //CMD_WRITE,

    S2_DEVICEQUERY,
    S2_GETSTATIONADDRESS,
    S2_CONFIGINTERFACE,
    S2_ADDMULTICASTADDRESS,
    S2_DELMULTICASTADDRESS,
    // S2_MULTICAST,
    // S2_BROADCAST,
    // S2_TRACKTYPE,
    // S2_UNTRACKTYPE,
    // S2_GETTYPESTATS,
//    S2_GETSPECIALSTATS,
    S2_GETGLOBALSTATS,
    //S2_ONEVENT,
    //S2_READORPHAN,
    //S2_ONLINE,
    //S2_OFFLINE,
    S2_ADDMULTICASTADDRESSES,
    S2_DELMULTICASTADDRESSES,

    //S2_GETSIGNALQUALITY,
    S2_GETNETWORKS,
    //S2_SETOPTIONS,
    //S2_SETKEY,
    //S2_GETNETWORKINFO,
    //S2_READMGMT,
    //S2_WRITEMGMT,
    //S2_GETRADIOBANDS,
    //S2_GETCRYPTTYPES,

    NSCMD_DEVICEQUERY,
    0
};

static int Do_NSCMD_DEVICEQUERY(struct IOStdReq *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->io_Unit;
    struct ExecBase *SysBase = unit->wu_Base->w_SysBase;

    struct NSDeviceQueryResult *dq;
    dq = io->io_Data;

    D(bug("[WiFi.0] NSCMD_DEVICEQUERY\n"));

    /* Fill out structure */
    dq->nsdqr_DeviceType = NSDEVTYPE_SANA2;
    dq->nsdqr_DeviceSubType = 0;
    dq->nsdqr_SupportedCommands = (UWORD*)WiFi_SupportedCommands;
    io->io_Actual = sizeof(struct NSDeviceQueryResult) + sizeof(APTR);
    dq->nsdqr_SizeAvailable = io->io_Actual;
    io->io_Error = 0;

    return 1;
}

static int Do_S2_ADDMULTICASTADDRESSES(struct IOSana2Req *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    D(bug("[WiFi.0] S2_ADDMULTICASTADDRESS%s\n", (ULONG)(io->ios2_Req.io_Command == S2_ADDMULTICASTADDRESSES ? "ES":"")));

    struct MulticastRange *range;
    uint64_t lower_bound, upper_bound;
    lower_bound = (uint64_t)*(UWORD*)io->ios2_SrcAddr << 32 | *(ULONG*)&io->ios2_SrcAddr[2];
    if (io->ios2_Req.io_Command == S2_ADDMULTICASTADDRESS)
    {
        upper_bound = lower_bound;
    }
    else
    {
        upper_bound = (uint64_t)*(UWORD*)io->ios2_DstAddr << 32 | *(ULONG*)&io->ios2_DstAddr[2];
    }

    /* Go through already registered multicast ranges. If one is found, increase use count and return */
    ForeachNode(&unit->wu_MulticastRanges, range)
    {
        if (range->mr_LowerBound == lower_bound && range->mr_UpperBound == upper_bound)
        {
            range->mr_UseCount++;
            return 1;
        }
    }

    /* No range was found. Create new one and add the multicast range on the WiFi module */
    range = AllocPooledClear(WiFiBase->w_MemPool, sizeof(struct MulticastRange));
    range->mr_UseCount = 1;
    range->mr_LowerBound = lower_bound;
    range->mr_UpperBound = upper_bound;
    AddHead((struct List *)&unit->wu_MulticastRanges, (struct Node *)range);

    /* Add range on WiFi now */
    // ...

    return 1;
}

static int Do_S2_DELMULTICASTADDRESSES(struct IOSana2Req *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    D(bug("[WiFi.0] S2_DELMULTICASTADDRESS%s\n", (ULONG)(io->ios2_Req.io_Command == S2_DELMULTICASTADDRESSES ? "ES":"")));

    struct MulticastRange *range;
    uint64_t lower_bound, upper_bound;
    lower_bound = (uint64_t)*(UWORD*)io->ios2_SrcAddr << 32 | *(ULONG*)&io->ios2_SrcAddr[2];
    if (io->ios2_Req.io_Command == S2_ADDMULTICASTADDRESS)
    {
        upper_bound = lower_bound;
    }
    else
    {
        upper_bound = (uint64_t)*(UWORD*)io->ios2_DstAddr << 32 | *(ULONG*)&io->ios2_DstAddr[2];
    }

    /* Go through already registered multicast ranges. Once found, decrease use count */
    ForeachNode(&unit->wu_MulticastRanges, range)
    {
        if (range->mr_LowerBound == lower_bound && range->mr_UpperBound == upper_bound)
        {
            range->mr_UseCount--;

            /* No user of this multicast range. Remove it and unregister on WiFi module */
            if (range->mr_UseCount == 0)
            {
                Remove((struct Node *)range);
                FreePooled(WiFiBase->w_MemPool, range, sizeof(struct MulticastRange));

                /* Remove the range on WiFi now... */
                // ...
            }
            return 1;
        }
    }

    return 1;
}

static int Do_S2_GETNETWORKS(struct IOSana2Req *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct Library *UtilityBase = WiFiBase->w_UtilityBase;
    APTR pool = io->ios2_Data;
    struct TagItem *tags = io->ios2_StatData;
    struct WiFiNetwork *network;
    CONST_STRPTR only_this_ssid = (CONST_STRPTR)GetTagData(S2INFO_SSID, 0, tags);
    struct TagItem **replyList = NULL;
    ULONG networkCount = 0;

    D(bug("[WiFi.0] S2_GETNETWORKS\n"));

    ObtainSemaphore(&WiFiBase->w_NetworkListLock);

    ForeachNode(&WiFiBase->w_NetworkList, network)
    {
        networkCount++;
    }

    replyList = (struct TagItem **)AllocPooled(pool, networkCount * sizeof(APTR));

    if (replyList == NULL)
    {
        io->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
        io->ios2_WireError = S2WERR_GENERIC_ERROR;
        
        ReleaseSemaphore(&WiFiBase->w_NetworkListLock);
        return 1;
    }

    networkCount = 0;

    ForeachNode(&WiFiBase->w_NetworkList, network)
    {
        struct TagItem *tags = NULL;

        if (only_this_ssid && 0 != _strcmp(only_this_ssid, network->wn_SSID))
        {
            continue;
        }

        tags = AllocPooled(pool, sizeof(struct TagItem) * 10);
        
        if (tags == NULL)
        {
            io->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
            io->ios2_WireError = S2WERR_GENERIC_ERROR;
            
            ReleaseSemaphore(&WiFiBase->w_NetworkListLock);
            return 1;
        }
        replyList[networkCount++] = tags;

        tags->ti_Tag = S2INFO_SSID;
        tags->ti_Data = (ULONG)AllocPooled(pool, network->wn_SSIDLength + 1);
        if (tags->ti_Data == NULL)
        {
            io->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
            io->ios2_WireError = S2WERR_GENERIC_ERROR;
            
            ReleaseSemaphore(&WiFiBase->w_NetworkListLock);
            return 1;
        }
        CopyMem(network->wn_SSID, (APTR)tags->ti_Data, network->wn_SSIDLength + 1);
        tags++;

        tags->ti_Tag = S2INFO_BSSID;
        tags->ti_Data = (ULONG)AllocPooled(pool, 6);
        if (tags->ti_Data == NULL)
        {
            io->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
            io->ios2_WireError = S2WERR_GENERIC_ERROR;
            
            ReleaseSemaphore(&WiFiBase->w_NetworkListLock);
            return 1;
        }
        CopyMem(network->wn_BSID, (APTR)tags->ti_Data, 6);
        tags++;

        tags->ti_Tag = S2INFO_BeaconInterval;
        tags->ti_Data = network->wn_BeaconPeriod;
        tags++;

        tags->ti_Tag = S2INFO_Channel;
        tags->ti_Data = network->wn_ChannelInfo.ci_CHNum;
        tags++;

        tags->ti_Tag = S2INFO_Signal;
        tags->ti_Data = network->wn_RSSI;
        tags++;

        tags->ti_Tag = TAG_DONE;
        tags->ti_Data = 0;
    }

    ReleaseSemaphore(&WiFiBase->w_NetworkListLock);

    io->ios2_DataLength = networkCount;
    io->ios2_StatData = replyList;
    io->ios2_Req.io_Error = 0;
    io->ios2_WireError = 0;

    return 1;
}

int Do_S2_DEVICEQUERY(struct IOSana2Req *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct Sana2DeviceQuery *info = io->ios2_StatData;
    ULONG size;

    D(bug("[WiFi.0] S2_DEVICEQUERY\n"));

    size = info->SizeAvailable;
    if (size < sizeof(*info))
    {
        io->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
        io->ios2_WireError = S2WERR_GENERIC_ERROR;
        
        return 1;
    }
    info->AddrFieldSize = 48;
    info->HardwareType = S2WireType_Ethernet;
    info->BPS = 100000000;
    info->MTU = 1500;
    info->SizeAvailable = size;
    info->SizeSupplied = sizeof(struct Sana2DeviceQuery);
    return 1;
}

static int Do_S2_CONFIGINTERFACE(struct IOSana2Req *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct SDIO *sdio = WiFiBase->w_SDIO;

    D(bug("[WiFi.0] S2_CONFIGINTERFACE\n"));

    if (unit->wu_Flags & IFF_CONFIGURED)
    {
        io->ios2_Req.io_Error = S2ERR_BAD_STATE;
        io->ios2_WireError = S2WERR_IS_CONFIGURED;
    }
    else
    {
        /* Try to set HW addr */
        PacketSetVar(sdio, "cur_etheraddr", io->ios2_SrcAddr, 6);

        /* Get HW addr back */
        PacketGetVar(sdio, "cur_etheraddr", unit->wu_EtherAddr, 6);
        D(bug("[WiFi.0] Ethernet addr set: %02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n",
                    unit->wu_EtherAddr[0], unit->wu_EtherAddr[1], unit->wu_EtherAddr[2],
                    unit->wu_EtherAddr[3], unit->wu_EtherAddr[4], unit->wu_EtherAddr[5]));

        ULONG d11Type = 0;
        static const char * const types[]= { "UNKNOWN", "N", "AC" };
        if (0 == PacketCmdIntGet(sdio, BRCMF_C_GET_VERSION, &d11Type))
        {
            D(bug("[WiFi] D11 Version: %s\n", (ULONG)types[d11Type]));
            sdio->s_Chip->c_D11Type = d11Type;
        }

        PacketUploadCLM(sdio);

        PacketSetVarInt(sdio, "assoc_listen", 10);

        if (sdio->s_Chip->c_ChipID == BRCM_CC_43430_CHIP_ID || sdio->s_Chip->c_ChipID == BRCM_CC_4345_CHIP_ID)
        {
            PacketCmdInt(sdio, 0x56, 0);
        }
        else
        {
            PacketCmdInt(sdio, 0x56, 2);
        }

        PacketSetVarInt(sdio, "bus:txglom", 0);
        PacketSetVarInt(sdio, "bcn_timeout", 10);
        PacketSetVarInt(sdio, "assoc_retry_max", 3);

        /* Pepare event mask. Allow only events which are really needed */
        UBYTE ev_mask[(BRCMF_E_LAST + 7) / 8];
        for (int i=0; i < (BRCMF_E_LAST + 7) / 8; i++) ev_mask[i] = 0;

#define EVENT_BIT(mask, i) (mask)[(i) / 8] |= 1 << ((i) % 8)
#define EVENT_BIT_CLEAR(mask, i) (mask)[(i) / 8] &= ~(1 << ((i) % 8))
        EVENT_BIT(ev_mask, BRCMF_E_IF);
        EVENT_BIT(ev_mask, BRCMF_E_LINK);
        EVENT_BIT(ev_mask, BRCMF_E_AUTH);
        EVENT_BIT(ev_mask, BRCMF_E_ASSOC);
        EVENT_BIT(ev_mask, BRCMF_E_DEAUTH);
        EVENT_BIT(ev_mask, BRCMF_E_DISASSOC);
        EVENT_BIT(ev_mask, BRCMF_E_ESCAN_RESULT);
        EVENT_BIT_CLEAR(ev_mask, 124);
#undef EVENT_BIT

        PacketSetVar(sdio, "event_msgs", ev_mask, (BRCMF_E_LAST + 7) / 8);

        PacketCmdInt(sdio, BRCMF_C_SET_SCAN_CHANNEL_TIME, 40);
        PacketCmdInt(sdio, BRCMF_C_SET_SCAN_UNASSOC_TIME, 40);
        PacketCmdInt(sdio, BRCMF_C_SET_SCAN_PASSIVE_TIME, 120);

        PacketCmdInt(sdio, BRCMF_C_UP, 0);

        char ver[128];
        for (int i=0; i < 128; i++) ver[i] = 0;
        PacketGetVar(sdio, "ver", ver, 128);

        // Remove \r and \n from version string. Replace first found with 0
        for (int i=0; i < 128; i++) { if (ver[i] == 13 || ver[i] == 10) { ver[i] = 0; break; } }
        D(bug("[WiFi.0] Firmware version: %s\n", (ULONG)ver));

        PacketSetVarInt(sdio, "roam_off", 1);

        PacketCmdInt(sdio, BRCMF_C_SET_INFRA, 1);
        PacketCmdInt(sdio, BRCMF_C_SET_PROMISC, 0);
        PacketCmdInt(sdio, BRCMF_C_UP, 1);

        D(bug("[WiFi.0] Interface is up\n"));

        /* Get current address */
        CopyMem(unit->wu_EtherAddr, io->ios2_SrcAddr, 6);

        /* We do not allow to change ethernet address yet */
        unit->wu_Flags |= IFF_CONFIGURED | IFF_UP;
    }

    return 1;
}

void HandleRequest(struct IOSana2Req *io)
{
    struct WiFiUnit *unit = (struct WiFiUnit *)io->ios2_Req.io_Unit;
    struct WiFiBase *WiFiBase = unit->wu_Base;
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    struct SDIO *sdio = WiFiBase->w_SDIO;

    ULONG complete = 0;

    /*
        Only NSCMD_DEVICEQUERY can use standard sized request. All other must be of 
        size IOSana2Req
    */
    if (io->ios2_Req.io_Message.mn_Length < sizeof(struct IOSana2Req) &&
        io->ios2_Req.io_Command != NSCMD_DEVICEQUERY)
    {
        io->ios2_Req.io_Error = IOERR_BADLENGTH;
        complete = 1;
    }
    else
    {
        switch (io->ios2_Req.io_Command)
        {
            case NSCMD_DEVICEQUERY:
                complete = Do_NSCMD_DEVICEQUERY((struct IOStdReq *)io);
                break;

            case S2_DEVICEQUERY:
                complete = Do_S2_DEVICEQUERY(io);
                break;
        
            case S2_GETSTATIONADDRESS:
                D(bug("[WiFi.0] S2_GETSTATIONADDRESS\n"));
                CopyMem(unit->wu_OrigEtherAddr, io->ios2_DstAddr, 6);
                CopyMem(unit->wu_EtherAddr, io->ios2_SrcAddr, 6);
                io->ios2_Req.io_Error = 0;
                complete = 1;
                break;

            case S2_GETGLOBALSTATS:
                D(bug("[WiFi.0] S2_GETGLOBALSTATS\n"));
                CopyMem(&unit->wu_Stats, io->ios2_StatData, sizeof(struct Sana2DeviceStats));
                io->ios2_Req.io_Error = 0;
                complete = 1;
                break;

            case S2_GETNETWORKS:
                complete = Do_S2_GETNETWORKS(io);
                break;

            case S2_ADDMULTICASTADDRESS: /* Fallthrough */
            case S2_ADDMULTICASTADDRESSES:
                complete = Do_S2_ADDMULTICASTADDRESSES(io);
                break;

            case S2_DELMULTICASTADDRESS: /* Fallthrough */
            case S2_DELMULTICASTADDRESSES:
                complete = Do_S2_DELMULTICASTADDRESSES(io);
                break;
            
            case S2_CONFIGINTERFACE:
                complete = Do_S2_CONFIGINTERFACE(io);
                break;

            default:
                D(bug("[WiFi.0] Unknown command %ld\n", io->ios2_Req.io_Command));
                io->ios2_Req.io_Error = IOERR_NOCMD;
                complete = 1;
                break;
        }
    }

    // If command is complete and not quick, reply it now
    if (complete && !(io->ios2_Req.io_Flags & IOF_QUICK))
    {
        ReplyMsg((struct Message *)io);
    }
}
