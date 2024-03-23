#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/timer.h>

#include <proto/exec.h>

#include "sdio.h"
#include "brcm.h"
#include "wifipi.h"
#include "packet.h"

/*
 * brcmfmac sdio bus specific header
 * This is the lowest layer header wrapped on the packets transmitted between
 * host and WiFi dongle which contains information needed for SDIO core and
 * firmware
 *
 * It consists of 3 parts: hardware header, hardware extension header and
 * software header
 * hardware header (frame tag) - 4 bytes
 * Byte 0~1: Frame length
 * Byte 2~3: Checksum, bit-wise inverse of frame length
 * hardware extension header - 8 bytes
 * Tx glom mode only, N/A for Rx or normal Tx
 * Byte 0~1: Packet length excluding hw frame tag
 * Byte 2: Reserved
 * Byte 3: Frame flags, bit 0: last frame indication
 * Byte 4~5: Reserved
 * Byte 6~7: Tail padding length
 * software header - 8 bytes
 * Byte 0: Rx/Tx sequence number
 * Byte 1: 4 MSB Channel number, 4 LSB arbitrary flag
 * Byte 2: Length of next data frame, reserved for Tx
 * Byte 3: Data offset
 * Byte 4: Flow control bits, reserved for Tx
 * Byte 5: Maximum Sequence number allowed by firmware for Tx, N/A for Tx packet
 * Byte 6~7: Reserved
 */
#define SDPCM_HWHDR_LEN			4
#define SDPCM_HWEXT_LEN			8
#define SDPCM_SWHDR_LEN			8
#define SDPCM_HDRLEN			(SDPCM_HWHDR_LEN + SDPCM_SWHDR_LEN)
/* software header */
#define SDPCM_SEQ_MASK			0x000000ff
#define SDPCM_SEQ_WRAP			256
#define SDPCM_CHANNEL_MASK		0x00000f00
#define SDPCM_CHANNEL_SHIFT		8
#define SDPCM_CONTROL_CHANNEL	0	/* Control */
#define SDPCM_EVENT_CHANNEL		1	/* Asyc Event Indication */
#define SDPCM_DATA_CHANNEL		2	/* Data Xmit/Recv */
#define SDPCM_GLOM_CHANNEL		3	/* Coalesced packets */
#define SDPCM_TEST_CHANNEL		15	/* Test/debug packets */
#define SDPCM_GLOMDESC(p)		(((u8 *)p)[1] & 0x80)
#define SDPCM_NEXTLEN_MASK		0x00ff0000
#define SDPCM_NEXTLEN_SHIFT		16
#define SDPCM_DOFFSET_MASK		0xff000000
#define SDPCM_DOFFSET_SHIFT		24
#define SDPCM_FCMASK_MASK		0x000000ff
#define SDPCM_WINDOW_MASK		0x0000ff00
#define SDPCM_WINDOW_SHIFT		8

struct Packet {
    // Hardware header
    UWORD p_Length;
    UWORD c_ChkSum;
    // Software header
    UBYTE c_Seq;
    UBYTE c_ChannelFlag;
    UBYTE c_NextLength;
    UBYTE c_DataOffset;
    UBYTE c_FlowControl;
    UBYTE c_MaxSeq;
    UBYTE c_Reserved[2];
} __attribute__((packed));

#define BCDC_DCMD_ERROR		0x01		/* 1=cmd failed */
#define BCDC_DCMD_SET		0x02		/* 0=get, 1=set cmd */

struct PacketCmd {
    ULONG c_Command;
    ULONG c_Length;
    UWORD c_Flags;
    UWORD c_ID;
    ULONG c_Status;
} __attribute((packed));

#define D(x) x

#define PACKET_RECV_STACKSIZE   (65536 / sizeof(ULONG))
#define PACKET_RECV_PRIORITY    20

#define PACKET_WAIT_DELAY_MIN   1000
#define PACKET_WAIT_DELAY_MAX   1000000

void PacketDump(struct SDIO *sdio, APTR data, char *src);

void PacketReceiver(struct SDIO *sdio, struct Task *caller)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    ULONG waitDelay = PACKET_WAIT_DELAY_MAX;

    D(bug("[WiFi.RECV] Packet receiver task\n"));
    D(bug("[WiFi.RECV] SDIO=%08lx, Caller task=%08lx\n", (ULONG)sdio, (ULONG)caller));

    // Create MessagePort and timer.device IORequest
    struct MsgPort *port = CreateMsgPort();
    struct timerequest *tr = (struct timerequest *)CreateIORequest(port, sizeof(struct timerequest));

    if (port == NULL || tr == NULL)
    {
        D(bug("[WiFi.RECV] Failed to create IO Request\n"));
        if (port != NULL) DeleteMsgPort(port);
        Signal(caller, SIGBREAKF_CTRL_C);
        return;
    }

    if (OpenDevice("timer.device", UNIT_MICROHZ, &tr->tr_node, 0))
    {
        D(bug("[WiFi.RECV] Failed to open timer.device\n"));
        DeleteIORequest(&tr->tr_node);
        DeleteMsgPort(port);
        Signal(caller, SIGBREAKF_CTRL_C);
        return;
    }

    // Set up receiver task pointer in SDIO
    sdio->s_ReceiverTask = FindTask(NULL);

    // Signal caller that we are done with setup
    Signal(caller, SIGBREAKF_CTRL_C);

    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_sec = waitDelay / 1000000;
    tr->tr_time.tv_micro = waitDelay % 1000000;
    SendIO(&tr->tr_node);

    // Clear 64 bytes of RX buffer
    UBYTE *buffer = sdio->s_RXBuffer;
    struct Packet *pkt = sdio->s_RXBuffer;

    for (int i=0; i < 64; i++) buffer[i] = 0;

    // Loop forever
    while(1)
    {
        ULONG sigSet = Wait(SIGBREAKF_CTRL_C | (1 << port->mp_SigBit));

        // Signal from timer.device?
        if (sigSet & (1 << port->mp_SigBit))
        {
            UBYTE gotTransfer = 0;

            // Check if IO really completed. If yes, remove it from the queue
            if (CheckIO(&tr->tr_node))
            {
                WaitIO(&tr->tr_node);
            }

            //D(bug("[WiFi.RECV] Periodic check for packets\n"));

            sdio->RecvPKT(buffer, 64, sdio);
            if (LE16(pkt->p_Length) != 0)
            {
                UWORD pktLen = LE16(pkt->p_Length);
                UWORD pktChk = LE16(pkt->c_ChkSum);

                D(bug("[WiFi.RECV] Potential packet with length %ld bytes:\n", pktLen));

                if ((pktChk | pktLen) == 0xffff)
                {
                    if (pktLen > 64)
                    {
                        D(bug("[WiFi.RECV]   Fetching more data from the block\n"));
                        sdio->RecvPKT(&buffer[64], pktLen - 64, sdio);
                    }

                    PacketDump(sdio, pkt, "WiFi.RECV");

                    switch(pkt->c_ChannelFlag)
                    {
                        case SDPCM_CONTROL_CHANNEL:
                            break;

                        case SDPCM_EVENT_CHANNEL:
                            break;
                    }

                    // Mark that we have the transfer, we will wait for next one a bit shorter
                    gotTransfer = 1;
                }
                else
                {
                    D(bug("[WiFi.RECV] Nope. Data:\n"));
                    for (int i=0; i < 256; i++)
                    {
                        if (i % 16 == 0)
                            bug("[WiFi]  ");
                        bug(" %02lx", buffer[i]);
                        if (i % 16 == 15)
                            bug("\n");
                    }
                }
            }

            if (gotTransfer)
            {
                waitDelay = PACKET_WAIT_DELAY_MIN;
            }
            else
            {
                if (waitDelay < PACKET_WAIT_DELAY_MAX)
                {
                    ULONG oldwait = waitDelay;
                    waitDelay = (waitDelay * 3) / 2;
                    if (waitDelay > PACKET_WAIT_DELAY_MAX) waitDelay = PACKET_WAIT_DELAY_MAX;

                    //D(bug("[WiFi.RECV] Increasing wait delay from %ld to %ld\n", oldwait, waitDelay));
                }
            }

            // Fire new IORequest
            tr->tr_node.io_Command = TR_ADDREQUEST;
            tr->tr_time.tv_sec = waitDelay / 1000000;
            tr->tr_time.tv_micro = waitDelay % 1000000;
            SendIO(&tr->tr_node);
        }

        // Shutdown signal?
        if (sigSet & SIGBREAKF_CTRL_C)
        {
            D(bug("[WiFi.RECV] Quiting receiver loop\n"));
            // Abort timer IO
            AbortIO(&tr->tr_node);
            WaitIO(&tr->tr_node);
            break;
        }
            
    }

    D(bug("[WiFi.RECV] Packet receiver is closing now\n"));
    CloseDevice(&tr->tr_node);
    DeleteIORequest(&tr->tr_node);
    DeleteMsgPort(port);
}

static const char * const brcmf_fil_errstr[] = {
	"BCME_OK",
	"BCME_ERROR",
	"BCME_BADARG",
	"BCME_BADOPTION",
	"BCME_NOTUP",
	"BCME_NOTDOWN",
	"BCME_NOTAP",
	"BCME_NOTSTA",
	"BCME_BADKEYIDX",
	"BCME_RADIOOFF",
	"BCME_NOTBANDLOCKED",
	"BCME_NOCLK",
	"BCME_BADRATESET",
	"BCME_BADBAND",
	"BCME_BUFTOOSHORT",
	"BCME_BUFTOOLONG",
	"BCME_BUSY",
	"BCME_NOTASSOCIATED",
	"BCME_BADSSIDLEN",
	"BCME_OUTOFRANGECHAN",
	"BCME_BADCHAN",
	"BCME_BADADDR",
	"BCME_NORESOURCE",
	"BCME_UNSUPPORTED",
	"BCME_BADLEN",
	"BCME_NOTREADY",
	"BCME_EPERM",
	"BCME_NOMEM",
	"BCME_ASSOCIATED",
	"BCME_RANGE",
	"BCME_NOTFOUND",
	"BCME_WME_NOT_ENABLED",
	"BCME_TSPEC_NOTFOUND",
	"BCME_ACM_NOTSUPPORTED",
	"BCME_NOT_WME_ASSOCIATION",
	"BCME_SDIO_ERROR",
	"BCME_DONGLE_DOWN",
	"BCME_VERSION",
	"BCME_TXFAIL",
	"BCME_RXFAIL",
	"BCME_NODEVICE",
	"BCME_NMODE_DISABLED",
	"BCME_NONRESIDENT",
	"BCME_SCANREJECT",
	"BCME_USAGE_ERROR",
	"BCME_IOCTL_ERROR",
	"BCME_SERIAL_PORT_ERR",
	"BCME_DISABLED",
	"BCME_DECERR",
	"BCME_ENCERR",
	"BCME_MICERR",
	"BCME_REPLAY",
	"BCME_IE_NOTFOUND",
};


void PacketDump(struct SDIO *sdio, APTR data, char *src)
{
    struct ExecBase *SysBase = sdio->s_SysBase;
    struct Packet *pkt = data;
    ULONG dataLength = LE16(pkt->p_Length) - sizeof(struct Packet);

    D(bug("[%s] Packet dump: \n", (ULONG)src));
    D(bug("[%s]   Len=%04lx, ChkSum=%04lx\n", (ULONG)src, LE16(pkt->p_Length), LE16(pkt->c_ChkSum)));
    D(bug("[%s]   SEQ=%03ld, CHAN=%ld, NEXT=%ld, DATA=%ld, FLOW=%ld, MAX_SEQ=%ld\n", (ULONG)src, 
                        pkt->c_Seq, pkt->c_ChannelFlag, pkt->c_NextLength, pkt->c_DataOffset, pkt->c_FlowControl, pkt->c_MaxSeq));
    
    data = (APTR)((ULONG)data + pkt->c_DataOffset);

    if (pkt->c_ChannelFlag == 0)
    {
        struct PacketCmd *c = data;
        D(bug("[%s]   CMD=%08lx, FLAGS=%04lx, ID=%04lx, STAT=%08lx\n", (ULONG)src, LE32(c->c_Command), LE16(c->c_Flags),
            LE16(c->c_ID), LE32(c->c_Status)));
    
        if (LE16(c->c_Flags) & BCDC_DCMD_ERROR)
        {
            LONG errCode = LE32(c->c_Status);
            D(bug("[%s]   Command ended with error: %s\n", (ULONG)src, (ULONG)brcmf_fil_errstr[-errCode]));
        }

        data = (APTR)((ULONG)data + sizeof(struct PacketCmd));
        dataLength -= sizeof(struct PacketCmd);
    }
    
    UBYTE *bdata = data;

    if (dataLength > 64) dataLength = 64;

    for (int i=0; i < dataLength; i++)
    {
        if (i % 16 == 0)
            bug("[%s]  ", (ULONG)src);
        bug(" %02lx", bdata[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (dataLength % 16 != 0) bug("\n");
}

static int int_strlen(const char *c)
{
    int len = 0;
    if (!c) return 0;

    while(*c++) len++;

    return len;
}

void PacketSetVar(struct SDIO *sdio, char *varName, const void *setBuffer, int setSize)
{
    struct ExecBase *SysBase = sdio->s_SysBase;
    UBYTE *pkt = sdio->s_TXBuffer;
    
    for (int i=0; i < 64; i++) pkt[i] = 0;
    struct Packet *p = (struct Packet *)&pkt[0];
    struct PacketCmd *c = (struct PacketCmd *)&pkt[12];
    int varSize = int_strlen(varName) + 1;

    UWORD totLen = sizeof(struct Packet) + sizeof(struct PacketCmd) + varSize + setSize;
    
    p->p_Length = LE16(totLen);
    p->c_ChkSum = ~p->p_Length;
    p->c_DataOffset = sizeof(struct Packet);
    p->c_FlowControl = 0;
    p->c_Seq = sdio->s_TXSeq++;
    c->c_Command = LE32(263);
    c->c_Length = LE32(varSize + setSize);
    c->c_Flags = LE16(BCDC_DCMD_SET);
    c->c_ID = LE16(++(sdio->s_CmdID));
    c->c_Status = 0;

    CopyMem(varName, &pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)], varSize);
    CopyMem(setBuffer, &pkt[sizeof(struct Packet) + sizeof(struct PacketCmd) + varSize], setSize);

    PacketDump(sdio, p, "WiFi");
#if 0
    D(bug("[WiFi] Packet to be send: \n"));
    for (int i=0; i < LE16(p->p_Length); i++)
    {
        if (i % 16 == 0)
            bug("[WiFi]  ");
        bug(" %02lx", pkt[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (LE16(p->p_Length) % 16 != 0) bug("\n");
#endif
    sdio->SendPKT(pkt, LE16(p->p_Length), sdio);
}

void PacketSetVarInt(struct SDIO *sdio, char *varName, ULONG varValue)
{
    ULONG val = LE32(varValue);
    PacketSetVar(sdio, varName, &val, 4);
}

void PacketCmdInt(struct SDIO *sdio, ULONG cmd, ULONG cmdValue)
{
    struct ExecBase *SysBase = sdio->s_SysBase;
    UBYTE *pkt = sdio->s_TXBuffer;
    
    for (int i=0; i < 64; i++) pkt[i] = 0;
    struct Packet *p = (struct Packet *)&pkt[0];
    struct PacketCmd *c = (struct PacketCmd *)&pkt[12];

    UWORD totLen = sizeof(struct Packet) + sizeof(struct PacketCmd) + 4;
    
    p->p_Length = LE16(totLen);
    p->c_ChkSum = ~p->p_Length;
    p->c_DataOffset = sizeof(struct Packet);
    p->c_FlowControl = 0;
    p->c_Seq = sdio->s_TXSeq++;
    c->c_Command = LE32(cmd);
    c->c_Length = LE32(4);
    c->c_Flags = LE16(2);
    c->c_ID = LE16(++(sdio->s_CmdID));
    c->c_Status = 0;

    *(ULONG*)(&pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)]) = LE32(cmdValue);

    PacketDump(sdio, p, "WiFi");
    #if 0
    D(bug("[WiFi] Packet to be send: \n"));
    for (int i=0; i < LE16(p->p_Length); i++)
    {
        if (i % 16 == 0)
            bug("[WiFi]  ");
        bug(" %02lx", pkt[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (LE16(p->p_Length) % 16 != 0) bug("\n");
    #endif

    sdio->SendPKT(pkt, LE16(p->p_Length), sdio);
}

void PacketGetVar(struct SDIO *sdio, char *varName, void *getBuffer, int getSize)
{
    struct ExecBase *SysBase = sdio->s_SysBase;
    UBYTE *pkt = sdio->s_TXBuffer;
    
    for (int i=0; i < 64; i++) pkt[i] = 0;
    struct Packet *p = (struct Packet *)&pkt[0];
    struct PacketCmd *c = (struct PacketCmd *)&pkt[12];
    int varSize = int_strlen(varName) + 1;
    UWORD max = varSize;
    if (getSize > max) max = getSize;

    UWORD totLen = sizeof(struct Packet) + sizeof(struct PacketCmd) + max;
    
    p->p_Length = LE16(totLen);
    p->c_ChkSum = ~p->p_Length;
    p->c_DataOffset = sizeof(struct Packet);
    p->c_FlowControl = 0;
    p->c_Seq = sdio->s_TXSeq++;
    c->c_Command = LE32(262);
    c->c_Length = LE32(max);
    c->c_Flags = LE16(0);
    c->c_ID = LE16(++(sdio->s_CmdID));
    c->c_Status = 0;

    CopyMem(varName, &pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)], varSize);
    PacketDump(sdio, p, "WiFi");
    #if 0
    D(bug("[WiFi] Packet to be send: \n"));
    for (int i=0; i < LE16(p->p_Length); i++)
    {
        if (i % 16 == 0)
            bug("[WiFi]  ");
        bug(" %02lx", pkt[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (LE16(p->p_Length) % 16 != 0) bug("\n");
    #endif
    sdio->SendPKT(pkt, LE16(p->p_Length), sdio);
}

#define MAX_CHUNK_LEN			1400

#define DLOAD_HANDLER_VER		1	/* Downloader version */
#define DLOAD_FLAG_VER_MASK		0xf000	/* Downloader version mask */
#define DLOAD_FLAG_VER_SHIFT		12	/* Downloader version shift */

#define DL_BEGIN			0x0002
#define DL_END				0x0004

#define DL_TYPE_CLM			2

static int PacketUploadCLM(struct SDIO *sdio)
{
    struct ExecBase *SysBase = sdio->s_SysBase;

    // Check if there is CLM to be uploaded
    if (sdio->s_Chip->c_CLMBase && sdio->s_Chip->c_CLMSize)
    {
        LONG dataLen = sdio->s_Chip->c_CLMSize;
        ULONG transferred = 0;
        UBYTE *data = sdio->s_Chip->c_CLMBase;
        UWORD flag = DL_BEGIN | (DLOAD_HANDLER_VER << DLOAD_FLAG_VER_SHIFT);

        struct UploadHeader {
            UWORD flag;
            UWORD dload_type;
            ULONG len;
            ULONG crc;
            UBYTE data[];
        };

        struct UploadHeader *upload = AllocMem(sizeof(struct UploadHeader) + MAX_CHUNK_LEN, MEMF_CLEAR);

        if (upload)
        {
            // Upload CLM in chunks of size MAX_CHUNK_LEN
            do {
                ULONG transferLen; 

                if (dataLen > MAX_CHUNK_LEN) {
                    transferLen = MAX_CHUNK_LEN;
                }
                else {
                    transferLen = dataLen;
                    flag |= DL_END;
                }

                CopyMem(data, &upload->data[0], transferLen);

                upload->flag = LE16(flag);
                upload->dload_type = LE16(DL_TYPE_CLM);
                upload->len = LE32(transferLen);
                upload->crc = 0;

                PacketSetVar(sdio, "clmload", upload, sizeof(struct UploadHeader) + transferLen);


void delay_us(ULONG us, struct WiFiBase *WiFiBase)
{
    (void)WiFiBase;
    ULONG timer = LE32(*(volatile ULONG*)0xf2003004);
    ULONG end = timer + us;

    if (end < timer) {
        while (end < LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
    }
    while (end > LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
}
//delay_us(2000000, sdio->s_WiFiBase);

                transferred += transferLen;
                dataLen -= transferLen;
                data += transferLen;

                flag &= ~DL_BEGIN;
            } while (dataLen > 0);

            FreeMem(upload, sizeof(struct UploadHeader) + MAX_CHUNK_LEN);
        }

        D(bug("[WiFi] CLM upload complete. Getting status\n"));
        PacketGetVar(sdio, "clmload_status", NULL, 32);
    }
    else
    {
        D(bug("[WiFi] No CLM to upload\n"));
    }

    return 1;
}

void StartPacketReceiver(struct SDIO *sdio)
{
    struct WiFiBase *WiFiBase = sdio->s_WiFiBase;
    struct ExecBase *SysBase = sdio->s_SysBase;
    APTR entry = (APTR)PacketReceiver;
    struct Task *task;
    struct MemList *ml;
    ULONG *stack;
    char task_name[] = "WiFiPi Packet Receiver";

    D(bug("[WiFi] Starting packet receiver task\n"));

    // Get all memory we need for the receiver task
    task = AllocMem(sizeof(struct Task), MEMF_PUBLIC | MEMF_CLEAR);
    ml = AllocMem(sizeof(struct MemList) + sizeof(struct MemEntry), MEMF_PUBLIC | MEMF_CLEAR);
    stack = AllocMem(PACKET_RECV_STACKSIZE * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);

    // Prepare mem list, put task and its stack there
    ml->ml_NumEntries = 2;
    ml->ml_ME[0].me_Un.meu_Addr = task;
    ml->ml_ME[0].me_Length = sizeof(struct Task);

    ml->ml_ME[1].me_Un.meu_Addr = &stack[0];
    ml->ml_ME[1].me_Length = PACKET_RECV_STACKSIZE * sizeof(ULONG);

    // Task's UserData will contain pointer to SDIO
    task->tc_UserData = sdio;

    // Set up stack
    task->tc_SPLower = &stack[0];
    task->tc_SPUpper = &stack[PACKET_RECV_STACKSIZE];

    // Push ThisTask and SDIO on the stack
    stack = (ULONG *)task->tc_SPUpper;
    *--stack = (ULONG)FindTask(NULL);
    *--stack = (ULONG)sdio;
    task->tc_SPReg = stack;

    task->tc_Node.ln_Name = task_name;
    task->tc_Node.ln_Type = NT_TASK;
    task->tc_Node.ln_Pri = PACKET_RECV_PRIORITY;

    NewMinList((struct MinList *)&task->tc_MemEntry);
    AddHead(&task->tc_MemEntry, &ml->ml_Node);

    D(bug("[WiFi] Bringing packet receiver to life\n"));

    AddTask(task, entry, NULL);
    Wait(SIGBREAKF_CTRL_C);

    if (sdio->s_ReceiverTask)
        D(bug("[WiFi] Packet receiver up and running\n"));
    else
        D(bug("[WiFi] Packet receiver not started!\n"));

//GetVar	= 262,
//SetVar	= 263,

    PacketGetVar(sdio, "cur_etheraddr", NULL, 6);

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

    static const ULONG ev_mask[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    PacketSetVar(sdio, "event_msgs", ev_mask, 16);

    PacketCmdInt(sdio, BRCMF_C_SET_SCAN_CHANNEL_TIME, 40);
    PacketCmdInt(sdio, BRCMF_C_SET_SCAN_UNASSOC_TIME, 40);
    PacketCmdInt(sdio, BRCMF_C_SET_SCAN_PASSIVE_TIME, 120);

    PacketCmdInt(sdio, BRCMF_C_UP, 0);

    PacketGetVar(sdio, "ver", NULL, 128);
    PacketSetVarInt(sdio, "roam_off", 1);

    PacketCmdInt(sdio, BRCMF_C_SET_INFRA, 1);
    PacketCmdInt(sdio, BRCMF_C_SET_PROMISC, 0);
    PacketCmdInt(sdio, BRCMF_C_UP, 1);

void delay_us(ULONG us, struct WiFiBase *WiFiBase)
{
    (void)WiFiBase;
    ULONG timer = LE32(*(volatile ULONG*)0xf2003004);
    ULONG end = timer + us;

    if (end < timer) {
        while (end < LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
    }
    while (end > LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
}
delay_us(1000000, sdio->s_WiFiBase);

    static const UBYTE params[4+2+2+4+32+6+1+1+4*4+2+2+14*2+32+4] = {
        1,0,0,0,
        1,0,
        0x34,0x12,
        0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0xff,0xff,0xff,0xff,0xff,0xff,
        2,
        0,
        0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,
        14,0,
        1,0,
        0x01,0x2b,0x02,0x2b,0x03,0x2b,0x04,0x2b,0x05,0x2e,0x06,0x2e,0x07,0x2e,
        0x08,0x2b,0x09,0x2b,0x0a,0x2b,0x0b,0x2b,0x0c,0x2b,0x0d,0x2b,0x0e,0x2b,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };
    PacketCmdInt(sdio, 49, 0);
    PacketSetVar(sdio, "escan", params, sizeof(params));


delay_us(5000000, sdio->s_WiFiBase);
    PacketCmdInt(sdio, 49, 0);
    PacketSetVar(sdio, "escan", params, sizeof(params));


delay_us(5000000, sdio->s_WiFiBase);
    PacketCmdInt(sdio, 49, 0);
    PacketSetVar(sdio, "escan", params, sizeof(params));


#if 0

    UBYTE pkt[256];
    for (int i=0; i < 256; i++) pkt[i] = 0;
    struct Packet *p = (struct Packet *)&pkt[0];
    struct PacketCmd *c = (struct PacketCmd *)&pkt[12];
    char cmd[] = "cur_etheraddr";
    

    p->p_Length = LE16(12 + 16 + 32); //sizeof(cmd));
    p->c_ChkSum = ~p->p_Length;
    p->c_DataOffset = sizeof(struct Packet);
    p->c_FlowControl = 0;
    p->c_Seq = 0;
    c->c_Command = LE32(262);   // GetVar
    c->c_Length = LE32(32); //sizeof(cmd));     // Length
    c->c_Flags = 0;
    c->c_ID = LE16(1);
    c->c_Status = 0;
    
    CopyMem(cmd, &pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)], sizeof(cmd));
    D(bug("[WiFi] Packet: \n"));
    for (int i=0; i < LE16(p->p_Length); i++)
    {
        if (i % 16 == 0)
            bug("[WiFi]  ");
        bug(" %02lx", pkt[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (LE16(p->p_Length) % 16 != 0) bug("\n");

    sdio->SendPKT(pkt, LE16(p->p_Length), sdio);

#endif

#if 0

    for (int i=0; i < 256; i++) pkt[i] = 0;
    p->p_Length = LE16(12 + 16 + 4);
    p->c_ChkSum = ~p->p_Length;
    p->c_DataOffset = sizeof(struct Packet);
    p->c_FlowControl = 0;
    p->c_Seq = 1;
    c->c_Command = LE32(49);   // GetVar
    c->c_Length = LE32(4);     // Length
    c->c_Flags = LE16(2);
    c->c_ID = LE16(2);
    c->c_Status = 0;

    pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)] = 0;
    pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)+1] = 0;
    pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)+2] = 0;
    pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)+3] = 0;

    D(bug("[WiFi] Packet: \n"));
    for (int i=0; i < LE16(p->p_Length); i++)
    {
        if (i % 16 == 0)
            bug("[WiFi]  ");
        bug(" %02lx", pkt[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (LE16(p->p_Length) % 16 != 0) bug("\n");

    sdio->SendPKT(pkt, LE16(p->p_Length), sdio);

    for (int i=0; i < 256; i++) pkt[i] = 0;

    p->p_Length = LE16(12 + 16 + sizeof(cmd2) + sizeof(params));
    p->c_ChkSum = ~p->p_Length;
    p->c_DataOffset = sizeof(struct Packet);
    p->c_FlowControl = 0;
    p->c_Seq = 2;
    c->c_Command = LE32(263);   // SetVar
    c->c_Length = LE32(sizeof(cmd2) + sizeof(params));     // Length
    c->c_Flags = LE16(2);
    c->c_ID = LE16(3);
    c->c_Status = 0;
    
    CopyMem(cmd2, &pkt[sizeof(struct Packet) + sizeof(struct PacketCmd)], sizeof(cmd2));
    CopyMem(params, &pkt[sizeof(struct Packet) + sizeof(struct PacketCmd) + sizeof(cmd2)], sizeof(params));
    
    D(bug("[WiFi] Packet: \n"));
    for (int i=0; i < LE16(p->p_Length); i++)
    {
        if (i % 16 == 0)
            bug("[WiFi]  ");
        bug(" %02lx", pkt[i]);
        if (i % 16 == 15)
            bug("\n");
    }
    if (LE16(p->p_Length) % 16 != 0) bug("\n");

    sdio->SendPKT(pkt, LE16(p->p_Length), sdio);

    #endif
}
