/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* THIS CODE IS PARTIALLY BASED ON WORK OF JOHN CRONIN */
/* Copyright (C) 2013 by John Cronin <jncronin@tysos.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/memory.h>
#include <inline/alib.h>

#include <devices/trackdisk.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include <hardware/intbits.h>

#include "sdcard.h"
#include "emmc.h"
#include "sdhost.h"
#include "mbox.h"
#include "findtoken.h"

extern const char deviceName[];
extern const char deviceIdString[];

void kprintf(const char * msg asm("a0"), void * args asm("a1")) 
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    RawDoFmt(msg, args, (APTR)putch, NULL);
}

/*
    Some properties, like e.g. #size-cells, are not always available in a key, but in that case the properties
    should be searched for in the parent. The process repeats recursively until either root key is found
    or the property is found, whichever occurs first
*/
CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR DeviceTreeBase)
{
    do {
        /* Find the property first */
        APTR prop = DT_FindProperty(key, property);

        if (prop)
        {
            /* If property is found, get its value and exit */
            return DT_GetPropValue(prop);
        }
        
        /* Property was not found, go to the parent and repeat */
        key = DT_GetParent(key);
    } while (key);

    return NULL;
}

void delay(ULONG us, struct SDCardBase *SDCardBase)
{
#if 1
    ULONG timer = LE32(*(volatile ULONG*)0xf2003004);
    ULONG end = timer + us;

    if (end < timer) {
        while (end < LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
    }
    while (end > LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
#else
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    SDCardBase->sd_Port.mp_SigTask = FindTask(NULL);
    SDCardBase->sd_TimeReq.tr_time.tv_micro = us % 1000000;
    SDCardBase->sd_TimeReq.tr_time.tv_secs = us / 1000000;
    SDCardBase->sd_TimeReq.tr_node.io_Command = TR_ADDREQUEST;

    DoIO((struct IORequest *)&SDCardBase->sd_TimeReq);
#endif
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct SDCardBase *SDCardBase = NULL;
    struct CurrentBinding binding;

    bug("[brcm-sdhc] Init\n");

    DeviceTreeBase = OpenResource("devicetree.resource");

    if (DeviceTreeBase != NULL)
    {
        APTR base_pointer = NULL;
    
        ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
        GetCurrentBinding(&binding, sizeof(binding));

        base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

        if (base_pointer != NULL)
        {
            APTR key;
            ULONG relFuncTable[7];
            ULONG disabled = 0;
            
            relFuncTable[0] = (ULONG)&SD_Open;
            relFuncTable[1] = (ULONG)&SD_Close;
            relFuncTable[2] = (ULONG)&SD_Expunge;
            relFuncTable[3] = (ULONG)&SD_ExtFunc;
            relFuncTable[4] = (ULONG)&SD_BeginIO;
            relFuncTable[5] = (ULONG)&SD_AbortIO;
            relFuncTable[6] = (ULONG)-1;

            SDCardBase = (struct SDCardBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
            MakeFunctions(SDCardBase, relFuncTable, 0);

            SDCardBase->sd_ManuID[0x01] = "Panasoni";
            SDCardBase->sd_ManuID[0x02] = "Toshiba ";
            SDCardBase->sd_ManuID[0x03] = "SanDisk ";
            SDCardBase->sd_ManuID[0x08] = "SiliconP";
            SDCardBase->sd_ManuID[0x18] = "Infineon";
            SDCardBase->sd_ManuID[0x1b] = "Samsung ";
            SDCardBase->sd_ManuID[0x1c] = "Transcnd";
            SDCardBase->sd_ManuID[0x1d] = "AData   ";
            SDCardBase->sd_ManuID[0x1e] = "Transcnd";
            SDCardBase->sd_ManuID[0x1f] = "Kingston";
            SDCardBase->sd_ManuID[0x27] = "Phison  ";
            SDCardBase->sd_ManuID[0x28] = "Lexar   ";
            SDCardBase->sd_ManuID[0x30] = "SanDisk ";
            SDCardBase->sd_ManuID[0x31] = "SiliconP";
            SDCardBase->sd_ManuID[0x41] = "Kingston";
            SDCardBase->sd_ManuID[0x33] = "STMicro ";
            SDCardBase->sd_ManuID[0x6f] = "STMicro ";
            SDCardBase->sd_ManuID[0x74] = "Transcnd";
            SDCardBase->sd_ManuID[0x76] = "Patriot ";
            SDCardBase->sd_ManuID[0x82] = "Sony    ";
            SDCardBase->sd_ManuID[0x89] = "Unknown ";
            SDCardBase->sd_ManuID[0x9f] = "GoodRAM ";

            SDCardBase->sd_ConfigDev = binding.cb_ConfigDev;
            SDCardBase->sd_ROMBase = binding.cb_ConfigDev->cd_BoardAddr;

            SDCardBase->sd_RequestBase = AllocMem(256*4, MEMF_FAST);
            SDCardBase->sd_Request = (ULONG *)(((intptr_t)SDCardBase->sd_RequestBase + 127) & ~127);

            SDCardBase->sd_Device.dd_Library.lib_Node.ln_Type = NT_DEVICE;
            SDCardBase->sd_Device.dd_Library.lib_Node.ln_Pri = SDCARD_PRIORITY;
            SDCardBase->sd_Device.dd_Library.lib_Node.ln_Name = (STRPTR)deviceName;

            SDCardBase->sd_Device.dd_Library.lib_NegSize = BASE_NEG_SIZE;
            SDCardBase->sd_Device.dd_Library.lib_PosSize = BASE_POS_SIZE;
            SDCardBase->sd_Device.dd_Library.lib_Version = SDCARD_VERSION;
            SDCardBase->sd_Device.dd_Library.lib_Revision = SDCARD_REVISION;
            SDCardBase->sd_Device.dd_Library.lib_IdString = (STRPTR)deviceIdString;

            SDCardBase->sd_SysBase = SysBase;
            SDCardBase->sd_DeviceTreeBase = DeviceTreeBase;

            InitSemaphore(&SDCardBase->sd_Lock);
            SDCardBase->sd_Port.mp_Flags = PA_SIGNAL;
            SDCardBase->sd_Port.mp_SigBit = SIGBREAKB_CTRL_C;
            NewList(&SDCardBase->sd_Port.mp_MsgList);

            SDCardBase->sd_TimeReq.tr_node.io_Message.mn_ReplyPort = &SDCardBase->sd_Port;
            OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)&SDCardBase->sd_TimeReq, 0);

            SDCardBase->sd_DoIO = int_do_io;
            extern const UWORD NSDSupported[];
            SDCardBase->sd_NSDSupported = (APTR)NSDSupported;

            /* Set MBOX functions in device base */
            SDCardBase->get_clock_rate = (APTR)get_clock_rate;
            SDCardBase->set_clock_rate = (APTR)set_clock_rate;
            SDCardBase->get_clock_state = (APTR)get_clock_state;
            SDCardBase->set_clock_state = (APTR)set_clock_state;
            SDCardBase->get_power_state = (APTR)get_power_state;
            SDCardBase->set_power_state = (APTR)set_power_state;
            SDCardBase->set_led_state = (APTR)set_led_state;

            /* Set SD functions in device base */
#if USE_SDHOST
            SDCardBase->sd_PowerCycle = (APTR)sdhost_powerCycle;
            SDCardBase->sd_SetLED = (APTR)sdhost_led;
            SDCardBase->sd_GetBaseClock = (APTR)sdhost_getclock;
            SDCardBase->sd_CardInit = (APTR)sdhost_card_init;
            SDCardBase->sd_CMD = (APTR)sdhost_cmd;
            SDCardBase->sd_CMD_int = (APTR)sdhost_cmd_int;
            SDCardBase->sd_Read = (APTR)sdhost_read;
            SDCardBase->sd_Write = (APTR)sdhost_write;
#else
            SDCardBase->sd_PowerCycle = (APTR)powerCycle;
            SDCardBase->sd_SetLED = (APTR)led;
            SDCardBase->sd_GetBaseClock = (APTR)getclock;
            SDCardBase->sd_CMD = (APTR)cmd;
            SDCardBase->sd_CMD_int = (APTR)cmd_int;
            SDCardBase->sd_CardInit = (APTR)sd_card_init;
            SDCardBase->sd_Read = (APTR)sd_read;
            SDCardBase->sd_Write = (APTR)sd_write;
#endif
            SDCardBase->sd_Delay = (APTR)delay;

            SumLibrary((struct Library*)SDCardBase);

            bug("[brcm-sdhc] DeviceBase at %08lx\n", (ULONG)SDCardBase);

            const char *compatible = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "compatible"));
            if (strcmp("raspberrypi,model-zero-2-w", compatible) == 0)
            {
                bug("[brcm-sdhc] Zero2-W detected, inverting LED logic\n");
                SDCardBase->sd_SetLED = (APTR)sdhost_led_inverted;
            }

            const char *cmdline = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/chosen"), "bootargs"));
            const char *cmd;

            SDCardBase->sd_HideUnit0 = 0;
            SDCardBase->sd_ReadOnlyUnit0 = 1;

            if ((cmd = FindToken(cmdline, "sd.verbose=")))
            {
                ULONG verbose = 0;

                for (int i=0; i < 3; i++)
                {
                    if (cmd[11 + i] < '0' || cmd[11 + i] > '9')
                        break;

                    verbose = verbose * 10 + cmd[11 + i] - '0';
                }

                if (verbose > 10)
                    verbose = 10;

                bug("[brcm-sdhc] Requested verbosity level: %ld\n", verbose);

                SDCardBase->sd_Verbose = (UBYTE)verbose;
            }

            if ((cmd = FindToken(cmdline, "sd.unit0=")))
            {
                if (cmd[9] == 'r' && cmd[10] == 'o' && (cmd[11] == 0 || cmd[11] == ' ')) {
                    SDCardBase->sd_ReadOnlyUnit0 = 1;
                    SDCardBase->sd_HideUnit0 = 0;
                    bug("[brcm-sdhc] Unit 0 is read only\n");
                }
                else if (cmd[9] == 'r' && cmd[10] == 'w' && (cmd[11] == 0 || cmd[11] == ' ')) {
                    SDCardBase->sd_ReadOnlyUnit0 = 0;
                    SDCardBase->sd_HideUnit0 = 0;
                    bug("[brcm-sdhc] Unit 0 is writable\n");
                }
                else if (cmd[9] == 'o' && cmd[10] == 'f' && cmd[11] == 'f' && (cmd[12] == 0 || cmd[12] == ' ')) {
                    SDCardBase->sd_HideUnit0 = 1;
                    bug("[brcm-sdhc] Unit 0 is hidden\n");
                }
            }

            if (FindToken(cmdline, "sd.low_speed"))
            {
                bug("[brcm-sdhc] 50MHz mode disabled per command line\n");

                SDCardBase->sd_DisableHighSpeed = 1;
            }

            if ((cmd = FindToken(cmdline, "sd.clock=")))
            {
                ULONG clock = 0;

                for (int i=0; i < 3; i++)
                {
                    if (cmd[9 + i] < '0' || cmd[9 + i] > '9')
                        break;

                    clock = clock * 10 + cmd[9 + i] - '0';
                }

                if (clock > 0 && clock < 200)
                {
                    bug("[brcm-sdhc] Overclocking to %ld MHz requested\n", clock);
                    SDCardBase->sd_Overclock = 1000000 * clock;
                }
            }

            if (FindToken(cmdline, "sd.disable"))
            {
                bug("[brcm-sdhc] brcm-sdhc.device disabled by user\n");

                disabled = 1;
            }

            /* Get VC4 physical address of mailbox interface. Subsequently it will be translated to m68k physical address */
            key = DT_OpenKey("/aliases");
            if (key)
            {
                CONST_STRPTR mbox_alias = DT_GetPropValue(DT_FindProperty(key, "mailbox"));

                DT_CloseKey(key);
               
                if (mbox_alias != NULL)
                {
                    key = DT_OpenKey(mbox_alias);

                    if (key)
                    {
                        int size_cells = 1;
                        int address_cells = 1;

                        const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                        if (siz != NULL)
                            size_cells = *siz;
                        
                        if (addr != NULL)
                            address_cells = *addr;

                        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                        SDCardBase->sd_MailBox = (APTR)reg[address_cells - 1];

                        DT_CloseKey(key);
                    }
                }
                DT_CloseKey(key);
            }

            /* Open /aliases and find out the "link" to the emmc */
            key = DT_OpenKey("/aliases");
            if (key)
            {
                CONST_STRPTR mmc_alias = DT_GetPropValue(DT_FindProperty(key, "mmc"));

                DT_CloseKey(key);
               
                if (mmc_alias != NULL)
                {
                    /* Open the alias and find out the MMIO VC4 physical base address */
                    key = DT_OpenKey(mmc_alias);
                    if (key) {
                        int size_cells = 1;
                        int address_cells = 1;

                        const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                        if (siz != NULL)
                            size_cells = *siz;
                        
                        if (addr != NULL)
                            address_cells = *addr;

                        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                        SDCardBase->sd_SDHC = (APTR)reg[address_cells - 1];
                        DT_CloseKey(key);
                    }
                }               
                DT_CloseKey(key);
            }

            /* Open /aliases and find out the "link" to the sdhost */
            key = DT_OpenKey("/aliases");
            if (key)
            {
                CONST_STRPTR sdhost_alias = DT_GetPropValue(DT_FindProperty(key, "sdhost"));

                DT_CloseKey(key);
               
                if (sdhost_alias != NULL)
                {
                    /* Open the alias and find out the MMIO VC4 physical base address */
                    key = DT_OpenKey(sdhost_alias);
                    if (key) {
                        int size_cells = 1;
                        int address_cells = 1;

                        const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                        if (siz != NULL)
                            size_cells = *siz;
                        
                        if (addr != NULL)
                            address_cells = *addr;

                        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                        SDCardBase->sd_SDHOST = (APTR)reg[address_cells - 1];
                        DT_CloseKey(key);
                    }
                }               
                DT_CloseKey(key);
            }

            /* Open /soc key and learn about VC4 to CPU mapping. Use it to adjust the addresses obtained above */
            key = DT_OpenKey("/soc");
            if (key)
            {
                int size_cells = 1;
                int address_cells = 1;
                int cpu_address_cells = 1;

                const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);
                const ULONG * cpu_addr = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "#address-cells"));
            
                if (siz != NULL)
                    size_cells = *siz;
                
                if (addr != NULL)
                    address_cells = *addr;

                if (cpu_addr != NULL)
                    cpu_address_cells = *cpu_addr;

                const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "ranges"));

                ULONG phys_vc4 = reg[address_cells - 1];
                ULONG phys_cpu = reg[address_cells + cpu_address_cells - 1];

                SDCardBase->sd_MailBox = (APTR)((ULONG)SDCardBase->sd_MailBox - phys_vc4 + phys_cpu);
                SDCardBase->sd_SDHC = (APTR)((ULONG)SDCardBase->sd_SDHC - phys_vc4 + phys_cpu);
                SDCardBase->sd_SDHOST = (APTR)((ULONG)SDCardBase->sd_SDHOST - phys_vc4 + phys_cpu);

                if (SDCardBase->sd_Verbose > 0) {
                    bug("[brcm-sdhc] Mailbox at %08lx\n", (ULONG)SDCardBase->sd_MailBox);
                    bug("[brcm-sdhc] SDHC regs at %08lx\n", (ULONG)SDCardBase->sd_SDHC);
                    bug("[brcm-sdhc] SDHOST regs at %08lx\n", (ULONG)SDCardBase->sd_SDHOST);
                }

                DT_CloseKey(key);
            }

            /* If both sd_MailBox and sd_SDHC are set, everything went OK and now we can add the device */
            if (SDCardBase->sd_MailBox != NULL && SDCardBase->sd_SDHC != NULL && disabled == 0)
            {
                AddDevice((struct Device *)SDCardBase);

                /* Turn the power/act led off */
                SDCardBase->sd_SetLED(0, SDCardBase);

                /* Select AF3 on GPIOs 48..53 for SDHC */
                /* Select AF0 on GPIOs 48..53 for SDHOST */
                ULONG tmp = rd32((APTR)0xf2200000, 0x10);
                tmp &= 0x00ffffff;
#if USE_SDHOST
                tmp |= 0x24000000; //0x3f000000 - AF3, 0x24000000 - AF0;
                wr32((APTR)0xf2200000, 0x10, tmp);
                wr32((APTR)0xf2200000, 0x14, 0x924); // 0xfff - AF3, 0x924 - AF0;
#else
                tmp |= 0x3f000000; //0x3f000000 - AF3, 0x24000000 - AF0;
                wr32((APTR)0xf2200000, 0x10, tmp);
                wr32((APTR)0xf2200000, 0x14, 0xfff); // 0xfff - AF3, 0x924 - AF0;
#endif
                // LED is on GPIO29
                tmp = rd32((APTR)0xf2200000, 0x08);
                tmp &= ~(7 << 27);
                tmp |= 1 << 27;
                wr32((APTR)0xf2200000, 0x08, tmp);

                /* Enable EMMC/SDHOST clock */
                SDCardBase->set_clock_state(1, 1, SDCardBase);

                /* Initialize the card */
                if (0 == SDCardBase->sd_CardInit(SDCardBase))
                {
                    uint8_t buff[512];
                    /* Initializataion was successful. Read parition table and create the units */

                    if (SDCardBase->sd_Verbose) {
                        bug("[brcm-sdhc] Attempting to read card capacity\n");
                    }
                    SDCardBase->sd_CMD(DESELECT_CARD, 0, 1000000, SDCardBase);
                    SDCardBase->sd_CMD(SEND_CSD, SDCardBase->sd_CardRCA << 16, 1000000, SDCardBase);

                    if (!FAIL(SDCardBase)) {
                        SDCardBase->sd_UnitCount = 1;
                        SDCardBase->sd_Units[0] = AllocMem(sizeof(struct SDCardUnit), MEMF_PUBLIC | MEMF_CLEAR);
                        SDCardBase->sd_Units[0]->su_StartBlock = 0;
                        SDCardBase->sd_Units[0]->su_BlockCount = 1024 * ((SDCardBase->sd_Res1 >> 8) & 0x3fffff) + 1024;
                        SDCardBase->sd_Units[0]->su_Base = SDCardBase;
                        SDCardBase->sd_Units[0]->su_UnitNum = 0;

                        if (SDCardBase->sd_Verbose) {
                            bug("[brcm-sdhc] Reported card capacity: %ld MB (%ld sectors)\n", SDCardBase->sd_Units[0]->su_BlockCount / 2048,
                                SDCardBase->sd_Units[0]->su_BlockCount);
                        }

                        if (SDCardBase->sd_Verbose > 1)
                            bug("[brcm-sdhc] Attempting to read block at 0\n");
                        
                        ULONG ret = SDCardBase->sd_Read(buff, 512, 0, SDCardBase);
                        
                        if (SDCardBase->sd_Verbose > 1)
                            bug("[brcm-sdhc] Result %ld\n", ret);

                        if (ret > 0)
                        {
                            for (int i=0; i < 4; i++) {
                                uint8_t *b = &buff[0x1be + 16 * i];
                                ULONG p0_Type = b[4];
                                ULONG p0_Start = b[8] | (b[9] << 8) | (b[10] << 16) | (b[11] << 24);
                                ULONG p0_Len = b[12] | (b[13] << 8) | (b[14] << 16) | (b[15] << 24);

                                // Partition does exist. List it.
                                if (p0_Type != 0) {
                                    if (SDCardBase->sd_Verbose) {
                                        ULONG args[] = {
                                            i, p0_Type, p0_Start, p0_Len 
                                        };

                                        RawDoFmt("[brcm-sdhc] Partition%ld: type 0x%02lx, start 0x%08lx, length 0x%08lx\n", args, (APTR)putch, NULL);
                                    }

                                    // Partition type 0x76 (Amithlon-like) creates new virtual unit with given capacity
                                    if (p0_Type == 0x76) {
                                        struct SDCardUnit *unit = AllocMem(sizeof(struct SDCardUnit), MEMF_PUBLIC | MEMF_CLEAR);
                                        unit->su_StartBlock = p0_Start;
                                        unit->su_BlockCount = p0_Len;
                                        unit->su_Base = SDCardBase;
                                        unit->su_UnitNum = SDCardBase->sd_UnitCount;
                                        
                                        SDCardBase->sd_Units[SDCardBase->sd_UnitCount++] = unit;
                                    }
                                }
                            }
                        }
                    }

                    if (SDCardBase->sd_Verbose)
                        RawDoFmt("[brcm-sdhc] Init complete.\n", NULL, (APTR)putch, NULL);

                    /* Initialization is complete. Create all tasks for the units now */
                    for (int unit = 0; unit < SDCardBase->sd_UnitCount; unit++)
                    {
                        if (unit == 0 && SDCardBase->sd_HideUnit0)
                            continue;

                        APTR entry = (APTR)UnitTask;
                        struct Task *task = AllocMem(sizeof(struct Task), MEMF_PUBLIC | MEMF_CLEAR);
                        struct MemList *ml = AllocMem(sizeof(struct MemList) + 2*sizeof(struct MemEntry), MEMF_PUBLIC | MEMF_CLEAR);
                        ULONG *stack = AllocMem(UNIT_TASK_STACKSIZE * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
                        const char unit_name[] = "brcm-sdhc unit 0";
                        STRPTR unit_name_copy = AllocMem(sizeof(unit_name), MEMF_PUBLIC | MEMF_CLEAR);
                        CopyMem((APTR)unit_name, unit_name_copy, sizeof(unit_name));

                        ml->ml_NumEntries = 3;
                        ml->ml_ME[0].me_Un.meu_Addr = task;
                        ml->ml_ME[0].me_Length = sizeof(struct Task);

                        ml->ml_ME[1].me_Un.meu_Addr = &stack[0];
                        ml->ml_ME[1].me_Length = UNIT_TASK_STACKSIZE * sizeof(ULONG);

                        ml->ml_ME[2].me_Un.meu_Addr = unit_name_copy;
                        ml->ml_ME[2].me_Length = sizeof(unit_name);

                        unit_name_copy[sizeof(unit_name) - 2] += unit;

                        task->tc_UserData = SDCardBase->sd_Units[unit];
                        task->tc_SPLower = &stack[0];
                        task->tc_SPUpper = &stack[UNIT_TASK_STACKSIZE];
                        task->tc_SPReg = task->tc_SPUpper;

                        task->tc_Node.ln_Name = unit_name_copy;
                        task->tc_Node.ln_Type = NT_TASK;
                        task->tc_Node.ln_Pri = UNIT_TASK_PRIORITY;

                        NewList(&task->tc_MemEntry);
                        AddHead(&task->tc_MemEntry, &ml->ml_Node);

                        SDCardBase->sd_Units[unit]->su_Caller = FindTask(NULL);

                        if (unit == 0 && SDCardBase->sd_ReadOnlyUnit0)
                            SDCardBase->sd_Units[unit]->su_ReadOnly = 1;

                        AddTask(task, entry, NULL);
                        Wait(SIGBREAKF_CTRL_C);
                    }

                }
                
                binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
                binding.cb_ConfigDev->cd_Driver = SDCardBase;
            }
            else
            {
                /*  
                    Something failed, device will not be added to the system, free allocated memory and 
                    return NULL instead
                */
                CloseDevice((struct IORequest *)&SDCardBase->sd_TimeReq);
                FreeMem(base_pointer, BASE_NEG_SIZE + BASE_POS_SIZE);
                SDCardBase = NULL;
            }
        }

        CloseLibrary((struct Library*)ExpansionBase);
    }

    //SDCardBase->sd_Interrupt.is_Code = (APTR)sdhost_irq_gate;
    //AddIntServer(INTB_EXTER, &SDCardBase->sd_Interrupt);

    //wr32((APTR)0xf3000000, 0x34, 0x30124f80);

    return SDCardBase;
}
