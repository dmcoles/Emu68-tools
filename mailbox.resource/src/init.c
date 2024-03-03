/*
    Copyright © 2024 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/execbase.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>
#include <proto/mailbox.h>
#include <common/compiler.h>

#include "mailbox.h"

extern const char deviceName[];
extern const char deviceIdString[];
CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR DeviceTreeBase);

static ULONG mbox_recv(REGARG(uint32_t channel, "d0"), REGARG(struct MailboxBase * MailboxBase, "a6"))
{
	volatile ULONG *mbox_read = (volatile ULONG *)(MailboxBase->mb_MailBox);
	volatile ULONG *mbox_status = (volatile ULONG *)((ULONG)MailboxBase->mb_MailBox + 0x18);
	uint32_t response, status;

	do
	{
		do
		{
			status = LE32(*mbox_status);
			asm volatile("nop");
		}
		while (status & MBOX_RX_EMPTY);

		asm volatile("nop");
		response = LE32(*mbox_read);
		asm volatile("nop");
	}
	while ((response & MBOX_CHANMASK) != channel);

	return (response & ~MBOX_CHANMASK);
}

static void mbox_send(REGARG(ULONG channel, "d0"), REGARG(ULONG data, "d1"), REGARG(struct MailboxBase * MailboxBase, "a6"))
{
	volatile uint32_t *mbox_write = (uint32_t*)((uintptr_t)MailboxBase->mb_MailBox + 0x20);
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)MailboxBase->mb_MailBox + 0x18);
	uint32_t status;

	data &= ~MBOX_CHANMASK;
	data |= channel & MBOX_CHANMASK;

	do
	{
		status = LE32(*mbox_status);
		asm volatile("nop");
	}
	while (status & MBOX_TX_FULL);

	asm volatile("nop");
	*mbox_write = LE32(data);
}

static void mbox_request(REGARG(ULONG MBox, "d0"), REGARG(APTR Request, "a0"), REGARG(ULONG ReqSize, "d1"), REGARG(struct MailboxBase * MailboxBase, "a6"))
{
    struct ExecBase *SysBase = MailboxBase->mb_ExecBase;
    ULONG len = ReqSize;

    if (ReqSize > 4096-128) return;

    ObtainSemaphore(&MailboxBase->mb_Lock);

    CopyMem(Request, MailboxBase->mb_Request, ReqSize);

    CachePreDMA(MailboxBase->mb_Request, &len, 0);
    MB_Send(MBox, (ULONG)MailboxBase->mb_Request);
    MB_Recv(MBox);
    len = ReqSize;
    CachePostDMA(MailboxBase->mb_Request, &len, 0);

    CopyMem(MailboxBase->mb_Request, Request, ReqSize);

    ReleaseSemaphore(&MailboxBase->mb_Lock);
}

APTR Init(REGARG(struct ExecBase *SysBase, "a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct MailboxBase *MailboxBase = NULL;
    struct CurrentBinding binding;

    bug("[mailbox] Init\n");

    DeviceTreeBase = OpenResource("devicetree.resource");

    if (DeviceTreeBase != NULL)
    {
        APTR base_pointer = NULL;
    
        ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
        GetCurrentBinding(&binding, sizeof(binding));

        base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

        if (base_pointer != NULL)
        {
            BYTE start_on_boot = 0;
            APTR key;
            ULONG relFuncTable[MB_FUNC_COUNT + 1];
            relFuncTable[0] = (ULONG)&mbox_send;
            relFuncTable[1] = (ULONG)&mbox_recv;
            relFuncTable[2] = (ULONG)&mbox_request;
            relFuncTable[3] = (ULONG)-1;

            MailboxBase = (struct MailboxBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
            MailboxBase->mb_ExecBase = SysBase;

            MakeFunctions(MailboxBase, relFuncTable, 0);

//            UnicamBase->u_ConfigDev = binding.cb_ConfigDev;
//            UnicamBase->u_ROMBase = binding.cb_ConfigDev->cd_BoardAddr;
            MailboxBase->mb_Node.lib_Node.ln_Type = NT_RESOURCE;
            MailboxBase->mb_Node.lib_Node.ln_Pri = MB_PRIORITY;
            MailboxBase->mb_Node.lib_Node.ln_Name = (STRPTR)deviceName;
            MailboxBase->mb_Node.lib_NegSize = BASE_NEG_SIZE;
            MailboxBase->mb_Node.lib_PosSize = BASE_POS_SIZE;
            MailboxBase->mb_Node.lib_Version = MB_VERSION;
            MailboxBase->mb_Node.lib_Revision = MB_REVISION;
            MailboxBase->mb_Node.lib_IdString = (STRPTR)deviceIdString;

            InitSemaphore(&MailboxBase->mb_Lock);

            MailboxBase->mb_RequestBase = AllocMem(4096, MEMF_FAST);
            MailboxBase->mb_Request = (ULONG *)(((intptr_t)MailboxBase->mb_RequestBase + 127) & ~127);

            SumLibrary((struct Library*)MailboxBase);

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

                        const ULONG * siz = GetPropValueRecursive(key, "#size-cells", DeviceTreeBase);
                        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                        if (siz != NULL)
                            size_cells = *siz;
                        
                        if (addr != NULL)
                            address_cells = *addr;

                        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                        MailboxBase->mb_MailBox = (APTR)reg[address_cells - 1];

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

                const ULONG * siz = GetPropValueRecursive(key, "#size-cells", DeviceTreeBase);
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

                if (MailboxBase->mb_MailBox != 0) {
                    MailboxBase->mb_MailBox = (APTR)((ULONG)MailboxBase->mb_MailBox - phys_vc4 + phys_cpu);
                }

                MailboxBase->mb_PeripheralBase = (APTR)phys_cpu;

                DT_CloseKey(key);
            }

            bug("[mailbox] Periph base: %08lx, Mailbox: %08lx\n", (ULONG)MailboxBase->mb_PeripheralBase,
                                                                  (ULONG)MailboxBase->mb_MailBox);

            AddResource(MailboxBase);

            binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
            binding.cb_ConfigDev->cd_Driver = MailboxBase;
        }

        CloseLibrary((struct Library *)ExpansionBase);
    }

    return  MailboxBase;
}

static void putch(REGARG(UBYTE data, "d0"), REGARG(APTR ignore, "a3"))
{
    (void)ignore;
    *(UBYTE*)0xdeadbeef = data;
}

void kprintf(REGARG(const char * msg, "a0"), REGARG(void * args, "a1")) 
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

