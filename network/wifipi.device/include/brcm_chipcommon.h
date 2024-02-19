// SPDX-License-Identifier: ISC
/*
 * Copyright (c) 2010 Broadcom Corporation
 */

#ifndef	_SBCHIPC_H
#define	_SBCHIPC_H

#include <exec/types.h>

/* cpp contortions to concatenate w/arg prescan */
#ifndef	PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif

#define CHIPCREGOFFS(field)	__builtin_offsetof(struct chipcregs, field)
#define CORE_CC_REG(base, field) \
		(base + __builtin_offsetof(struct chipcregs, field))

struct chipcregs {
	ULONG chipid;		/* 0x0 */
	ULONG capabilities;
	ULONG corecontrol;	/* corerev >= 1 */
	ULONG bist;

	/* OTP */
	ULONG otpstatus;	/* 0x10, corerev >= 10 */
	ULONG otpcontrol;
	ULONG otpprog;
	ULONG otplayout;	/* corerev >= 23 */

	/* Interrupt control */
	ULONG intstatus;	/* 0x20 */
	ULONG intmask;

	/* Chip specific regs */
	ULONG chipcontrol;	/* 0x28, rev >= 11 */
	ULONG chipstatus;	/* 0x2c, rev >= 11 */

	/* Jtag Master */
	ULONG jtagcmd;		/* 0x30, rev >= 10 */
	ULONG jtagir;
	ULONG jtagdr;
	ULONG jtagctrl;

	/* serial flash interface registers */
	ULONG flashcontrol;	/* 0x40 */
	ULONG flashaddress;
	ULONG flashdata;
	ULONG PAD[1];

	/* Silicon backplane configuration broadcast control */
	ULONG broadcastaddress;	/* 0x50 */
	ULONG broadcastdata;

	/* gpio - cleared only by power-on-reset */
	ULONG gpiopullup;	/* 0x58, corerev >= 20 */
	ULONG gpiopulldown;	/* 0x5c, corerev >= 20 */
	ULONG gpioin;		/* 0x60 */
	ULONG gpioout;		/* 0x64 */
	ULONG gpioouten;	/* 0x68 */
	ULONG gpiocontrol;	/* 0x6C */
	ULONG gpiointpolarity;	/* 0x70 */
	ULONG gpiointmask;	/* 0x74 */

	/* GPIO events corerev >= 11 */
	ULONG gpioevent;
	ULONG gpioeventintmask;

	/* Watchdog timer */
	ULONG watchdog;	/* 0x80 */

	/* GPIO events corerev >= 11 */
	ULONG gpioeventintpolarity;

	/* GPIO based LED powersave registers corerev >= 16 */
	ULONG gpiotimerval;	/* 0x88 */
	ULONG gpiotimeroutmask;

	/* clock control */
	ULONG clockcontrol_n;	/* 0x90 */
	ULONG clockcontrol_sb;	/* aka m0 */
	ULONG clockcontrol_pci;	/* aka m1 */
	ULONG clockcontrol_m2;	/* mii/uart/mipsref */
	ULONG clockcontrol_m3;	/* cpu */
	ULONG clkdiv;		/* corerev >= 3 */
	ULONG gpiodebugsel;	/* corerev >= 28 */
	ULONG capabilities_ext;	/* 0xac  */

	/* pll delay registers (corerev >= 4) */
	ULONG pll_on_delay;	/* 0xb0 */
	ULONG fref_sel_delay;
	ULONG slow_clk_ctl;	/* 5 < corerev < 10 */
	ULONG PAD;

	/* Instaclock registers (corerev >= 10) */
	ULONG system_clk_ctl;	/* 0xc0 */
	ULONG clkstatestretch;
	ULONG PAD[2];

	/* Indirect backplane access (corerev >= 22) */
	ULONG bp_addrlow;	/* 0xd0 */
	ULONG bp_addrhigh;
	ULONG bp_data;
	ULONG PAD;
	ULONG bp_indaccess;
	ULONG PAD[3];

	/* More clock dividers (corerev >= 32) */
	ULONG clkdiv2;
	ULONG PAD[2];

	/* In AI chips, pointer to erom */
	ULONG eromptr;		/* 0xfc */

	/* ExtBus control registers (corerev >= 3) */
	ULONG pcmcia_config;	/* 0x100 */
	ULONG pcmcia_memwait;
	ULONG pcmcia_attrwait;
	ULONG pcmcia_iowait;
	ULONG ide_config;
	ULONG ide_memwait;
	ULONG ide_attrwait;
	ULONG ide_iowait;
	ULONG prog_config;
	ULONG prog_waitcount;
	ULONG flash_config;
	ULONG flash_waitcount;
	ULONG SECI_config;	/* 0x130 SECI configuration */
	ULONG PAD[3];

	/* Enhanced Coexistence Interface (ECI) registers (corerev >= 21) */
	ULONG eci_output;	/* 0x140 */
	ULONG eci_control;
	ULONG eci_inputlo;
	ULONG eci_inputmi;
	ULONG eci_inputhi;
	ULONG eci_inputintpolaritylo;
	ULONG eci_inputintpolaritymi;
	ULONG eci_inputintpolarityhi;
	ULONG eci_intmasklo;
	ULONG eci_intmaskmi;
	ULONG eci_intmaskhi;
	ULONG eci_eventlo;
	ULONG eci_eventmi;
	ULONG eci_eventhi;
	ULONG eci_eventmasklo;
	ULONG eci_eventmaskmi;
	ULONG eci_eventmaskhi;
	ULONG PAD[3];

	/* SROM interface (corerev >= 32) */
	ULONG sromcontrol;	/* 0x190 */
	ULONG sromaddress;
	ULONG sromdata;
	ULONG PAD[17];

	/* Clock control and hardware workarounds (corerev >= 20) */
	ULONG clk_ctl_st;	/* 0x1e0 */
	ULONG hw_war;
	ULONG PAD[70];

	/* UARTs */
	UBYTE uart0data;	/* 0x300 */
	UBYTE uart0imr;
	UBYTE uart0fcr;
	UBYTE uart0lcr;
	UBYTE uart0mcr;
	UBYTE uart0lsr;
	UBYTE uart0msr;
	UBYTE uart0scratch;
	UBYTE PAD[248];		/* corerev >= 1 */

	UBYTE uart1data;	/* 0x400 */
	UBYTE uart1imr;
	UBYTE uart1fcr;
	UBYTE uart1lcr;
	UBYTE uart1mcr;
	UBYTE uart1lsr;
	UBYTE uart1msr;
	UBYTE uart1scratch;
	ULONG PAD[62];

	/* save/restore, corerev >= 48 */
	ULONG sr_capability;          /* 0x500 */
	ULONG sr_control0;            /* 0x504 */
	ULONG sr_control1;            /* 0x508 */
	ULONG gpio_control;           /* 0x50C */
	ULONG PAD[60];

	/* PMU registers (corerev >= 20) */
	ULONG pmucontrol;	/* 0x600 */
	ULONG pmucapabilities;
	ULONG pmustatus;
	ULONG res_state;
	ULONG res_pending;
	ULONG pmutimer;
	ULONG min_res_mask;
	ULONG max_res_mask;
	ULONG res_table_sel;
	ULONG res_dep_mask;
	ULONG res_updn_timer;
	ULONG res_timer;
	ULONG clkstretch;
	ULONG pmuwatchdog;
	ULONG gpiosel;		/* 0x638, rev >= 1 */
	ULONG gpioenable;	/* 0x63c, rev >= 1 */
	ULONG res_req_timer_sel;
	ULONG res_req_timer;
	ULONG res_req_mask;
	ULONG pmucapabilities_ext; /* 0x64c, pmurev >=15 */
	ULONG chipcontrol_addr;	/* 0x650 */
	ULONG chipcontrol_data;	/* 0x654 */
	ULONG regcontrol_addr;
	ULONG regcontrol_data;
	ULONG pllcontrol_addr;
	ULONG pllcontrol_data;
	ULONG pmustrapopt;	/* 0x668, corerev >= 28 */
	ULONG pmu_xtalfreq;	/* 0x66C, pmurev >= 10 */
	ULONG retention_ctl;          /* 0x670, pmurev >= 15 */
	ULONG PAD[3];
	ULONG retention_grpidx;       /* 0x680 */
	ULONG retention_grpctl;       /* 0x684 */
	ULONG PAD[94];
	UWORD sromotp[768];
};

/* chipid */
#define	CID_ID_MASK		0x0000ffff	/* Chip Id mask */
#define	CID_REV_MASK		0x000f0000	/* Chip Revision mask */
#define	CID_REV_SHIFT		16	/* Chip Revision shift */
#define	CID_PKG_MASK		0x00f00000	/* Package Option mask */
#define	CID_PKG_SHIFT		20	/* Package Option shift */
#define	CID_CC_MASK		0x0f000000	/* CoreCount (corerev >= 4) */
#define CID_CC_SHIFT		24
#define	CID_TYPE_MASK		0xf0000000	/* Chip Type */
#define CID_TYPE_SHIFT		28

/* capabilities */
#define	CC_CAP_UARTS_MASK	0x00000003	/* Number of UARTs */
#define CC_CAP_MIPSEB		0x00000004	/* MIPS is in big-endian mode */
#define CC_CAP_UCLKSEL		0x00000018	/* UARTs clock select */
/* UARTs are driven by internal divided clock */
#define CC_CAP_UINTCLK		0x00000008
#define CC_CAP_UARTGPIO		0x00000020	/* UARTs own GPIOs 15:12 */
#define CC_CAP_EXTBUS_MASK	0x000000c0	/* External bus mask */
#define CC_CAP_EXTBUS_NONE	0x00000000	/* No ExtBus present */
#define CC_CAP_EXTBUS_FULL	0x00000040	/* ExtBus: PCMCIA, IDE & Prog */
#define CC_CAP_EXTBUS_PROG	0x00000080	/* ExtBus: ProgIf only */
#define	CC_CAP_FLASH_MASK	0x00000700	/* Type of flash */
#define	CC_CAP_PLL_MASK		0x00038000	/* Type of PLL */
#define CC_CAP_PWR_CTL		0x00040000	/* Power control */
#define CC_CAP_OTPSIZE		0x00380000	/* OTP Size (0 = none) */
#define CC_CAP_OTPSIZE_SHIFT	19	/* OTP Size shift */
#define CC_CAP_OTPSIZE_BASE	5	/* OTP Size base */
#define CC_CAP_JTAGP		0x00400000	/* JTAG Master Present */
#define CC_CAP_ROM		0x00800000	/* Internal boot rom active */
#define CC_CAP_BKPLN64		0x08000000	/* 64-bit backplane */
#define	CC_CAP_PMU		0x10000000	/* PMU Present, rev >= 20 */
#define	CC_CAP_SROM		0x40000000	/* Srom Present, rev >= 32 */
/* Nand flash present, rev >= 35 */
#define	CC_CAP_NFLASH		0x80000000

#define	CC_CAP2_SECI		0x00000001	/* SECI Present, rev >= 36 */
/* GSIO (spi/i2c) present, rev >= 37 */
#define	CC_CAP2_GSIO		0x00000002

/* sr_control0, rev >= 48 */
#define CC_SR_CTL0_ENABLE_MASK			BIT(0)
#define CC_SR_CTL0_ENABLE_SHIFT		0
#define CC_SR_CTL0_EN_SR_ENG_CLK_SHIFT	1 /* sr_clk to sr_memory enable */
#define CC_SR_CTL0_RSRC_TRIGGER_SHIFT	2 /* Rising edge resource trigger 0 to sr_engine */
#define CC_SR_CTL0_MIN_DIV_SHIFT	6 /* Min division value for fast clk in sr_engine */
#define CC_SR_CTL0_EN_SBC_STBY_SHIFT		16
#define CC_SR_CTL0_EN_SR_ALP_CLK_MASK_SHIFT	18
#define CC_SR_CTL0_EN_SR_HT_CLK_SHIFT		19
#define CC_SR_CTL0_ALLOW_PIC_SHIFT	20 /* Allow pic to separate power domains */
#define CC_SR_CTL0_MAX_SR_LQ_CLK_CNT_SHIFT	25
#define CC_SR_CTL0_EN_MEM_DISABLE_FOR_SLEEP	30

/* pmucapabilities */
#define PCAP_REV_MASK	0x000000ff
#define PCAP_RC_MASK	0x00001f00
#define PCAP_RC_SHIFT	8
#define PCAP_TC_MASK	0x0001e000
#define PCAP_TC_SHIFT	13
#define PCAP_PC_MASK	0x001e0000
#define PCAP_PC_SHIFT	17
#define PCAP_VC_MASK	0x01e00000
#define PCAP_VC_SHIFT	21
#define PCAP_CC_MASK	0x1e000000
#define PCAP_CC_SHIFT	25
#define PCAP5_PC_MASK	0x003e0000	/* PMU corerev >= 5 */
#define PCAP5_PC_SHIFT	17
#define PCAP5_VC_MASK	0x07c00000
#define PCAP5_VC_SHIFT	22
#define PCAP5_CC_MASK	0xf8000000
#define PCAP5_CC_SHIFT	27
/* pmucapabilites_ext PMU rev >= 15 */
#define PCAPEXT_SR_SUPPORTED_MASK	(1 << 1)
/* retention_ctl PMU rev >= 15 */
#define PMU_RCTL_MACPHY_DISABLE_MASK        (1 << 26)
#define PMU_RCTL_LOGIC_DISABLE_MASK         (1 << 27)


/*
* Maximum delay for the PMU state transition in us.
* This is an upper bound intended for spinwaits etc.
*/
#define PMU_MAX_TRANSITION_DLY	15000

/* core sbconfig regs are top 256bytes of regs */
#define	SBCONFIGOFF		0xf00

/* SOC Interconnect types (aka chip types) */
#define SOCI_SB		0
#define SOCI_AI		1


/* PL-368 DMP definitions */
#define DMP_DESC_TYPE_MSK	0x0000000F
#define  DMP_DESC_EMPTY		0x00000000
#define  DMP_DESC_VALID		0x00000001
#define  DMP_DESC_COMPONENT	0x00000001
#define  DMP_DESC_MASTER_PORT	0x00000003
#define  DMP_DESC_ADDRESS	0x00000005
#define  DMP_DESC_ADDRSIZE_GT32	0x00000008
#define  DMP_DESC_EOT		0x0000000F

#define DMP_COMP_DESIGNER	0xFFF00000
#define DMP_COMP_DESIGNER_S	20
#define DMP_COMP_PARTNUM	0x000FFF00
#define DMP_COMP_PARTNUM_S	8
#define DMP_COMP_CLASS		0x000000F0
#define DMP_COMP_CLASS_S	4
#define DMP_COMP_REVISION	0xFF000000
#define DMP_COMP_REVISION_S	24
#define DMP_COMP_NUM_SWRAP	0x00F80000
#define DMP_COMP_NUM_SWRAP_S	19
#define DMP_COMP_NUM_MWRAP	0x0007C000
#define DMP_COMP_NUM_MWRAP_S	14
#define DMP_COMP_NUM_SPORT	0x00003E00
#define DMP_COMP_NUM_SPORT_S	9
#define DMP_COMP_NUM_MPORT	0x000001F0
#define DMP_COMP_NUM_MPORT_S	4

#define DMP_MASTER_PORT_UID	0x0000FF00
#define DMP_MASTER_PORT_UID_S	8
#define DMP_MASTER_PORT_NUM	0x000000F0
#define DMP_MASTER_PORT_NUM_S	4

#define DMP_SLAVE_ADDR_BASE	0xFFFFF000
#define DMP_SLAVE_ADDR_BASE_S	12
#define DMP_SLAVE_PORT_NUM	0x00000F00
#define DMP_SLAVE_PORT_NUM_S	8
#define DMP_SLAVE_TYPE		0x000000C0
#define DMP_SLAVE_TYPE_S	6
#define  DMP_SLAVE_TYPE_SLAVE	0
#define  DMP_SLAVE_TYPE_BRIDGE	1
#define  DMP_SLAVE_TYPE_SWRAP	2
#define  DMP_SLAVE_TYPE_MWRAP	3
#define DMP_SLAVE_SIZE_TYPE	0x00000030
#define DMP_SLAVE_SIZE_TYPE_S	4
#define  DMP_SLAVE_SIZE_4K	0
#define  DMP_SLAVE_SIZE_8K	1
#define  DMP_SLAVE_SIZE_16K	2
#define  DMP_SLAVE_SIZE_DESC	3

#endif				/* _SBCHIPC_H */