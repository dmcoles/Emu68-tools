/*
    Copyright © 2023-2024 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/execbase.h>

#if defined(__INTELLISENSE__)
#include <clib/exec_protos.h>
#include <clib/expansion_protos.h>
#else
#include <proto/exec.h>
#include <proto/expansion.h>
#endif

#include <common/compiler.h>

#include "gic.h"

#define D(x) x

extern const char deviceName[];
extern const char deviceIdString[];

int _strcmp(const char *s1, const char *s2);

asm(".globl _GetVBR\n_GetVBR: movec.l VBR, d0; rte");
asm(".globl _GetVEC\n_GetVEC: movec.l #0x1e1, d0; rte");
asm(".globl _intSetVEC\n_intSetVEC: movec.l d0, #0x1e1; rte");

asm(
    "           .globl _IntEntry\n\t.globl _IntEntryLength\n"
    "_IntEntry: movem.l   D0/D1/A0/A1/A5/A6, -(SP)          \n" // Store scratch registers, just as exec ints do
    "           move.l    D2, -(SP)                         \n"
    "           movea.l   4.w, A6                           \n" // SysBase in A6
    "_ListAddr: movea.l   0xcafebabe, A0                    \n" // A0 contains head element of the list
    
    "_loop:     move.l    (A0), D2                          \n" // Get next node into D2
    "           beq.b     _exit                             \n" // If D2 == 0, end of the list
    "           movem.l   14(A0), A1/A5                     \n" // Load is_Data to A1, is_Code to A5
    "           jsr       (A5)                              \n" // Call interrupt handler
    "           bne.b     _exit                             \n" // If Z-flag was clear, exit loop immediately
    "           movea.l   d2, a0                            \n" // Put D2 into A0 and repeat
    "           bra.b     _loop                             \n"

    "_exit:     move.l    (SP)+, D2                         \n"
    "           movem.l   (SP)+, D0/D1/A0/A1/A5/A6          \n" // Restore scratch registers and return
    "           rte                                         \n"
    "_IntEntryLength: .long . - _IntEntry                   \n"
);

extern ULONG GetVBR();
extern ULONG GetVEC();
extern void intSetVEC();
extern UBYTE IntEntry;
extern ULONG IntEntryLength;
extern UBYTE ListAddr;

static inline void SetVEC(ULONG v, struct ExecBase *_SysBase)
{
    register struct ExecBase *SysBase asm("a6") = _SysBase;
    register ULONG vec asm("d0") = v;
    register APTR func asm("a5") = intSetVEC;

    asm volatile("jsr -30(%0)"::"r"(SysBase),"r"(vec), "r"(func));
}

APTR Init(REGARG(struct ExecBase *SysBase, "a6"))
{
    struct ExpansionBase *ExpansionBase = NULL;
    struct GICBase *GICBase = NULL;
    struct CurrentBinding binding;

    D(bug("[gic] Init\n"));

    APTR base_pointer = NULL;

    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    GetCurrentBinding(&binding, sizeof(binding));

    base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    if (base_pointer != NULL)
    {
        BYTE start_on_boot = 0;
        APTR key;
        ULONG relFuncTable[GIC_FUNC_COUNT + 1];

        relFuncTable[0] = (ULONG)&L_GIC_AddIntServer;
        relFuncTable[1] = (ULONG)&L_GIC_RemIntServer;
        relFuncTable[2] = (ULONG)-1;

        GICBase = (struct GICBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
        GICBase->g_SysBase = SysBase;

        MakeFunctions(GICBase, relFuncTable, 0);

        GICBase->g_ConfigDev = binding.cb_ConfigDev;
        GICBase->g_ROMBase = binding.cb_ConfigDev->cd_BoardAddr;
        GICBase->g_Node.lib_Node.ln_Type = NT_RESOURCE;
        GICBase->g_Node.lib_Node.ln_Pri = GIC_PRIORITY;
        GICBase->g_Node.lib_Node.ln_Name = (STRPTR)deviceName;
        GICBase->g_Node.lib_NegSize = BASE_NEG_SIZE;
        GICBase->g_Node.lib_PosSize = BASE_POS_SIZE;
        GICBase->g_Node.lib_Version = GIC_VERSION;
        GICBase->g_Node.lib_Revision = GIC_REVISION;
        GICBase->g_Node.lib_IdString = (STRPTR)deviceIdString;

        NewMinList((APTR)&GICBase->g_IntServers[0]);
        NewMinList((APTR)&GICBase->g_IntServers[1]);
        NewMinList((APTR)&GICBase->g_IntServers[2]);

        // Since ObtainQuickVector call was only working in V39 and was further blocked again,
        // do it here manually. Scan user vectors (64..255) and install IRQ, FIQ and ERR vectors
        // there.
        Disable();
        
        APTR *vbr = (APTR *)Supervisor(GetVBR);
        ULONG installed = 0;
        const ULONG handlerSize = IntEntryLength;

        D(bug("[gic] CPU VBR at %08lx\n", (ULONG)vbr));

        /* Go through the VBR table */
        for (int vector = 64; vector < 256; vector++)
        {
            /* If entry in the table is not set until now (NULL), install handler there */
            if (vbr[vector] == NULL)
            {
                APTR handler = NULL;
                
                static const char * const vecNames[] = { "IRQ", "FIQ", "ERR" };

                // Get memory for handler
                handler = AllocMem(handlerSize, MEMF_PUBLIC);

                if (handler == NULL) break;

                // Copy handler code
                CopyMem(&IntEntry, handler, handlerSize);

                // Install List address within handler's code
                *(APTR *)((ULONG)handler + 2 - (ULONG)&IntEntry + &ListAddr) = &GICBase->g_IntServers[installed];

                D(bug("[gic] Installing %s handler %08lx at vector %ld\n",
                    (ULONG)vecNames[installed], (ULONG)handler, vector));

                /* Remember location in GICBase */
                GICBase->g_Handlers[installed] = handler;
                GICBase->g_Vectors[installed] = vector;
                installed++;
            }

            /* Break if all handlers are installed */
            if (installed == 3) break;
        }

        if (installed != 3)
        {
            Enable();

            bug("[gic] Not all handlers were installed!\n");
            if (GICBase->g_Handlers[0] != NULL)
                FreeMem(GICBase->g_Handlers[0], handlerSize);
            if (GICBase->g_Handlers[1] != NULL)
                FreeMem(GICBase->g_Handlers[1], handlerSize);
            if (GICBase->g_Handlers[2] != NULL)
                FreeMem(GICBase->g_Handlers[2], handlerSize);

            FreeMem(base_pointer, BASE_NEG_SIZE + BASE_POS_SIZE);
            CloseLibrary((struct Library *)ExpansionBase);
            return NULL;
        }

        /* Set obtained vectors */
        ULONG vec = GICBase->g_Vectors[0] |
                   (GICBase->g_Vectors[1] << 8) |
                   (GICBase->g_Vectors[2] << 16);
        
        SetVEC(0x40000000 | vec, SysBase);

        Enable();

        SumLibrary((struct Library *)GICBase);
        AddResource(GICBase);

        binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
        binding.cb_ConfigDev->cd_Driver = GICBase;
    }

    CloseLibrary((struct Library *)ExpansionBase);

    return GICBase;
}

static void putch(REGARG(UBYTE data, "d0"), REGARG(APTR ignore, "a3"))
{
    (void)ignore;
    *(UBYTE *)0xdeadbeef = data;
}

void kprintf(REGARG(const char *msg, "a0"), REGARG(void *args, "a1"))
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    RawDoFmt(msg, args, (APTR)putch, NULL);
}

int _strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}
