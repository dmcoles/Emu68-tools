
#include <libraries/configregs.h>
#include <libraries/configvars.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/resident.h>

#include <proto/exec.h>
#include <proto/expansion.h>

extern const char deviceName[];
extern const char deviceIdString[];

static inline void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

void kprintf(const char * msg asm("a0"), void * args asm("a1")) 
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    RawDoFmt(msg, args, (APTR)putch, NULL);
}

#define bug(string, ...) \
    do { ULONG args[] = {0, __VA_ARGS__}; kprintf(string, &args[1]); } while(0)

struct Module {
    struct Node m_Node;
    struct Resident *m_Resident;
};

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct EMMCBase *EMMCBase = NULL;
    struct CurrentBinding binding;
    APTR ConfigDev;
    APTR ROMBase;
    struct Module *m;
    APTR retVal;
    ULONG foundCount = 0;

    struct List modules;

    bug("[Emu68] ROM Init\n");
    
    modules.lh_Head = (struct Node*)&modules.lh_Tail;
    modules.lh_Tail = NULL;
    modules.lh_TailPred = (struct Node*)&modules.lh_Head;

    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    GetCurrentBinding(&binding, sizeof(binding));

    ConfigDev = binding.cb_ConfigDev;
    ROMBase = binding.cb_ConfigDev->cd_BoardAddr;

    /* Scan all Resident structures in this ROM */
    bug("[Emu68] Searching for modules in ROM at %08lx\n", (LONG)ROMBase);

    for (UBYTE *addr = ROMBase + 0x1000; addr <= (UBYTE*)(ROMBase + 0x800000); addr+=2)
    {
        // If matchword (ILLEGAL opcode) is at given address, assume a match
        if (*(UWORD*)addr == RTC_MATCHWORD)
        {
            // Resident structure has to point to itself, if not, it's not a resident
            struct Resident *r = (struct Resident *)addr;

            if (r->rt_MatchTag != r)
                continue;

            m = AllocMem(sizeof(struct Module), MEMF_CLEAR);
            m->m_Node.ln_Pri = r->rt_Pri;
            m->m_Node.ln_Name = r->rt_Name;
            m->m_Resident = r;
            Enqueue(&modules, &m->m_Node);
            bug("[Emu68]   found %s\n", (ULONG)m->m_Node.ln_Name);

            foundCount++;

            // if rt_EndSkip is larger than current pointer and it points within the rom, use it
            // to move forward to next module
            if ((r->rt_EndSkip > (APTR)r) && (r->rt_EndSkip < (APTR)(ROMBase + 0x800000)))
                addr = (UBYTE *)(r->rt_EndSkip - 2);
        }
    }

    bug("[Emu68] Search done. Found %ld modules\n", foundCount);

    retVal = ROMBase;

    /* 
        Memory is scanned and modules sorted by priority. Go through them and call InitResident on every 
        of them.
    */

    while((m = (struct Module *)RemHead(&modules)))
    {
        if (!InitResident(m->m_Resident, NULL)) retVal = NULL;
        FreeMem(m, sizeof(struct Module));
    }

    binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;

    return retVal;
}
