#ifndef _BRCM_H
#define _BRCM_H

/* 
    This file is based on ISC-licensed Broadcom headers from Linux and 
    OpenBSD
*/

/*
 * Copyright (c) 2010 Broadcom Corporation
 */

/* 
    ISC License

    Permission to use, copy, modify, and/or distribute this software 
    for any purpose with or without fee is hereby granted, provided 
    that the above copyright notice and this permission notice appear 
    in all copies.

    THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL 
    WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
    WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
    THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR 
    CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING 
    FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
    CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
    OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* Chipcommon Core Chip IDs */
#define BRCM_CC_43143_CHIP_ID       43143
#define BRCM_CC_43235_CHIP_ID       43235
#define BRCM_CC_43236_CHIP_ID       43236
#define BRCM_CC_43238_CHIP_ID       43238
#define BRCM_CC_43241_CHIP_ID       0x4324
#define BRCM_CC_43242_CHIP_ID       43242
#define BRCM_CC_4329_CHIP_ID        0x4329
#define BRCM_CC_4330_CHIP_ID        0x4330
#define BRCM_CC_4334_CHIP_ID        0x4334
#define BRCM_CC_43340_CHIP_ID       43340
#define BRCM_CC_43341_CHIP_ID       43341
#define BRCM_CC_43362_CHIP_ID       43362
#define BRCM_CC_4335_CHIP_ID        0x4335
#define BRCM_CC_4339_CHIP_ID        0x4339
#define BRCM_CC_43430_CHIP_ID       43430
#define BRCM_CC_4345_CHIP_ID        0x4345
#define BRCM_CC_43454_CHIP_ID       43454
#define BRCM_CC_43465_CHIP_ID       43465
#define BRCM_CC_4350_CHIP_ID        0x4350
#define BRCM_CC_43525_CHIP_ID       43525
#define BRCM_CC_4354_CHIP_ID        0x4354
#define BRCM_CC_4355_CHIP_ID        0x4355
#define BRCM_CC_4356_CHIP_ID        0x4356
#define BRCM_CC_43566_CHIP_ID       43566
#define BRCM_CC_43567_CHIP_ID       43567
#define BRCM_CC_43569_CHIP_ID       43569
#define BRCM_CC_43570_CHIP_ID       43570
#define BRCM_CC_4358_CHIP_ID        0x4358
#define BRCM_CC_4359_CHIP_ID        0x4359
#define BRCM_CC_43602_CHIP_ID       43602
#define BRCM_CC_4364_CHIP_ID        0x4364
#define BRCM_CC_4365_CHIP_ID        0x4365
#define BRCM_CC_4366_CHIP_ID        0x4366
#define BRCM_CC_43664_CHIP_ID       43664
#define BRCM_CC_43666_CHIP_ID       43666
#define BRCM_CC_4371_CHIP_ID        0x4371
#define BRCM_CC_4377_CHIP_ID        0x4377
#define BRCM_CC_4378_CHIP_ID        0x4378
#define BRCM_CC_4387_CHIP_ID        0x4387
#define CY_CC_4373_CHIP_ID          0x4373
#define CY_CC_43012_CHIP_ID         43012
#define CY_CC_43439_CHIP_ID         43439
#define CY_CC_43752_CHIP_ID         43752

#define SDIOD_FBR_SIZE              0x100

/* io_en */
#define SDIO_FUNC_ENABLE_1          0x02
#define SDIO_FUNC_ENABLE_2          0x04

/* io_rdys */
#define SDIO_FUNC_READY_1           0x02
#define SDIO_FUNC_READY_2           0x04

/* intr_status */
#define INTR_STATUS_FUNC1           0x2
#define INTR_STATUS_FUNC2           0x4

/* mask of register map */
#define REG_F0_REG_MASK             0x7FF
#define REG_F1_MISC_MASK            0x1FFFF

/* function 0 vendor specific CCCR registers */

#define BIT(n) (1 << (n))

#define SDIO_CCCR_BRCM_CARDCAP                  0xf0
#define SDIO_CCCR_BRCM_CARDCAP_CMD14_SUPPORT    BIT(1)
#define SDIO_CCCR_BRCM_CARDCAP_CMD14_EXT        BIT(2)
#define SDIO_CCCR_BRCM_CARDCAP_CMD_NODEC        BIT(3)

/* Interrupt enable bits for each function */
#define SDIO_CCCR_IEN_FUNC0         BIT(0)
#define SDIO_CCCR_IEN_FUNC1         BIT(1)
#define SDIO_CCCR_IEN_FUNC2         BIT(2)

#define SDIO_CCCR_BRCM_CARDCTRL             0xf1
#define SDIO_CCCR_BRCM_CARDCTRL_WLANRESET   BIT(1)

#define SDIO_CCCR_BRCM_SEPINT           0xf2
#define SDIO_CCCR_BRCM_SEPINT_MASK      BIT(0)
#define SDIO_CCCR_BRCM_SEPINT_OE        BIT(1)
#define SDIO_CCCR_BRCM_SEPINT_ACT_HI    BIT(2)

/* function 1 miscellaneous registers */

/* sprom command and status */
#define SBSDIO_SPROM_CS             0x10000
/* sprom info register */
#define SBSDIO_SPROM_INFO           0x10001
/* sprom indirect access data byte 0 */
#define SBSDIO_SPROM_DATA_LOW       0x10002
/* sprom indirect access data byte 1 */
#define SBSDIO_SPROM_DATA_HIGH      0x10003
/* sprom indirect access addr byte 0 */
#define SBSDIO_SPROM_ADDR_LOW       0x10004
/* gpio select */
#define SBSDIO_GPIO_SELECT          0x10005
/* gpio output */
#define SBSDIO_GPIO_OUT             0x10006
/* gpio enable */
#define SBSDIO_GPIO_EN              0x10007
/* rev < 7, watermark for sdio device TX path */
#define SBSDIO_WATERMARK            0x10008
/* control busy signal generation */
#define SBSDIO_DEVICE_CTL           0x10009

/* SB Address Window Low (b15) */
#define SBSDIO_FUNC1_SBADDRLOW      0x1000A
/* SB Address Window Mid (b23:b16) */
#define SBSDIO_FUNC1_SBADDRMID      0x1000B
/* SB Address Window High (b31:b24)    */
#define SBSDIO_FUNC1_SBADDRHIGH     0x1000C
/* Frame Control (frame term/abort) */
#define SBSDIO_FUNC1_FRAMECTRL      0x1000D
/* ChipClockCSR (ALP/HT ctl/status) */
#define SBSDIO_FUNC1_CHIPCLKCSR     0x1000E
/* SdioPullUp (on cmd, d0-d2) */
#define SBSDIO_FUNC1_SDIOPULLUP     0x1000F
/* Write Frame Byte Count Low */
#define SBSDIO_FUNC1_WFRAMEBCLO     0x10019
/* Write Frame Byte Count High */
#define SBSDIO_FUNC1_WFRAMEBCHI     0x1001A
/* Read Frame Byte Count Low */
#define SBSDIO_FUNC1_RFRAMEBCLO     0x1001B
/* Read Frame Byte Count High */
#define SBSDIO_FUNC1_RFRAMEBCHI     0x1001C
/* MesBusyCtl (rev 11) */
#define SBSDIO_FUNC1_MESBUSYCTRL    0x1001D
/* Watermark for sdio device RX path */
#define SBSDIO_MESBUSY_RXFIFO_WM_MASK   0x7F
#define SBSDIO_MESBUSY_RXFIFO_WM_SHIFT  0
/* Enable busy capability for MES access */
#define SBSDIO_MESBUSYCTRL_ENAB         0x80
#define SBSDIO_MESBUSYCTRL_ENAB_SHIFT   7

/* Sdio Core Rev 12 */
#define SBSDIO_FUNC1_WAKEUPCTRL             0x1001E
#define SBSDIO_FUNC1_WCTRL_ALPWAIT_MASK     0x1
#define SBSDIO_FUNC1_WCTRL_ALPWAIT_SHIFT    0
#define SBSDIO_FUNC1_WCTRL_HTWAIT_MASK      0x2
#define SBSDIO_FUNC1_WCTRL_HTWAIT_SHIFT     1
#define SBSDIO_FUNC1_SLEEPCSR               0x1001F
#define SBSDIO_FUNC1_SLEEPCSR_KSO_MASK      0x1
#define SBSDIO_FUNC1_SLEEPCSR_KSO_SHIFT     0
#define SBSDIO_FUNC1_SLEEPCSR_KSO_EN        1
#define SBSDIO_FUNC1_SLEEPCSR_DEVON_MASK    0x2
#define SBSDIO_FUNC1_SLEEPCSR_DEVON_SHIFT   1

#define SBSDIO_FUNC1_MISC_REG_START         0x10000 /* f1 misc register start */
#define SBSDIO_FUNC1_MISC_REG_LIMIT         0x1001F /* f1 misc register end */

/* function 1 OCP space */

/* sb offset addr is <= 15 bits, 32k */
#define SBSDIO_SB_OFT_ADDR_MASK     0x07FFF
#define SBSDIO_SB_OFT_ADDR_LIMIT    0x08000
/* with b15, maps to 32-bit SB access */
#define SBSDIO_SB_ACCESS_2_4B_FLAG  0x08000

/* Address bits from SBADDR regs */
#define SBSDIO_SBWINDOW_MASK        0xffff8000

#define SDIOH_READ              0   /* Read request */
#define SDIOH_WRITE             1   /* Write request */

#define SDIOH_DATA_FIX          0   /* Fixed addressing */
#define SDIOH_DATA_INC          1   /* Incremental addressing */

/* internal return code */
#define BRCM_SUCCESS    0
#define BRCM_ERROR      1

/* Packet alignment for most efficient SDIO (can change based on platform) */
#define BRCMF_SDALIGN   BIT(6)


#define SI_ENUM_BASE_DEFAULT    0x18000000

/* Common core control flags */
#define	SICF_BIST_EN            0x8000
#define	SICF_PME_EN             0x4000
#define	SICF_CORE_BITS          0x3ffc
#define	SICF_FGC                0x0002
#define	SICF_CLOCK_EN           0x0001

/* Common core status flags */
#define	SISF_BIST_DONE          0x8000
#define	SISF_BIST_ERROR         0x4000
#define	SISF_GATED_CLK          0x2000
#define	SISF_DMA64              0x1000
#define	SISF_CORE_BITS          0x0fff

#endif /* _BRCM_H */
