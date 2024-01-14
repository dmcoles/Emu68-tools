/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <common/compiler.h>

#include "unicam.h"
#include "mbox.h"
#include "vc4-regs-unicam.h"

#define BIT(n) (UINT32_C(1) << (n))
#define u32 uint32_t
#define AARCH 32
#define GENMASK(h, l) ((~0UL - (1UL << (l)) + 1) & (~0UL >> (AARCH - 1 - (h))))

#define ARM_IO_BASE ((ULONG)UnicamBase->u_PeriphBase)
#define ARM_CSI0_BASE (ARM_IO_BASE + 0x800000)
#define ARM_CSI0_END (ARM_CSI0_BASE + 0x7FF)
#define ARM_CSI0_CLKGATE (ARM_IO_BASE + 0x802000) // 4 bytes
#define ARM_CSI1_BASE (ARM_IO_BASE + 0x801000)
#define ARM_CSI1_END (ARM_CSI1_BASE + 0x7FF)
#define ARM_CSI1_CLKGATE (ARM_IO_BASE + 0x802004) // 4 bytes
#define ARM_CM_BASE (ARM_IO_BASE + 0x101000)
#define ARM_CM_CAM0CTL (ARM_CM_BASE + 0x40)
#define ARM_CM_CAM0DIV (ARM_CM_BASE + 0x44)
#define ARM_CM_CAM1CTL (ARM_CM_BASE + 0x48)
#define ARM_CM_CAM1DIV (ARM_CM_BASE + 0x4C)
#define ARM_CM_PASSWD (0x5A << 24)

void myusleep(ULONG us)
{
    ULONG count;
    for (volatile ULONG count = us*100; count > 0; count--) {
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
        asm volatile("nop");asm volatile("nop");asm volatile("nop");
	}
}

void setup_csiclk(struct UnicamBase * UnicamBase)
{
    *(volatile ULONG *)(ARM_CM_CAM1CTL) = LE32(ARM_CM_PASSWD | (1 << 5));
    myusleep(100);
    while ((*(volatile ULONG *)(ARM_CM_CAM1CTL)) & LE32(1 << 7)) {}
    myusleep(100);
    *(volatile ULONG *)(ARM_CM_CAM1DIV) =
        LE32(ARM_CM_PASSWD | (4 << 12)); // divider , 12=100MHz on pi3 ??
    myusleep(100);
    *(volatile ULONG *)(ARM_CM_CAM1CTL) =
        LE32(ARM_CM_PASSWD | 6 | (1 << 4)); // pll? 6=plld, 5=pllc
    myusleep(100);
    while (((*(volatile ULONG *)(ARM_CM_CAM1CTL)) & LE32(1 << 7)) == 0) {}
    myusleep(100);
}

void ClockWrite(struct UnicamBase * UnicamBase, ULONG nValue)
{
    *(volatile ULONG *)(ARM_CSI1_CLKGATE) = LE32(ARM_CM_PASSWD | nValue);
}

void SetField(ULONG *pValue, ULONG nValue, ULONG nMask)
{
    ULONG nTempMask = nMask;
    while (!(nTempMask & 1)) {
        nValue <<= 1;
        nTempMask >>= 1;
    }

    *pValue = (*pValue & ~nMask) | nValue;
}

ULONG ReadReg(struct UnicamBase * UnicamBase, ULONG nOffset)
{
    ULONG temp;
    temp = LE32(*(volatile ULONG *)(ARM_CSI1_BASE + nOffset));

    return temp;
}

void WriteReg(struct UnicamBase * UnicamBase, ULONG nOffset, ULONG nValue)
{
    *(volatile ULONG *)(ARM_CSI1_BASE + nOffset) = LE32(nValue);
}

void WriteRegField(struct UnicamBase * UnicamBase, ULONG nOffset, ULONG nValue, ULONG nMask)
{
    ULONG nBuffer = ReadReg(UnicamBase, nOffset);
    SetField(&nBuffer, nValue, nMask);
    WriteReg(UnicamBase, nOffset, nBuffer);
}

void unicam_run(ULONG *address , UBYTE lanes, UBYTE datatype, ULONG width , ULONG height , UBYTE bbp, struct UnicamBase * UnicamBase)
{
    //enable power domain
    enable_unicam_domain(UnicamBase);

    //enable to clock to unicam
    setup_csiclk(UnicamBase);

    // Enable lane clocks (2 lanes)
    ClockWrite(UnicamBase, 0b010101);

    // Basic init
    WriteReg(UnicamBase, UNICAM_CTRL, UNICAM_MEM);

    // Enable analogue control, and leave in reset.
    ULONG nValue = UNICAM_AR;
    SetField(&nValue, 7, UNICAM_CTATADJ_MASK);
    SetField(&nValue, 7, UNICAM_PTATADJ_MASK);
    WriteReg(UnicamBase, UNICAM_ANA, nValue);

    myusleep(1000);

    // Come out of reset
    WriteRegField(UnicamBase, UNICAM_ANA, 0, UNICAM_AR);

    // Peripheral reset
    WriteRegField(UnicamBase, UNICAM_CTRL, 1, UNICAM_CPR);
    WriteRegField(UnicamBase, UNICAM_CTRL, 0, UNICAM_CPR);

    WriteRegField(UnicamBase, UNICAM_CTRL, 0, UNICAM_CPE);

    // Enable Rx control (CSI2 DPHY)
    nValue = ReadReg(UnicamBase, UNICAM_CTRL);
    SetField(&nValue, UNICAM_CPM_CSI2, UNICAM_CPM_MASK);
    SetField(&nValue, UNICAM_DCM_STROBE, UNICAM_DCM_MASK);

    // Packet framer timeout
    SetField(&nValue, 0xf, UNICAM_PFT_MASK);
    SetField(&nValue, 128, UNICAM_OET_MASK);
    WriteReg(UnicamBase, UNICAM_CTRL, nValue);

    WriteReg(UnicamBase, UNICAM_IHWIN, 0);
    WriteReg(UnicamBase, UNICAM_IVWIN, 0);

    // AXI bus access QoS setup
    nValue = ReadReg(UnicamBase, UNICAM_PRI);
    SetField(&nValue, 0, UNICAM_BL_MASK);
    SetField(&nValue, 0, UNICAM_BS_MASK);
    SetField(&nValue, 0xe, UNICAM_PP_MASK);
    SetField(&nValue, 8, UNICAM_NP_MASK);
    SetField(&nValue, 2, UNICAM_PT_MASK);
    SetField(&nValue, 1, UNICAM_PE);
    WriteReg(UnicamBase, UNICAM_PRI, nValue);

    WriteRegField(UnicamBase, UNICAM_ANA, 0, UNICAM_DDL);

    ULONG nLineIntFreq = height >> 2;
    nValue = UNICAM_FSIE | UNICAM_FEIE | UNICAM_IBOB;
    SetField(&nValue, nLineIntFreq >= 128 ? nLineIntFreq : 128, UNICAM_LCIE_MASK);
    WriteReg(UnicamBase, UNICAM_ICTL, nValue);
    WriteReg(UnicamBase, UNICAM_STA, UNICAM_STA_MASK_ALL);
    WriteReg(UnicamBase, UNICAM_ISTA, UNICAM_ISTA_MASK_ALL);

    WriteRegField(UnicamBase, UNICAM_CLT, 2, UNICAM_CLT1_MASK); // tclk_term_en
    WriteRegField(UnicamBase, UNICAM_CLT, 6, UNICAM_CLT2_MASK); // tclk_settle
    WriteRegField(UnicamBase, UNICAM_DLT, 2, UNICAM_DLT1_MASK); // td_term_en
    WriteRegField(UnicamBase, UNICAM_DLT, 6, UNICAM_DLT2_MASK); // ths_settle
    WriteRegField(UnicamBase, UNICAM_DLT, 0, UNICAM_DLT3_MASK); // trx_enable

    WriteRegField(UnicamBase, UNICAM_CTRL, 0, UNICAM_SOE);

    // Packet compare setup - required to avoid missing frame ends
    nValue = 0;
    SetField(&nValue, 1, UNICAM_PCE);
    SetField(&nValue, 1, UNICAM_GI);
    SetField(&nValue, 1, UNICAM_CPH);
    SetField(&nValue, 0, UNICAM_PCVC_MASK);
    SetField(&nValue, 1, UNICAM_PCDT_MASK);
    WriteReg(UnicamBase, UNICAM_CMP0, nValue);

    // Enable clock lane and set up terminations (CSI2 DPHY, non-continous clock)
    nValue = 0;
    SetField(&nValue, 1, UNICAM_CLE);
    SetField(&nValue, 1, UNICAM_CLLPE);
    WriteReg(UnicamBase, UNICAM_CLK, nValue);

    // Enable required data lanes with appropriate terminations.
    // The same value needs to be written to UNICAM_DATn registers for
    // the active lanes, and 0 for inactive ones.
    // (CSI2 DPHY, non-continous clock, 2 data lanes)
    nValue = 0;
    SetField(&nValue, 1, UNICAM_DLE);
    SetField(&nValue, 1, UNICAM_DLLPE);
    WriteReg(UnicamBase, UNICAM_DAT0, nValue);
    if (lanes == 1)
        WriteReg(UnicamBase, UNICAM_DAT1, 0);
    if (lanes == 2)
        WriteReg(UnicamBase, UNICAM_DAT1, nValue);

    WriteReg(UnicamBase, UNICAM_IBLS, width*(bbp/8));

    // Write DMA buffer address

    WriteReg(UnicamBase, UNICAM_IBSA0, (u32)(address) & ~0xC0000000 | 0xC0000000);
    WriteReg(UnicamBase, UNICAM_IBEA0,
            (ULONG)(address + (width * height * (bbp/8))) & ~0xC0000000 | 0xC0000000);

    // Set packing configuration
    ULONG nUnPack = UNICAM_PUM_NONE;
    ULONG nPack = UNICAM_PPM_NONE;

    nValue = 0;
    SetField(&nValue, nUnPack, UNICAM_PUM_MASK);
    SetField(&nValue, nPack, UNICAM_PPM_MASK);
    WriteReg(UnicamBase, UNICAM_IPIPE, nValue);

    // CSI2 mode, hardcode VC 0 for now.
    WriteReg(UnicamBase, UNICAM_IDI0, (0 << 6) | datatype);

    nValue = ReadReg(UnicamBase, UNICAM_MISC);
    SetField(&nValue, 1, UNICAM_FL0);
    SetField(&nValue, 1, UNICAM_FL1);
    WriteReg(UnicamBase, UNICAM_MISC, nValue);

    // Clear ED setup
    WriteReg(UnicamBase, UNICAM_DCS, 0);

    // Enable peripheral
    WriteRegField(UnicamBase, UNICAM_CTRL, 1, UNICAM_CPE);

    // Load image pointers
    WriteRegField(UnicamBase, UNICAM_ICTL, 1, UNICAM_LIP_MASK);
}

void unicam_stop(struct UnicamBase * UnicamBase)
{
    // Analogue lane control disable
    WriteRegField(UnicamBase, UNICAM_ANA, 1, UNICAM_DDL);

    // Stop the output engine
    WriteRegField(UnicamBase, UNICAM_CTRL, 1, UNICAM_SOE);

    // Disable the data lanes
    WriteReg(UnicamBase, UNICAM_DAT0, 0);
    WriteReg(UnicamBase, UNICAM_DAT1, 0);

    // Peripheral reset
    WriteRegField(UnicamBase, UNICAM_CTRL, 1, UNICAM_CPR);
    myusleep(50);
    WriteRegField(UnicamBase, UNICAM_CTRL, 0, UNICAM_CPR);

    // Disable peripheral
    WriteRegField(UnicamBase, UNICAM_CTRL, 0, UNICAM_CPE);

    // Disable all lane clocks
    ClockWrite(UnicamBase, 0);

    //Disable unicam power domain
    // TODO  disable_unicam_domain();
}
