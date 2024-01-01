/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/errors.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include <stdint.h>

#include "unicam.h"

/* status register flags */

#define MBOX_TX_FULL (1UL << 31)
#define MBOX_RX_EMPTY (1UL << 30)
#define MBOX_CHANMASK 0xF

/* VideoCore tags used. */

#define VCTAG_GET_ARM_MEMORY     0x00010005
#define VCTAG_GET_CLOCK_RATE     0x00030002

static uint32_t mbox_recv(uint32_t channel, struct UnicamBase * UnicamBase)
{
	volatile uint32_t *mbox_read = (uint32_t*)(UnicamBase->u_MailBox);
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)UnicamBase->u_MailBox + 0x18);
	uint32_t response, status;

	do
	{
		do
		{
			status = LE32(*mbox_status);
			asm volatile("nop");
		}
		while (status & MBOX_RX_EMPTY);

		asm volatile("nop");
		response = LE32(*mbox_read);
		asm volatile("nop");
	}
	while ((response & MBOX_CHANMASK) != channel);

	return (response & ~MBOX_CHANMASK);
}

static void mbox_send(uint32_t channel, uint32_t data, struct UnicamBase * UnicamBase)
{
	volatile uint32_t *mbox_write = (uint32_t*)((uintptr_t)UnicamBase->u_MailBox + 0x20);
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)UnicamBase->u_MailBox + 0x18);
	uint32_t status;

	data &= ~MBOX_CHANMASK;
	data |= channel & MBOX_CHANMASK;

	do
	{
		status = LE32(*mbox_status);
		asm volatile("nop");
	}
	while (status & MBOX_TX_FULL);

	asm volatile("nop");
	*mbox_write = LE32(data);
}

ULONG enable_unicam_domain(struct UnicamBase *UnicamBase)
{
    struct ExecBase *SysBase = UnicamBase->u_SysBase;
    ULONG *FBReq = UnicamBase->u_Request;

    ULONG len = 8 * 4;

    FBReq[0] = LE32(4 * 8);      // Length
    FBReq[1] = 0;                // Request
    FBReq[2] = LE32(0x00038030); // SetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(14); // unicam1
    FBReq[6] = LE32(1);
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, UnicamBase);
    mbox_recv(8, UnicamBase);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

struct Size get_display_size(struct UnicamBase *UnicamBase)
{
	struct ExecBase *SysBase = UnicamBase->u_SysBase;
    ULONG *FBReq = UnicamBase->u_Request;
    int c = 1;
	ULONG len = 0;
    struct Size dimension;

    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x40003);
    FBReq[c++] = LE32(8);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0);
    FBReq[c++] = LE32(0);
    FBReq[c++] = 0;

	len = c * sizeof(ULONG);

    FBReq[0] = LE32(c << 2);

	CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, UnicamBase);
    mbox_recv(8, UnicamBase);
	CachePostDMA(FBReq, &len, 0);

    dimension.width = LE32(FBReq[5]);
    dimension.height = LE32(FBReq[6]);

    return dimension;
}
