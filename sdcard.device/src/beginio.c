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
#include <dos/dosextens.h>

#include <devices/newstyle.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

#include <proto/exec.h>

#include "sdcard.h"

#define HEAD_COUNT      128
#define SECTOR_COUNT    64

#ifndef TD_READ64
#define TD_READ64       24
#endif

#ifndef TD_WRITE64
#define TD_WRITE64      25
#endif

#ifndef TD_FORMAT64
#define TD_FORMAT64     27
#endif

static const ULONG quick = 
    (1 << TD_CHANGENUM)     |
    (1 << TD_GETDRIVETYPE)  |
    (1 << TD_GETGEOMETRY)   |
    (1 << TD_CHANGESTATE)   |
    (1 << TD_ADDCHANGEINT)  |
    (1 << TD_REMCHANGEINT)  |
    (1 << TD_MOTOR)         |
    (1 << TD_PROTSTATUS);

const UWORD NSDSupported[] = {
        CMD_READ,
        CMD_WRITE,
        CMD_UPDATE,
        CMD_CLEAR,
        TD_CHANGENUM,
        TD_FORMAT,
        TD_GETDRIVETYPE,
        TD_GETGEOMETRY,
        TD_PROTSTATUS,
        TD_ADDCHANGEINT,
        TD_REMCHANGEINT,
        TD_CHANGESTATE,
        TD_MOTOR,
        TD_READ64,
        TD_WRITE64,
        TD_FORMAT64,
        NSCMD_DEVICEQUERY,
        NSCMD_TD_READ64,
        NSCMD_TD_WRITE64,
        NSCMD_TD_FORMAT64,
        HD_SCSICMD,
        0
    };

static inline char get_hex(int a)
{
    a &= 15;
    if (a < 10)
        return a + '0';
    else
        return a + 'A' - 10;
}

void int_handle_scsi(struct IOStdReq *io, struct SDCardBase * SDCardBase)
{
    struct IOStdReq *iostd = (struct IOStdReq *)io;
    struct SCSICmd *cmd = io->io_Data;
    struct SDCardUnit *unit = (struct SDCardUnit *)io->io_Unit;
    struct ExecBase *SysBase = unit->su_Base->sd_SysBase;
    ULONG actual;
    ULONG blocks;
    ULONG lba;
    UBYTE *data = (UBYTE *)cmd->scsi_Data;
    UBYTE *tmp1;
    UBYTE tmp2;
    UBYTE tmpbuf[8];

    switch (cmd->scsi_Command[0])
    {
        case 0x00:
            io->io_Error = 0;
            break;
        
        case 0x12: // INQUIRY
            for (int i = 0; i < cmd->scsi_Length; i++) {
                UBYTE val;

                switch (i) {
                case 0: // direct-access device
                        val = 0;
                        break;
                case 1: // non-removable medium
                        val = 0;
                        break;
                case 2: // VERSION = 0
                        val = 0;
                        break;
                case 3: // NORMACA=0, HISUP = 0, RESPONSE_DATA_FORMAT = 2
                        val = (0 << 5) | (0 << 4) | 2;
                        break;
                case 4: // ADDITIONAL_LENGTH = 44 - 4
                        val = 44 - 4;
                        break;
                default:
                        if (i == 8)
                        {
                            tmp2 = (SDCardBase->sd_CID[0] >> 16) & 0xff;
                            UBYTE **id = (UBYTE**)SDCardBase->sd_ManuID;
                            if (id[tmp2] != NULL) {
                                tmp1 = (STRPTR)id[tmp2];
                            }
                            else {
                                tmpbuf[0] = 'n';
                                tmpbuf[1] = '/';
                                tmpbuf[2] = 'a';
                                tmpbuf[3] = ' ';
                                tmpbuf[4] = '(';
                                tmpbuf[5] = get_hex(tmp2 >> 4);
                                tmpbuf[6] = get_hex(tmp2);
                                tmpbuf[7] = ')';
                                tmp1 = tmpbuf;
                            }
                        }
                        else if (i == 16) {
                            tmp1 = (UBYTE *)&SDCardBase->sd_CID[0];
                            tmp1 += 2;
                        }
                        else if (i == 36) {
                            val = (SDCardBase->sd_CID[3] >> 16) | (SDCardBase->sd_CID[2] << 16);
                        }
                        if (i >= 8 && i < 16)
                            val = tmp1[i - 8];
                        else if (i >= 16 && i < 23)
                            val = tmp1[i - 16];
                        else if (i >= 23 && i < 32) {
                            if (unit->su_UnitNum == 0)
                                val = ' ';
                            else {
                                if (i == 23)
                                    val = '/';
                                else if (i == 24)
                                    val = '0' + unit->su_UnitNum;
                                else
                                    val = ' ';
                            }
                        }
                        else if (i >= 32 && i < 36) {
                            switch (i) {
                                case 32:
                                    val = 'v';
                                    break;
                                case 33:
                                    val = '0' + ((SDCardBase->sd_CID[2] >> 20) & 0xf);
                                    break;
                                case 34:
                                    val = '.';
                                    break;
                                case 35:
                                    val = '0' + ((SDCardBase->sd_CID[2] >> 16) & 0xf);
                                    break;
                                default:
                                    val = ' ';
                                    break;
                            }
                        }
                        else if (i >= 36 && i < 44) {
                            if ((i & 1) == 0)
                                val >>= 4;
                            val = get_hex(val);
                        } else
                            val = 0;
                        break;
                }

                data[i] = val;
            }

            cmd->scsi_Actual = cmd->scsi_Length;
            io->io_Error = 0;
            break;

        case 0x08: // READ (6)
            lba = cmd->scsi_Command[1] & 0x1f;
            lba = (lba << 8) | cmd->scsi_Command[2];
            lba = (lba << 8) | cmd->scsi_Command[3];
            blocks = cmd->scsi_Command[4];

            if (lba >= unit->su_BlockCount || (lba + blocks) > unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            if (cmd->scsi_Length < blocks * 512) {
                io->io_Error = IOERR_BADLENGTH;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Read((APTR)cmd->scsi_Data, cmd->scsi_Length, unit->su_StartBlock + lba, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = HFERR_BadStatus;
                }
                else
                {
                    io->io_Error = 0;
                    cmd->scsi_Actual = actual;
                }
            }         
            break;

        case 0x28: // READ (10)
            lba = cmd->scsi_Command[2];
            lba = (lba << 8) | cmd->scsi_Command[3];
            lba = (lba << 8) | cmd->scsi_Command[4];
            lba = (lba << 8) | cmd->scsi_Command[5];
            blocks =(cmd->scsi_Command[7] << 8) | cmd->scsi_Command[8];

            if (lba >= unit->su_BlockCount || (lba + blocks) > unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            if (cmd->scsi_Length < blocks * 512) {
                io->io_Error = IOERR_BADLENGTH;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Read((APTR)cmd->scsi_Data, cmd->scsi_Length, unit->su_StartBlock + lba, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = HFERR_BadStatus;
                }
                else
                {
                    io->io_Error = 0;
                    cmd->scsi_Actual = actual;
                }
            }         
            break;

        case 0x0a: // WRITE (6)
            if (unit->su_ReadOnly) {
                io->io_Error = TDERR_WriteProt;
                iostd->io_Actual = 0;
            }
            else {
                lba = cmd->scsi_Command[1] & 0x1f;
                lba = (lba << 8) | cmd->scsi_Command[2];
                lba = (lba << 8) | cmd->scsi_Command[3];
                blocks = cmd->scsi_Command[4];

                if (lba >= unit->su_BlockCount || (lba + blocks) > unit->su_BlockCount) {
                    io->io_Error = IOERR_BADADDRESS;
                    iostd->io_Actual = 0;
                }
                if (cmd->scsi_Length < blocks * 512) {
                    io->io_Error = IOERR_BADLENGTH;
                    iostd->io_Actual = 0;
                }
                else
                {
                    actual = SDCardBase->sd_Write((APTR)cmd->scsi_Data, cmd->scsi_Length, unit->su_StartBlock + lba, SDCardBase);
                    if (actual < 0)
                    {
                        io->io_Error = HFERR_BadStatus;
                    }
                    else
                    {
                        io->io_Error = 0;
                        cmd->scsi_Actual = actual;
                    }
                }         
            }
            break;
        
        case 0x2a: // WRITE (10)
            if (unit->su_ReadOnly) {
                io->io_Error = TDERR_WriteProt;
                iostd->io_Actual = 0;
            }
            else {
                lba = cmd->scsi_Command[2];
                lba = (lba << 8) | cmd->scsi_Command[3];
                lba = (lba << 8) | cmd->scsi_Command[4];
                lba = (lba << 8) | cmd->scsi_Command[5];
                blocks =(cmd->scsi_Command[7] << 8) | cmd->scsi_Command[8];

                if (lba >= unit->su_BlockCount || (lba + blocks) > unit->su_BlockCount) {
                    io->io_Error = IOERR_BADADDRESS;
                    iostd->io_Actual = 0;
                }
                if (cmd->scsi_Length < blocks * 512) {
                    io->io_Error = IOERR_BADLENGTH;
                    iostd->io_Actual = 0;
                }
                else
                {
                    actual = SDCardBase->sd_Write((APTR)cmd->scsi_Data, cmd->scsi_Length, unit->su_StartBlock + lba, SDCardBase);
                    if (actual < 0)
                    {
                        io->io_Error = HFERR_BadStatus;
                    }
                    else
                    {
                        io->io_Error = 0;
                        cmd->scsi_Actual = actual;
                    }
                }         
            }
            break;       

        case 0x25: // READ_CAPACITY (10)
            if (cmd->scsi_CmdLength < 10)
            {
                io->io_Error = HFERR_BadStatus;
                break;
            }

            if (cmd->scsi_Command[2] != 0 || cmd->scsi_Command[3] != 0 || cmd->scsi_Command[4] != 0 || cmd->scsi_Command[5] != 0 || (cmd->scsi_Command[8] & 1))
            {
                io->io_Error = HFERR_BadStatus;
                break;
            }

            if (cmd->scsi_Length < 8)
            {
                io->io_Error = IOERR_BADLENGTH;
                break;
            }

            data[0] = ((unit->su_BlockCount - 1) >> 24) & 0xff;
            data[1] = ((unit->su_BlockCount - 1) >> 16) & 0xff;
            data[2] = ((unit->su_BlockCount - 1) >> 8) & 0xff;
            data[3] = (unit->su_BlockCount - 1) & 0xff;
            data[4] = (unit->su_Base->sd_BlockSize >> 24) & 0xff;
            data[5] = (unit->su_Base->sd_BlockSize >> 16) & 0xff;
            data[6] = (unit->su_Base->sd_BlockSize >> 8) & 0xff;
            data[7] = (unit->su_Base->sd_BlockSize) & 0xff;
            
            cmd->scsi_Actual = 8;
            io->io_Error = 0;
            
            break;

        case 0x1a: // MODE SENSE (6)
            for (int i=0; i < cmd->scsi_Length; i++)
                data[i] = 0;
            data[0] = 3 + 8 + 0x16;
            data[1] = 0; // MEDIUM TYPE
            data[2] = 0;
            data[3] = 8;
            if (unit->su_BlockCount > (1 << 24))
                blocks = 0xffffff;
            else
                blocks = unit->su_BlockCount;
            data[4] = (blocks >> 16) & 0xff;
            data[5] = (blocks >>  8) & 0xff;
            data[6] = (blocks >>  0) & 0xff;
            data[7] = 0;
            data[8] = 0;
            data[9] = 0;
            data[10] = (unit->su_Base->sd_BlockSize >> 8) & 0xff;
            data[11] = (unit->su_Base->sd_BlockSize) & 0xff;
            switch (((UWORD)cmd->scsi_Command[2] << 8) | cmd->scsi_Command[3]) {
                case 0x0300: // Format Device Mode
                    data[12] = 0x03;  // PAGE CODE
                    data[13] = 0x16;  // PAGE_LENGTH
                    data[14] = HEAD_COUNT >> 8;     // TRACKS PER ZONE 15..8
                    data[15] = HEAD_COUNT;    // TRACKS PER ZONE 7..0
                    
                    data[22] = SECTOR_COUNT >> 8;     // SECTORS PER TRACK 15..8
                    data[23] = SECTOR_COUNT;    // SECTORS PER TRACK 7..0
                    data[24] = (unit->su_Base->sd_BlockSize >> 8) & 0xff;
                    data[25] = (unit->su_Base->sd_BlockSize) & 0xff;

                    data[32] = (1 << 6) | (1 << 5);

                    cmd->scsi_Actual = data[0] + 1;
                    io->io_Error = 0;
                    break;
                case 0x0400: // Rigid Drive Geometry
                    data[12] = 0x04;  // PAGE CODE
                    data[13] = 0x16;  // PAGE LENGTH
                    data[14] = (unit->su_BlockCount / (HEAD_COUNT * SECTOR_COUNT)) >> 16;
                    data[15] = (unit->su_BlockCount / (HEAD_COUNT * SECTOR_COUNT)) >> 8;
                    data[16] = (unit->su_BlockCount / (HEAD_COUNT * SECTOR_COUNT));
                    data[17] = HEAD_COUNT;

                    cmd->scsi_Actual = cmd->scsi_Data[0] + 1;
                    io->io_Error = 0;
                    break;
                default:
                    io->io_Error = HFERR_BadStatus;
                    break;
            }
            break;

        default:
            io->io_Error = IOERR_NOCMD;
            break;
    }

    // CHECK_CONDITION status in case of error, GOOD otherwise
    if (io->io_Error) {
        cmd->scsi_Status = 0x02;
        cmd->scsi_CmdActual = 0;
    } else {
        cmd->scsi_Status = 0x00;
        cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    }
}

void int_do_io(struct IORequest *io , struct SDCardBase * SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    struct SDCardUnit *unit = (struct SDCardUnit *)io->io_Unit;
    struct IOStdReq *iostd = (struct IOStdReq *)io;

    unsigned long long off64;
    ULONG offset;
    ULONG actual;
    ULONG startblock = unit->su_StartBlock;
    ULONG endblock = unit->su_StartBlock + unit->su_BlockCount;

    /* If the IO was meant to be aborted, perform no action anymore */
    if (io->io_Error == IOERR_ABORTED) {
        return;
    }

    /* Clear any error */
    io->io_Error = 0;

    ObtainSemaphore(&SDCardBase->sd_Lock);
    SDCardBase->sd_SetLED(1, SDCardBase);

    switch (io->io_Command)
    {
        case NSCMD_DEVICEQUERY:
            if (iostd->io_Length  < sizeof(struct NSDeviceQueryResult)) {
                io->io_Error = IOERR_BADLENGTH;
            }
            else
            {
                struct NSDeviceQueryResult *nsd = (struct NSDeviceQueryResult *)iostd->io_Data;

                nsd->nsdqr_DevQueryFormat = 0;
                nsd->nsdqr_SizeAvailable = sizeof(struct NSDeviceQueryResult);
                nsd->nsdqr_SupportedCommands = SDCardBase->sd_NSDSupported;
                nsd->nsdqr_DeviceType = NSDEVTYPE_TRACKDISK;
                nsd->nsdqr_DeviceSubType = 0;

                iostd->io_Actual = sizeof(struct NSDeviceQueryResult);
            }
            break;

        case TD_ADDCHANGEINT:
            iostd->io_Actual = 0;
            break;

        case TD_REMCHANGEINT:
            iostd->io_Actual = 0;
            break;

        case TD_CHANGESTATE:
            iostd->io_Actual = 0;
            break;

        case TD_PROTSTATUS:
            iostd->io_Actual = unit->su_ReadOnly;
            break;

        case TD_CHANGENUM:
            iostd->io_Actual = 0;
            break;

        case TD_GETDRIVETYPE:
            iostd->io_Actual = DG_DIRECT_ACCESS;
            break;
        
        case TD_MOTOR:
            break;

        case TD_GETGEOMETRY:
            if (iostd->io_Length < sizeof(struct DriveGeometry)) {
                io->io_Error = IOERR_BADLENGTH;
            }
            else {
                struct DriveGeometry *g = (struct DriveGeometry *)iostd->io_Data;

                g->dg_SectorSize = SDCardBase->sd_BlockSize;
                g->dg_TotalSectors = unit->su_BlockCount;
                g->dg_TrackSectors = SECTOR_COUNT;
                g->dg_Heads = HEAD_COUNT;
                g->dg_CylSectors = SECTOR_COUNT * HEAD_COUNT;
                g->dg_Cylinders = g->dg_TotalSectors / (SECTOR_COUNT * HEAD_COUNT);
                g->dg_BufMemType = MEMF_PUBLIC;
                g->dg_DeviceType = DG_DIRECT_ACCESS;
                g->dg_Flags = 0; /* Non-removable */

                iostd->io_Actual = sizeof(struct DriveGeometry);
            }

        case CMD_CLEAR:
            iostd->io_Actual = 0;
            break;

        case CMD_UPDATE:
            iostd->io_Actual = 0;
            break;

        case CMD_READ:
            offset = iostd->io_Offset / SDCardBase->sd_BlockSize;

            if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) > unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Read(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = TDERR_NotSpecified;
                }
                else
                {
                    iostd->io_Actual = actual;
                }
            }         
            break;
        
        case TD_READ64: // Fallthrough
        case NSCMD_TD_READ64:
            off64 = iostd->io_Offset;
            off64 |= ((unsigned long long)iostd->io_Actual) << 32;
            offset = off64 >> 9;

            if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) > unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Read(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = TDERR_NotSpecified;
                }
                else
                {
                    iostd->io_Actual = actual;
                }
            }         
            break;
        
        case TD_FORMAT: // Fallthrough
        case CMD_WRITE:
            if (unit->su_ReadOnly) {
                io->io_Error = TDERR_WriteProt;
                iostd->io_Actual = 0;
            }
            else {
                offset = iostd->io_Offset / SDCardBase->sd_BlockSize;

                if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) > unit->su_BlockCount) {
                    io->io_Error = IOERR_BADADDRESS;
                    iostd->io_Actual = 0;
                }
                else
                {
                    actual = SDCardBase->sd_Write(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                    if (actual < 0)
                    {
                        io->io_Error = TDERR_NotSpecified;
                    }
                    else
                    {
                        iostd->io_Actual = actual;
                    }
                }         
            }
            break;

        case TD_FORMAT64: // Fallthrough
        case TD_WRITE64: // Fallthrough
        case NSCMD_TD_FORMAT64: // Fallthrough
        case NSCMD_TD_WRITE64:
            if (unit->su_ReadOnly) {
                io->io_Error = TDERR_WriteProt;
                iostd->io_Actual = 0;
            }
            else {
                off64 = iostd->io_Offset;
                off64 |= ((unsigned long long)iostd->io_Actual) << 32;
                offset = off64 >> 9;

                if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) > unit->su_BlockCount) {
                    io->io_Error = IOERR_BADADDRESS;
                    iostd->io_Actual = 0;
                }
                else
                {
                    actual = SDCardBase->sd_Write(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                    if (actual < 0)
                    {
                        io->io_Error = TDERR_NotSpecified;
                    }
                    else
                    {
                        iostd->io_Actual = actual;
                    }
                }         
            }
            break;
        
        case HD_SCSICMD:
            int_handle_scsi(iostd, SDCardBase);
            break;

        default:
            io->io_Error = IOERR_NOCMD;
            break;
    }

    SDCardBase->sd_SetLED(0, SDCardBase);
    ReleaseSemaphore(&SDCardBase->sd_Lock);
}

void SD_BeginIO(struct IORequest *io asm("a1"))
{
    struct SDCardBase *SDCardBase = (struct SDCardBase *)io->io_Device;
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    struct SDCardUnit *unit = (struct SDCardUnit *)io->io_Unit;
    struct IOStdReq *std = (struct IOStdReq *)io;

    io->io_Error = 0;
    io->io_Message.mn_Node.ln_Type = NT_MESSAGE;

    if (SDCardBase->sd_Verbose > 1)
    {
        ULONG args[] = {
            (ULONG)unit->su_UnitNum,
            (ULONG)io->io_Unit,
            (ULONG)io->io_Command,
            (ULONG)std->io_Length,
            (ULONG)std->io_Actual,
            (ULONG)std->io_Offset,
        };

        RawDoFmt("[brcm-sdhc:%ld] BeginIO Unit=%08lx, cmd=%ld, length=%ld, actual=%08lx, offset=%08lx\n", args, (APTR)putch, NULL);
    }

    Disable();

    /* Check if command is quick. If this is the case, process immediately */
    if (io->io_Command == NSCMD_DEVICEQUERY || 
        ((io->io_Command < 32) && ((1 << io->io_Command) & quick)))
    {
        Enable();
        switch(io->io_Command)
        {
            case NSCMD_DEVICEQUERY: /* Fallthrough */
            case TD_CHANGENUM:      /* Fallthrough */
            case TD_GETDRIVETYPE:   /* Fallthrough */
            case TD_GETNUMTRACKS:   /* Fallthrough */
            case TD_GETGEOMETRY:    /* Fallthrough */
            case TD_CHANGESTATE:    /* Fallthrough */
            case TD_ADDCHANGEINT:   /* Fallthrough */
            case TD_REMCHANGEINT:   /* Fallthrough */
            case TD_MOTOR:          /* Fallthrough */
            case TD_PROTSTATUS:
                SDCardBase->sd_DoIO(io, SDCardBase);
                break;
            default:
                io->io_Error = IOERR_NOCMD;
                break;
        }

        /* 
            The IOF_QUICK flag was cleared. It means the caller was going to wait for command 
            completion. Therefore, reply the command now.
        */
        if (!(io->io_Flags & IOF_QUICK))
            ReplyMsg(&io->io_Message);
    }
    else
    {
        /* 
            If command is slow, clear IOF_QUICK flag and put it to some internal queue. When
            done with slow command, use ReplyMsg to notify exec that the command completed.
            In such case *do not* reply the command now.
            When modifying IoRequest, do it in disabled state.
            In case of a quick command, handle it now.
        */
        io->io_Flags &= ~IOF_QUICK;
        Enable();

        /* Push the command to a queue, process it maybe in another task/process, reply when
        completed */
        PutMsg(&unit->su_Unit.unit_MsgPort, &io->io_Message);
    }
}
