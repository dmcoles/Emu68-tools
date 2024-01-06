/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/expansion.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include <common/compiler.h>

#include "devicetree.h"

extern UBYTE rom_end;

of_node_t * dt_build_node(of_node_t *parent, struct DeviceTreeBase *DeviceTreeBase)
{
    struct ExecBase *SysBase = DeviceTreeBase->dt_ExecBase;

    of_node_t *e = AllocMem(sizeof(of_node_t), MEMF_PUBLIC | MEMF_CLEAR);

    if (e != NULL)
    {
        if (parent != NULL)
        {
            e->on_next = parent->on_children;
            parent->on_children = e;
        }

        e->on_parent = parent;
        e->on_children = NULL;
        e->on_properties = NULL;
        e->on_name = (char *)DeviceTreeBase->dt_Data;
        DeviceTreeBase->dt_Data += (_strlen((char *)DeviceTreeBase->dt_Data) + 4) / 4;
        uint32_t tmp;

        while(1)
        {
            switch (tmp = *DeviceTreeBase->dt_Data++)
            {
                case FDT_BEGIN_NODE:
                {
                    dt_build_node(e, DeviceTreeBase);
                    break;
                }

                case FDT_PROP:
                {
                    of_property_t *p = AllocMem(sizeof(of_property_t), MEMF_PUBLIC | MEMF_CLEAR);
                    p->op_length = *DeviceTreeBase->dt_Data++;
                    p->op_name = &DeviceTreeBase->dt_Strings[*DeviceTreeBase->dt_Data++];
                    if (p->op_length)
                        p->op_value = DeviceTreeBase->dt_Data;
                    else
                        p->op_value = NULL;
                    p->op_next = e->on_properties;
                    e->on_properties = p;
                    DeviceTreeBase->dt_Data += (p->op_length + 3)/4;
                    break;
                }

                case FDT_NOP:
                    break;

                case FDT_END_NODE:
                    return e;

                default:
                    break;
            }
        }
    }

    return e;
}

of_node_t * dt_parse(void *dt, struct DeviceTreeBase *DeviceTreeBase)
{
    uint32_t token = 0;

    struct fdt_header *hdr = (struct fdt_header *)dt;

    if (hdr->magic == FDT_MAGIC)
    {
        DeviceTreeBase->dt_Strings = (char*)hdr + hdr->off_dt_strings;
        DeviceTreeBase->dt_Data = (uint32_t*)((char*)hdr + hdr->off_dt_struct);

        if (hdr->off_mem_rsvmap)
        {
            struct fdt_reserve_entry *rsrvd = (void*)((intptr_t)hdr + hdr->off_mem_rsvmap);

            while (rsrvd->address != 0 || rsrvd->size != 0) {
                rsrvd++;
            }
        }

        do
        {
            token = *DeviceTreeBase->dt_Data++;

            switch (token)
            {
                case FDT_BEGIN_NODE:
                    DeviceTreeBase->dt_Root = dt_build_node(NULL, DeviceTreeBase);
                    break;
                case FDT_PROP:
                {
                    break;
                }
                default:
                    break;
            }
        } while (token != FDT_END);
    }
    else
    {
        hdr = NULL;
    }

    return DeviceTreeBase->dt_Root;
}

APTR Init(REGARG(struct ExecBase *SysBase, "a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct CurrentBinding binding;

    APTR base_pointer = NULL;
    
    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    GetCurrentBinding(&binding, sizeof(binding));

    base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    if (base_pointer)
    {
        ULONG relFuncTable[12];
            
        relFuncTable[0] = (ULONG)&L_OpenKey;
        relFuncTable[1] = (ULONG)&L_CloseKey;
        relFuncTable[2] = (ULONG)&L_GetChild;
        relFuncTable[3] = (ULONG)&L_FindProperty;
        relFuncTable[4] = (ULONG)&L_GetProperty;
        relFuncTable[5] = (ULONG)&L_GetPropLen;
        relFuncTable[6] = (ULONG)&L_GetPropName;
        relFuncTable[7] = (ULONG)&L_GetPropValue;
        relFuncTable[8] = (ULONG)&L_GetParent;
        relFuncTable[9] = (ULONG)&L_GetKeyName;
        relFuncTable[10] = (ULONG)&L_FindPropertyRecursive;
        
        relFuncTable[11] = (ULONG)-1;

        DeviceTreeBase = (struct DeviceTreeBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
        MakeFunctions(DeviceTreeBase, relFuncTable, 0);
        
        DeviceTreeBase->dt_Node.lib_Node.ln_Type = NT_RESOURCE;
        DeviceTreeBase->dt_Node.lib_Node.ln_Pri = 120;
        DeviceTreeBase->dt_Node.lib_Node.ln_Name = (STRPTR)deviceName;

        DeviceTreeBase->dt_Node.lib_NegSize = BASE_NEG_SIZE;
        DeviceTreeBase->dt_Node.lib_PosSize = BASE_POS_SIZE;
        DeviceTreeBase->dt_Node.lib_Version = DT_VERSION;
        DeviceTreeBase->dt_Node.lib_Revision = DT_REVISION;
        DeviceTreeBase->dt_Node.lib_IdString = (STRPTR)deviceIdString;

        DeviceTreeBase->dt_ExecBase = SysBase;
        DeviceTreeBase->dt_StrNull = "(null)";

        dt_parse(&rom_end, DeviceTreeBase);

        SumLibrary((struct Library*)DeviceTreeBase);
        AddResource(DeviceTreeBase);

        /* Add non-autoconfig memory to the system */
        Add_DT_Memory(SysBase, DeviceTreeBase);

        binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
        binding.cb_ConfigDev->cd_Driver = DeviceTreeBase;
    }

    CloseLibrary((struct Library*)ExpansionBase);

    return DeviceTreeBase;
}
