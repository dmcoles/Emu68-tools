/*
    Copyright © 2023-2024 Michal Schulz <michal.schulz@gmx.de>
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
#include <common/compiler.h>

#include "unicam.h"
#include "smoothing.h"

extern const char deviceName[];
extern const char deviceIdString[];

CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR DeviceTreeBase);
CONST_STRPTR FindToken(CONST_STRPTR string, CONST_STRPTR token);
int _strcmp(const char *s1, const char *s2);

APTR Init(REGARG(struct ExecBase *SysBase, "a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct UnicamBase *UnicamBase = NULL;
    struct CurrentBinding binding;

    bug("[unicam] Init\n");

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
            BYTE use_smoothing = 0;
            BYTE use_integer_scaling = 0;
            BYTE kernel_b = 25;    // 0.25
            BYTE kernel_c = 75;    // 0.75

            APTR key;
#if UNICAM_FUNC_COUNT
            ULONG relFuncTable[UNICAM_FUNC_COUNT + 1];
            relFuncTable[0] = (ULONG)&EMMC_Open;
            relFuncTable[1] = (ULONG)&EMMC_Close;
            relFuncTable[2] = (ULONG)&EMMC_Expunge;
            relFuncTable[3] = (ULONG)&EMMC_ExtFunc;
            relFuncTable[4] = (ULONG)&EMMC_BeginIO;
            relFuncTable[5] = (ULONG)&EMMC_AbortIO;
            relFuncTable[6] = (ULONG)-1;
#endif

            UnicamBase = (struct UnicamBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
            UnicamBase->u_SysBase = SysBase;

#if UNICAM_FUNC_COUNT
            MakeFunctions(UnicamBase, relFuncTable, 0);
#endif

            UnicamBase->u_ConfigDev = binding.cb_ConfigDev;
            UnicamBase->u_ROMBase = binding.cb_ConfigDev->cd_BoardAddr;
            UnicamBase->u_Node.lib_Node.ln_Type = NT_RESOURCE;
            UnicamBase->u_Node.lib_Node.ln_Pri = UNICAM_PRIORITY;
            UnicamBase->u_Node.lib_Node.ln_Name = (STRPTR)deviceName;
            UnicamBase->u_Node.lib_NegSize = BASE_NEG_SIZE;
            UnicamBase->u_Node.lib_PosSize = BASE_POS_SIZE;
            UnicamBase->u_Node.lib_Version = UNICAM_VERSION;
            UnicamBase->u_Node.lib_Revision = UNICAM_REVISION;
            UnicamBase->u_Node.lib_IdString = (STRPTR)deviceIdString;

            UnicamBase->u_RequestBase = AllocMem(256*4, MEMF_FAST);
            UnicamBase->u_Request = (ULONG *)(((intptr_t)UnicamBase->u_RequestBase + 127) & ~127);

            UnicamBase->u_IsVC6 = 0;

            SumLibrary((struct Library*)UnicamBase);

            const char *cmdline = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/chosen"), "bootargs"));
            const char *cmd;

            if (FindToken(cmdline, "unicam.boot"))
            {
                start_on_boot = 1;
            }

            if (FindToken(cmdline, "unicam.integer"))
            {
                use_integer_scaling = 1;
            }

            if (FindToken(cmdline, "unicam.smooth"))
            {
                use_smoothing = 1;
            }

            if ((cmd = FindToken(cmdline, "unicam.b=")))
            {
                ULONG val = 0;

                for (int i=0; i < 3; i++)
                {
                    if (cmd[9 + i] < '0' || cmd[9 + i] > '9')
                        break;

                    val = val * 10 + cmd[9 + i] - '0';
                }

                if (val > 100)
                    val = 100;
                
                kernel_b = (val * 256) / 100;
            }

            if ((cmd = FindToken(cmdline, "unicam.c=")))
            {
                ULONG val = 0;

                for (int i=0; i < 3; i++)
                {
                    if (cmd[9 + i] < '0' || cmd[9 + i] > '9')
                        break;

                    val = val * 10 + cmd[9 + i] - '0';
                }

                if (val > 100)
                    val = 100;
                
                kernel_c = (val * 256) / 100;
            }

            key = DT_OpenKey("/gpu");
            if (key)
            {
                const char *comp = DT_GetPropValue(DT_FindProperty(key, "compatible"));

                if (comp != NULL)
                {
                    if (_strcmp("brcm,bcm2711-vc5", comp) == 0)
                    {
                        UnicamBase->u_IsVC6 = 1;
                    }
                }
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

                        const ULONG * siz = GetPropValueRecursive(key, "#size-cells", DeviceTreeBase);
                        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                        if (siz != NULL)
                            size_cells = *siz;
                        
                        if (addr != NULL)
                            address_cells = *addr;

                        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                        UnicamBase->u_MailBox = (APTR)reg[address_cells - 1];

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

                if (UnicamBase->u_MailBox != 0)
                    UnicamBase->u_MailBox = (APTR)((ULONG)UnicamBase->u_MailBox - phys_vc4 + phys_cpu);
                
                #if 0
                if (EMMCBase->emmc_Regs != 0)
                    EMMCBase->emmc_Regs = (APTR)((ULONG)EMMCBase->emmc_Regs - phys_vc4 + phys_cpu);
                #endif

                DT_CloseKey(key);
            }

            AddResource(UnicamBase);

            binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
            binding.cb_ConfigDev->cd_Driver = UnicamBase;
        }

        CloseLibrary((struct Library *)ExpansionBase);
    }

    return  UnicamBase;
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


CONST_STRPTR FindToken(CONST_STRPTR string, CONST_STRPTR token)
{
    CONST_STRPTR ret = NULL;

    if (string)
    {
        do {
            while (*string == ' ' || *string == '\t') {
                string++;
            }

            if (*string == 0)
                break;

            for (int i=0; token[i] != 0; i++)
            {
                if (string[i] != token[i])
                {
                    break;
                }

                if (token[i] == '=') {
                    ret = string;
                    break;
                }

                if (string[i+1] == 0 || string[i+1] == ' ' || string[i+1] == '\t') {
                    ret = string;
                    break;
                }
            }

            if (ret)
                break;

            while(*string != 0) {
                if (*string != ' ' && *string != '\t')
                    string++;
                else break;
            }

        } while(!ret && *string != 0);
    }
    return ret;
}

int _strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}
