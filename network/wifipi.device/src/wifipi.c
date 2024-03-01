#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>

#include <proto/exec.h>

#include "wifipi.h"
#include "sdio.h"

static struct Core *brcm_chip_get_core(UWORD coreID, struct Chip *chip)
{
    struct Core *core;

    ForeachNode(&chip->c_Cores, core)
    {
        if (core->c_CoreID == coreID)
            return core;
    }
    
    return NULL;
}


int chip_init(struct WiFiBase *WiFiBase)
{
    struct ExecBase * SysBase = WiFiBase->w_SysBase;
    struct Chip *chip;

    // Get memory for the chip structure
    chip = AllocMem(sizeof(struct Chip), MEMF_CLEAR);
    
    if (chip == NULL)
        return 0;

    // Initialize list of cores
    NewMinList(&chip->c_Cores);

    chip->GetCore = brcm_chip_get_core;
}
