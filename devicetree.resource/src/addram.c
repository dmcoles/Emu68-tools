#include <proto/devicetree.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <libraries/expansion.h>
#include <exec/memory.h>
#include <stdint.h>

#include <common/compiler.h>

static void putch(REGARG(UBYTE data, "d0"), REGARG(APTR ignore, "a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

void Add_DT_Memory(struct ExecBase *SysBase, APTR DeviceTreeBase)
{
    uint64_t base;
    uint64_t size;
    ULONG type;

    // Obtain pointer to reg property of /memory. Get base and size
    APTR property = DT_FindProperty(DT_OpenKey("/memory"), "reg");
    const ULONG *reg = DT_GetPropValue(property);
    ULONG length = DT_GetPropLen(property) / 4;
    ULONG pos = 0;
    const ULONG * addr = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "#address-cells"));
    const ULONG * siz = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/"), "#size-cells"));

    ULONG addr_len = 1;
    ULONG size_len = 1;

    if (addr)
        addr_len = *addr;
    if (siz)
        size_len = *siz;

    while (pos < length)
    {
        base = 0;
        size = 0;

        /* Get address and size of the memory block */
        for (int i = 0; i < addr_len; i++)
        {
            base = (base << 32) | reg[pos++];
        }

        for (int i = 0; i < size_len; i++)
        {
            size = (size << 32) | reg[pos++];
        }

        /* Ignore all blocks of memory above 2GB limit */
        if (base >= 0x80000000)
            continue;

        /* If block starts in ZorroIII space, find all ConfigDev and adjust base address */
        if (base >= 0x40000000)
        {
            struct Library *ExpansionBase = OpenLibrary("expansion.library", 0);

            if (ExpansionBase == NULL)
                continue;

            struct ConfigDev *cd = NULL;

            while((cd = FindConfigDev(cd, -1, -1)))
            {
                if (cd->cd_BoardSize == 0)
                    continue;
                
                if ((ULONG)cd->cd_BoardAddr < 0x40000000)
                    continue;

                if (base < (ULONG)cd->cd_BoardAddr + cd->cd_BoardSize)
                {
                    ULONG diff = (ULONG)cd->cd_BoardAddr + cd->cd_BoardSize - base;
                    base += diff;
                    size -= diff;
                }
            }

            CloseLibrary(ExpansionBase);
        }
        
        /* Trim remaining blocks to fit within 2GB space */
        if ((base + size) > 0x80000000)
        {
            size = 0x80000000 - base;
        }

        // Base was below 0x08000000? Check both ranges, below that base and above!
        if (base < 0x08000000)
        {
            // Large enough to be eventually splitted by the system?
            if (base + size > 0x08000000) {
                ULONG t1 = TypeOfMem((APTR)(ULONG)base + (0x08000000 - base) / 2);
                ULONG t2 = TypeOfMem((APTR)0x08000000 + (size - (0x08000000 - base)) / 2);

                if (t1 == 0 && t2 == 0) {
                    ULONG args[] = {
                        base,
                        base + size - 1
                    };
                    RawDoFmt("[DTREE] Adding expansion memory %08lx..%08lx\n", args, (APTR)putch, NULL);
                    AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)(ULONG)base, "expansion memory");
                }
                else {
                    if (t1 == 0) {
                        ULONG args[] = {
                            base,
                            0x07ffffff
                        };
                        RawDoFmt("[DTREE] Adding expansion memory %08lx..%08lx\n", args, (APTR)putch, NULL);
                        AddMemList(0x08000000 - base, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)(ULONG)base, "expansion memory");
                    }
                    if (t2 == 0) {
                        ULONG args[] = {
                            0x08000000,
                            base + size - 1
                        };
                        RawDoFmt("[DTREE] Adding expansion memory %08lx..%08lx\n", args, (APTR)putch, NULL);
                        AddMemList(size - (0x08000000 - base), MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)0x08000000, "expansion memory");
                    }
                }
            }
            else
            {
                // Check middle of this memory for the type
                type = TypeOfMem((APTR)(ULONG)base + size/2);

                // If TypeOfMem returned 0, then Exec has not added this block of memory yet. Do it now.
                if (type == 0) {
                    ULONG args[] = {
                        base,
                        base + size - 1
                    };
                    RawDoFmt("[DTREE] Adding expansion memory %08lx..%08lx\n", args, (APTR)putch, NULL);
                    AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)(ULONG)base, "expansion memory");
                }
            }
        }
        else {
            // Check middle of this memory for the type
            type = TypeOfMem((APTR)(ULONG)base + size/2);

            // If TypeOfMem returned 0, then Exec has not added this block of memory yet. Do it now.
            if (type == 0) {
                ULONG args[] = {
                    base,
                    base + size - 1
                };
                RawDoFmt("[DTREE] Adding expansion memory %08lx..%08lx\n", args, (APTR)putch, NULL);
                AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)(ULONG)base, "expansion memory");
            }
        }
    }   
}
