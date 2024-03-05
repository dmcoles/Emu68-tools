#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>

#include <proto/exec.h>

#include "wifipi.h"
#include "sdio.h"
#include "brcm.h"
#include "brcm_sdio.h"
#include "brcm_chipcommon.h"

#define D(x) x

/* Agent registers (common for every core) */
#define BCMA_OOB_SEL_OUT_A30		0x0100
#define BCMA_IOCTL			0x0408 /* IO control */
#define  BCMA_IOCTL_CLK			0x0001
#define  BCMA_IOCTL_FGC			0x0002
#define  BCMA_IOCTL_CORE_BITS		0x3FFC
#define  BCMA_IOCTL_PME_EN		0x4000
#define  BCMA_IOCTL_BIST_EN		0x8000
#define BCMA_IOST			0x0500 /* IO status */
#define  BCMA_IOST_CORE_BITS		0x0FFF
#define  BCMA_IOST_DMA64		0x1000
#define  BCMA_IOST_GATED_CLK		0x2000
#define  BCMA_IOST_BIST_ERROR		0x4000
#define  BCMA_IOST_BIST_DONE		0x8000
#define BCMA_RESET_CTL			0x0800
#define  BCMA_RESET_CTL_RESET		0x0001
#define BCMA_RESET_ST			0x0804

#define BCMA_NS_ROM_IOST_BOOT_DEV_MASK	0x0003
#define BCMA_NS_ROM_IOST_BOOT_DEV_NOR	0x0000
#define BCMA_NS_ROM_IOST_BOOT_DEV_NAND	0x0001
#define BCMA_NS_ROM_IOST_BOOT_DEV_ROM	0x0002

static inline void delay_us(ULONG us, struct WiFiBase *WiFiBase)
{
    (void)WiFiBase;
    ULONG timer = LE32(*(volatile ULONG*)0xf2003004);
    ULONG end = timer + us;

    if (end < timer) {
        while (end < LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
    }
    while (end > LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
}

static struct Core *brcm_chip_get_core(struct Chip *chip, UWORD coreID)
{
    struct Core *core;

    ForeachNode(&chip->c_Cores, core)
    {
        if (core->c_CoreID == coreID)
            return core;
    }
    
    return NULL;
}

static void brcm_chip_disable_arm(struct Chip *chip, UWORD id)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    struct Core *core;
    ULONG val;

    core = chip->GetCore(chip, id);
    if (!core)
        return;

    switch (id)
    {
        case BCMA_CORE_ARM_CM3:
            chip->DisableCore(chip, core, 0, 0);
            break;
        
        case BCMA_CORE_ARM_CR4:
        case BCMA_CORE_ARM_CA7:
            /* clear all IOCTL bits except HALT bit */
            val = chip->c_SDIO->Read32(core->c_WrapBase + BCMA_IOCTL, chip->c_SDIO);
            val &= ARMCR4_BCMA_IOCTL_CPUHALT;
            chip->ResetCore(chip, core, val, ARMCR4_BCMA_IOCTL_CPUHALT, ARMCR4_BCMA_IOCTL_CPUHALT);
            break;

        default:
            bug("[WiFi] chip_disable_arm unknown id: %ld\n", id);
            break;
    }
}

static void sdio_activate(struct Core *core, ULONG resetVector)
{
    ULONG reg_addr;
 
    /* clear all interrupts */
    reg_addr = core->c_BaseAddress + SD_REG(intstatus);
    core->c_Chip->c_SDIO->Write32(reg_addr, 0xFFFFFFFF, core->c_Chip->c_SDIO);

    if (resetVector)
        /* Write reset vector to address 0 */
        core->c_Chip->c_SDIO->Write32(0, resetVector, core->c_Chip->c_SDIO);
}

static void brcm_chip_cm3_set_passive(struct Chip *chip)
{
    struct Core *core;

    brcm_chip_disable_arm(chip, BCMA_CORE_ARM_CM3);
    core = chip->GetCore(chip, BCMA_CORE_80211);
    chip->ResetCore(chip, core, D11_BCMA_IOCTL_PHYRESET |
                    D11_BCMA_IOCTL_PHYCLOCKEN,
                    D11_BCMA_IOCTL_PHYCLOCKEN,
                    D11_BCMA_IOCTL_PHYCLOCKEN);
    core = chip->GetCore(chip, BCMA_CORE_INTERNAL_MEM);
    chip->ResetCore(chip, core, 0, 0, 0);
#if 0
    /* disable bank #3 remap for this device */
    if (chip->pub.chip == BRCM_CC_43430_CHIP_ID ||
        chip->pub.chip == CY_CC_43439_CHIP_ID) {
        sr = container_of(core, struct brcmf_core_priv, pub);
        brcmf_chip_core_write32(sr, SOCRAMREGOFFS(bankidx), 3);
        brcmf_chip_core_write32(sr, SOCRAMREGOFFS(bankpda), 0);
    }
#endif
}

static BOOL brcm_chip_cm3_set_active(struct Chip *chip, ULONG resetVector)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    struct Core *core;
    (void)resetVector;

    core = chip->GetCore(chip, BCMA_CORE_INTERNAL_MEM);
    if (!chip->IsCoreUp(chip, core)) {
        bug("[WiFi] SOCRAM core is down after reset?\n");
        return FALSE;
    }

    sdio_activate(chip->GetCore(chip, BCMA_CORE_SDIO_HOST), 0);

    core = chip->GetCore(chip, BCMA_CORE_ARM_CM3);
    chip->ResetCore(chip, core, 0, 0, 0);

    return TRUE;
}

struct Core *brcmf_chip_get_d11core(struct Chip *chip, UBYTE unit)
{
    struct Core *core;

    ForeachNode(&chip->c_Cores, core)
    {
        if (core->c_CoreID == BCMA_CORE_80211) {
            if (unit-- == 0)
                return core;
        }
    }
    return NULL;
}

static void brcm_chip_cr4_set_passive(struct Chip *chip)
{
    int i;
    struct Core *core;

    brcm_chip_disable_arm(chip, BCMA_CORE_ARM_CR4);

    /* Disable the cores only and let the firmware enable them.
        * Releasing reset ourselves breaks BCM4387 in weird ways.
        */
    for (i = 0; (core = brcmf_chip_get_d11core(chip, i)); i++)
        chip->DisableCore(chip, core, D11_BCMA_IOCTL_PHYRESET |
                        D11_BCMA_IOCTL_PHYCLOCKEN,
                        D11_BCMA_IOCTL_PHYCLOCKEN);
}

static BOOL brcm_chip_cr4_set_active(struct Chip *chip, ULONG resetVector)
{
    struct Core *core;

    sdio_activate(chip->GetCore(chip, BCMA_CORE_SDIO_HOST), resetVector);
    
    /* restore ARM */
    core = chip->GetCore(chip, BCMA_CORE_ARM_CR4);
    chip->ResetCore(chip, core, ARMCR4_BCMA_IOCTL_CPUHALT, 0, 0);

    return TRUE;
}

static void brcm_chip_ca7_set_passive(struct Chip *chip)
{
    struct Core *core;

    brcm_chip_disable_arm(chip, BCMA_CORE_ARM_CA7);

    core = chip->GetCore(chip, BCMA_CORE_80211);
    chip->ResetCore(chip, core, D11_BCMA_IOCTL_PHYRESET |
                    D11_BCMA_IOCTL_PHYCLOCKEN,
                    D11_BCMA_IOCTL_PHYCLOCKEN,
                    D11_BCMA_IOCTL_PHYCLOCKEN);
}

static BOOL brcm_chip_ca7_set_active(struct Chip *chip, ULONG resetVector)
{
    struct Core *core;

    sdio_activate(chip->GetCore(chip, BCMA_CORE_SDIO_HOST), resetVector);

    /* restore ARM */
    core = chip->GetCore(chip, BCMA_CORE_ARM_CA7);
    chip->ResetCore(chip, core, ARMCR4_BCMA_IOCTL_CPUHALT, 0, 0);

    return TRUE;
}

static BOOL brcm_chip_ai_iscoreup(struct Chip *chip, struct Core *core)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    ULONG regdata;
    BOOL ret;

    D(bug("[WiFi] AI:IsCoreUp(%04lx): ", core->c_CoreID));

    regdata = chip->c_SDIO->Read32(core->c_WrapBase + BCMA_IOCTL, chip->c_SDIO);
    ret = (regdata & (BCMA_IOCTL_FGC | BCMA_IOCTL_CLK)) == BCMA_IOCTL_CLK;

    regdata = chip->c_SDIO->Read32(core->c_WrapBase + BCMA_RESET_CTL, chip->c_SDIO);
    ret = ret && ((regdata & BCMA_RESET_CTL_RESET) == 0);

    D({
        if (ret) bug(" UP\n");
        else bug("DOWN\n");
    });

    return ret;
}

static void brcm_chip_ai_disablecore(struct Chip *chip, struct Core *core, ULONG preReset, ULONG reset)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    ULONG regdata;

    D(bug("[WiFi] AI:DisableCore(%04lx, %08lx, %08lx)\n", core->c_CoreID, preReset, reset));

    /* if core is already in reset, skip reset */
    regdata = chip->c_SDIO->Read32(core->c_WrapBase + BCMA_RESET_CTL, chip->c_SDIO);
    if ((regdata & BCMA_RESET_CTL_RESET) == 0)
    {
        /* configure reset */
        chip->c_SDIO->Write32(core->c_WrapBase + BCMA_IOCTL,
                    preReset | BCMA_IOCTL_FGC | BCMA_IOCTL_CLK, chip->c_SDIO);
        chip->c_SDIO->Read32(core->c_WrapBase + BCMA_IOCTL, chip->c_SDIO);

        /* put in reset */
        chip->c_SDIO->Write32(core->c_WrapBase + BCMA_RESET_CTL,
                    BCMA_RESET_CTL_RESET, chip->c_SDIO);
        
        delay_us(20, chip->c_WiFiBase);

        /* wait till reset is 1 */
        int tout = 300;
        while (tout && chip->c_SDIO->Read32(core->c_WrapBase + BCMA_RESET_CTL, chip->c_SDIO) == BCMA_RESET_CTL_RESET) {
            delay_us(100, chip->c_WiFiBase);
            tout--;
        }
    }

    /* in-reset configure */
    chip->c_SDIO->Write32(core->c_WrapBase + BCMA_IOCTL,
                reset | BCMA_IOCTL_FGC | BCMA_IOCTL_CLK, chip->c_SDIO);
    chip->c_SDIO->Read32(core->c_WrapBase + BCMA_IOCTL, chip->c_SDIO);
}

static void brcm_chip_ai_resetcore(struct Chip *chip, struct Core *core, ULONG preReset, ULONG reset, ULONG postReset)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    int count;
    struct Core *d11core2 = NULL;

    D(bug("[WiFi] AI:ResetCore(%04lx, %08lx, %08lx, %08lx)\n", core->c_CoreID, preReset, reset, postReset));

    /* special handle two D11 cores reset */
    if (core->c_CoreID == BCMA_CORE_80211) {
        #if 0
        d11core2 = brcmf_chip_get_d11core(&ci->pub, 1);
        if (d11core2) {
            brcmf_dbg(INFO, "found two d11 cores, reset both\n");
            d11priv2 = container_of(d11core2,
                        struct brcmf_core_priv, pub);
        }
        #endif
    }

    /* must disable first to work for arbitrary current core state */
    chip->DisableCore(chip, core, preReset, reset);
    if (d11core2)
        chip->DisableCore(chip, d11core2, preReset, reset);

    count = 0;
    while (chip->c_SDIO->Read32(core->c_WrapBase + BCMA_RESET_CTL, chip->c_SDIO) & BCMA_RESET_CTL_RESET)
    {
        chip->c_SDIO->Write32(core->c_WrapBase + BCMA_RESET_CTL, 0, chip->c_SDIO);
        count++;
        if (count > 50)
            break;
        delay_us(60, chip->c_WiFiBase);
    }

    if (d11core2)
    {
        count = 0;
        while (chip->c_SDIO->Read32(d11core2->c_WrapBase + BCMA_RESET_CTL, chip->c_SDIO) & BCMA_RESET_CTL_RESET)
        {
            chip->c_SDIO->Write32(d11core2->c_WrapBase + BCMA_RESET_CTL, 0, chip->c_SDIO);
            count++;
            if (count > 50)
                break;
            delay_us(60, chip->c_WiFiBase);
        }
    }

    chip->c_SDIO->Write32(core->c_WrapBase + BCMA_IOCTL, postReset | BCMA_IOCTL_CLK, chip->c_SDIO);
    chip->c_SDIO->Read32(core->c_WrapBase + BCMA_IOCTL, chip->c_SDIO);

    if (d11core2)
    {
        chip->c_SDIO->Write32(d11core2->c_WrapBase + BCMA_IOCTL, postReset | BCMA_IOCTL_CLK, chip->c_SDIO);
        chip->c_SDIO->Read32(d11core2->c_WrapBase + BCMA_IOCTL, chip->c_SDIO);
    }
}

static int brcm_chip_cores_check(struct Chip * chip)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    struct Core *core;
    UBYTE need_socram = FALSE;
    UBYTE has_socram = FALSE;
    UBYTE cpu_found = FALSE;
    int idx = 1;

    D(bug("[WiFi] Cores check\n"));

    ForeachNode(&chip->c_Cores, core)
    {
        D(bug("[WiFi] Core #%ld 0x%04lx:%03ld base 0x%08lx wrap 0x%08lx",
            idx++, core->c_CoreID, core->c_CoreREV, core->c_BaseAddress, core->c_WrapBase));

        switch (core->c_CoreID)
        {
            case BCMA_CORE_ARM_CM3:
                D(bug(" is ARM CM3, needs SOC RAM"));
                cpu_found = TRUE;
                need_socram = TRUE;
                chip->SetActive = brcm_chip_cm3_set_active;
                chip->SetPassive = brcm_chip_cm3_set_passive;
                break;

            case BCMA_CORE_INTERNAL_MEM:
                has_socram = TRUE;
                D(bug(" is SOC RAM"));
                break;

            case BCMA_CORE_ARM_CR4:
                D(bug(" is ARM_CR4"));
                cpu_found = TRUE;
                chip->SetActive = brcm_chip_cr4_set_active;
                chip->SetPassive = brcm_chip_cr4_set_passive;
                break;

            case BCMA_CORE_ARM_CA7:
                D(bug(" is ARM_CA7"));
                cpu_found = TRUE;
                chip->SetActive = brcm_chip_ca7_set_active;
                chip->SetPassive = brcm_chip_ca7_set_passive;
                break;

            default:
                break;
        }
        D(bug("\n"));
    }

    if (!cpu_found)
    {
        D(bug("[WiFi] CPU core not detected\n"));
        return FALSE;
    }

    /* check RAM core presence for ARM CM3 core */
    if (need_socram && !has_socram)
    {
        D(bug("[WiFi] RAM core not provided with ARM CM3 core\n"));
        return FALSE;
    }
    return TRUE;
}



static int sdio_buscoreprep(struct SDIO *sdio)
{
    struct ExecBase *SysBase = sdio->s_SysBase;
    UBYTE clkval, clkset, tout;
    D(bug("[WiFi] sdio_buscoreprep\n"));

    /* Try forcing SDIO core to do ALPAvail request only */
    clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ;
    sdio->BackplaneAddr(SI_ENUM_BASE_DEFAULT, sdio);
    sdio->WriteByte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, clkset, sdio);

    /* If register supported, wait for ALPAvail and then force ALP */
    /* This may take up to 15 milliseconds */
    clkval = sdio->ReadByte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, sdio);
    if ((clkval & ~SBSDIO_AVBITS) != clkset) {
        D(bug("ChipClkCSR access: wrote 0x%02lx read 0x%02lx\n",
                clkset, clkval));
        return 0;
    }

    tout = 15;
    do {
        clkval = sdio->ReadByte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, sdio);
        delay_us(1000, sdio->s_WiFiBase);
        if (--tout == 0)
        {
            
            break;
        }
    } while (!SBSDIO_ALPAV(clkval));

    if (!SBSDIO_ALPAV(clkval)) {
        D(bug("[WiFi] timed out while waiting for ALP ready\n"));
        return 0;
    }

    clkset = SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_FORCE_ALP;
    sdio->WriteByte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, clkset, sdio);
    delay_us(65, sdio->s_WiFiBase);

    /* Also, disable the extra SDIO pull-ups */
    sdio->WriteByte(SD_FUNC_BAK, SBSDIO_FUNC1_SDIOPULLUP, 0, sdio);

    return 1;
}

static ULONG brcm_chip_dmp_get_desc(struct SDIO *sdio, ULONG *eromaddr, UBYTE *type)
{
    struct ExecBase * SysBase = sdio->s_SysBase;
    ULONG val;

    /* read next descriptor */
    val = sdio->Read32(*eromaddr, sdio);
    *eromaddr += 4;

    if (!type)
        return val;

    /* determine descriptor type */
    *type = (val & DMP_DESC_TYPE_MSK);
    if ((*type & ~DMP_DESC_ADDRSIZE_GT32) == DMP_DESC_ADDRESS)
        *type = DMP_DESC_ADDRESS;

    return val;
}


static int brcm_chip_dmp_get_regaddr(struct SDIO *sdio, ULONG *eromaddr, LONG *regbase, LONG *wrapbase)
{
    struct ExecBase * SysBase = sdio->s_SysBase;
    UBYTE desc;
    ULONG val, szdesc;
    UBYTE stype, sztype, wraptype;

    *regbase = 0;
    *wrapbase = 0;

    val = brcm_chip_dmp_get_desc(sdio, eromaddr, &desc);
    if (desc == DMP_DESC_MASTER_PORT) {
        wraptype = DMP_SLAVE_TYPE_MWRAP;
    } else if (desc == DMP_DESC_ADDRESS) {
        /* revert erom address */
        *eromaddr -= 4;
        wraptype = DMP_SLAVE_TYPE_SWRAP;
    } else {
        *eromaddr -= 4;
        return 0;
    }

    do {
        /* locate address descriptor */
        do {
            val = brcm_chip_dmp_get_desc(sdio, eromaddr, &desc);
            /* unexpected table end */
            if (desc == DMP_DESC_EOT) {
                *eromaddr -= 4;
                return 0;
            }
        } while (desc != DMP_DESC_ADDRESS &&
                desc != DMP_DESC_COMPONENT);

        /* stop if we crossed current component border */
        if (desc == DMP_DESC_COMPONENT) {
            *eromaddr -= 4;
            return 0;
        }

        /* skip upper 32-bit address descriptor */
        if (val & DMP_DESC_ADDRSIZE_GT32)
            brcm_chip_dmp_get_desc(sdio, eromaddr, NULL);

        sztype = (val & DMP_SLAVE_SIZE_TYPE) >> DMP_SLAVE_SIZE_TYPE_S;

        /* next size descriptor can be skipped */
        if (sztype == DMP_SLAVE_SIZE_DESC) {
            szdesc = brcm_chip_dmp_get_desc(sdio, eromaddr, NULL);
            /* skip upper size descriptor if present */
            if (szdesc & DMP_DESC_ADDRSIZE_GT32)
                brcm_chip_dmp_get_desc(sdio, eromaddr, NULL);
        }

        /* look for 4K or 8K register regions */
        if (sztype != DMP_SLAVE_SIZE_4K &&
            sztype != DMP_SLAVE_SIZE_8K)
            continue;

        stype = (val & DMP_SLAVE_TYPE) >> DMP_SLAVE_TYPE_S;

        /* only regular slave and wrapper */
        if (*regbase == 0 && stype == DMP_SLAVE_TYPE_SLAVE)
            *regbase = val & DMP_SLAVE_ADDR_BASE;
        if (*wrapbase == 0 && stype == wraptype)
            *wrapbase = val & DMP_SLAVE_ADDR_BASE;
    } while (*regbase == 0 || *wrapbase == 0);

    return 1;
}


int brcm_chip_dmp_erom_scan(struct Chip * chip)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    struct SDIO * sdio = chip->c_SDIO;
    ULONG val;
    UBYTE desc_type = 0;
    ULONG eromaddr;
    UWORD id;
    UBYTE nmw, nsw, rev;
    ULONG base, wrap;
    int err;

    D(bug("[WiFi] EROM scan\n"));

    eromaddr = sdio->Read32(CORE_CC_REG(SI_ENUM_BASE_DEFAULT, eromptr), sdio);

    D(bug("[WiFi] EROM base addr: %08lx\n", eromaddr));

    while (desc_type != DMP_DESC_EOT)
    {
        val = brcm_chip_dmp_get_desc(sdio, &eromaddr, &desc_type);

        if (!(val & DMP_DESC_VALID))
            continue;

        if (desc_type == DMP_DESC_EMPTY)
            continue;

        /* need a component descriptor */
        if (desc_type != DMP_DESC_COMPONENT)
            continue;

        id = (val & DMP_COMP_PARTNUM) >> DMP_COMP_PARTNUM_S;

        /* next descriptor must be component as well */
        val = brcm_chip_dmp_get_desc(sdio, &eromaddr, &desc_type);
        if ((val & DMP_DESC_TYPE_MSK) != DMP_DESC_COMPONENT)
            return 0;

        /* only look at cores with master port(s) */
        nmw = (val & DMP_COMP_NUM_MWRAP) >> DMP_COMP_NUM_MWRAP_S;
        nsw = (val & DMP_COMP_NUM_SWRAP) >> DMP_COMP_NUM_SWRAP_S;
        rev = (val & DMP_COMP_REVISION) >> DMP_COMP_REVISION_S;

        /* need core with ports */
        if (nmw + nsw == 0 &&
            id != BCMA_CORE_PMU &&
            id != BCMA_CORE_GCI)
            continue;

        /* try to obtain register address info */
        err = brcm_chip_dmp_get_regaddr(sdio, &eromaddr, &base, &wrap);
        if (!err)
            continue;

        D(bug("[WiFi] Found core with id=0x%04lx, base=0x%08lx, wrap=0x%08lx\n", id, base, wrap));

        struct Core *core = AllocMem(sizeof(struct Core), MEMF_ANY);
        if (core)
        {
            core->c_Chip = chip;
            core->c_BaseAddress = base;
            core->c_WrapBase = wrap;
            core->c_CoreID = id;
            core->c_CoreREV = rev;

            AddTail((struct List *)&chip->c_Cores, (struct Node *)core);
        }
    }

    return 0;
}

int chip_setup(struct Chip *chip)
{
    struct ExecBase *SysBase = chip->c_WiFiBase->w_SysBase;
    struct Core *cc;
    struct Core *pmu;
    ULONG base;
    ULONG val;
    int ret = 1;

    cc = (struct Core *)chip->c_Cores.mlh_Head;
    base = cc->c_BaseAddress;

    /* get chipcommon capabilites */
    chip->c_Caps = chip->c_SDIO->Read32(CORE_CC_REG(base, capabilities), chip->c_SDIO);
    chip->c_CapsExt = chip->c_SDIO->Read32(CORE_CC_REG(base, capabilities_ext), chip->c_SDIO);

    D(bug("[WiFi] chip_setup\n"));
    D(bug("[WiFi] Chipcomm caps: %08lx, caps ext: %08lx\n", chip->c_Caps, chip->c_CapsExt));
#if 0
    /* get pmu caps & rev */
    pmu = brcmf_chip_get_pmu(pub); /* after reading cc_caps_ext */
    if (pub->cc_caps & CC_CAP_PMU) {
        val = chip->ops->read32(chip->ctx,
                    CORE_CC_REG(pmu->base, pmucapabilities));
        pub->pmurev = val & PCAP_REV_MASK;
        pub->pmucaps = val;
    }

    brcmf_dbg(INFO, "ccrev=%d, pmurev=%d, pmucaps=0x%x\n",
            cc->pub.rev, pub->pmurev, pub->pmucaps);
#endif
#if 0
    /* execute bus core specific setup */
    if (chip->ops->setup)
        ret = chip->ops->setup(chip->ctx, pub);
#endif

    return ret;
}

int chip_init(struct SDIO *sdio)
{
    struct ExecBase * SysBase = sdio->s_SysBase;
    struct WiFiBase * WiFiBase = sdio->s_WiFiBase;
    struct Chip *chip;

    // Get memory for the chip structure
    chip = AllocMem(sizeof(struct Chip), MEMF_CLEAR);
    
    D(bug("[WiFi] chip_init\n"));

    if (chip == NULL)
        return 0;

    // Initialize list of cores
    NewMinList(&chip->c_Cores);

    chip->c_SDIO = sdio;
    chip->c_WiFiBase = WiFiBase;

    D(bug("[WiFi] Setting block sizes for backplane and radio functions\n"));

    /* Set blocksize for function 1 to 64 bytes */
    sdio->WriteByte(SD_FUNC_CIA, SDIO_FBR_ADDR(1, 0x10), 0x40, sdio);   // Function 1 - backplane
    sdio->WriteByte(SD_FUNC_CIA, SDIO_FBR_ADDR(1, 0x11), 0x00, sdio);

    /* Set blocksize for function 2 to 512 bytes */
    sdio->WriteByte(SD_FUNC_CIA, SDIO_FBR_ADDR(2, 0x10), 0x00, sdio);    // Function 2 - radio
    sdio->WriteByte(SD_FUNC_CIA, SDIO_FBR_ADDR(2, 0x11), 0x02, sdio);

    /* Enable backplane function */
    /* Enable backplane function */
    D(bug("[WiFi] Enabling function 1 (backplane)\n"));
    sdio->WriteByte(SD_FUNC_CIA, BUS_IOEN_REG, 1 << SD_FUNC_BAK, sdio);
    do {
        D(bug("[WiFi] Waiting...\n"));
    } while(0 == (sdio->ReadByte(SD_FUNC_CIA, BUS_IORDY_REG, sdio) & (1 << SD_FUNC_BAK)));
    D(bug("[WiFi] Backplane is up\n"));

    sdio->BackplaneAddr(SI_ENUM_BASE_DEFAULT, sdio);

    /* Force PLL off until the chip is attached */
    sdio->WriteByte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ, sdio);
    UBYTE tmp = sdio->ReadByte(SD_FUNC_BAK, SBSDIO_FUNC1_CHIPCLKCSR, sdio);
    if (((tmp & ~SBSDIO_AVBITS) != (SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ))) {
        D(bug("[WiFi] Chip CLK CSR access error, wrote 0x%02lx, read 0x%02lx\n", SBSDIO_FORCE_HW_CLKREQ_OFF | SBSDIO_ALP_AVAIL_REQ, tmp));
    }

    if (!sdio_buscoreprep(sdio))
    {
        FreeMem(chip, sizeof(struct Chip));
        return 0;
    }

    ULONG id = sdio->Read32(CORE_CC_REG(SI_ENUM_BASE_DEFAULT, chipid), sdio);

    chip->c_ChipID = id & CID_ID_MASK;
    chip->c_ChipREV = (id & CID_REV_MASK) >> CID_REV_SHIFT;

    D(bug("[WiFi] Chip ID: %04lx rev %ld\n", chip->c_ChipID, chip->c_ChipREV));

    ULONG soci_type = (id & CID_TYPE_MASK) >> CID_TYPE_SHIFT;

    D(bug("[WiFi] SOCI type: %s\n", (ULONG)(soci_type == SOCI_SB ? "SB" : "AI")));

    // SOCI_AI - EROM contains information about available cores and their base addresses
    if (soci_type == SOCI_AI)
    {
        chip->IsCoreUp = brcm_chip_ai_iscoreup;
        chip->DisableCore = brcm_chip_ai_disablecore;
        chip->ResetCore = brcm_chip_ai_resetcore;
        brcm_chip_dmp_erom_scan(chip);
    }
    // SOCI_SB - force cores at fixed addresses. Actually it is most likely not really the
    // case on RaspberryPi
    else
    {
        D(bug("[WiFi] SB type SOCI not supported!\n"));
        FreeMem(chip, sizeof(struct Chip));
        return 0;
    }

    chip->GetCore = brcm_chip_get_core;

    // Check if all necessary cores were found
    if (!brcm_chip_cores_check(chip))
    {
        struct Core *core;
        while ((core = (struct Core *)RemHead((struct List *)&chip->c_Cores)))
        {
            FreeMem(core, sizeof(struct Core));
        }
        FreeMem(chip, sizeof(struct Chip));
        return 0;
    }

    chip->SetPassive(chip);

    chip_setup(chip);

}
