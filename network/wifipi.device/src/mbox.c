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

#include "wifipi.h"

/* status register flags */

#define MBOX_TX_FULL (1UL << 31)
#define MBOX_RX_EMPTY (1UL << 30)
#define MBOX_CHANMASK 0xF

/* VideoCore tags used. */

#define VCTAG_GET_ARM_MEMORY     0x00010005
#define VCTAG_GET_CLOCK_RATE     0x00030002

static uint32_t mbox_recv(uint32_t channel, struct WiFiBase * WiFiBase)
{
	volatile uint32_t *mbox_read = (uint32_t*)(WiFiBase->w_MailBox);
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)WiFiBase->w_MailBox + 0x18);
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

static void mbox_send(uint32_t channel, uint32_t data, struct WiFiBase * WiFiBase)
{
	volatile uint32_t *mbox_write = (uint32_t*)((uintptr_t)WiFiBase->w_MailBox + 0x20);
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)WiFiBase->w_MailBox + 0x18);
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

uint32_t get_clock_rate(uint32_t clock_id, struct WiFiBase * WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG *FBReq = WiFiBase->w_Request;
    ULONG len = 8*4;

    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00030047);// GetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, WiFiBase);
    ULONG resp = mbox_recv(8, WiFiBase); 
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_clock_rate(uint32_t clock_id, uint32_t speed, struct WiFiBase * WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG *FBReq = WiFiBase->w_Request;
    ULONG len = 9*4;

    FBReq[0] = LE32(4*9);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00038002);// SetClockRate
    FBReq[3] = LE32(12);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = LE32(speed);
    FBReq[7] = 0;
    FBReq[8] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, WiFiBase);
    ULONG reply = mbox_recv(8, WiFiBase);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t get_clock_state(uint32_t id, struct WiFiBase * WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG *FBReq = WiFiBase->w_Request;
    ULONG len = 8*4;

    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00030001);// GetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, WiFiBase);
    ULONG reply = mbox_recv(8, WiFiBase);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_clock_state(uint32_t id, uint32_t state, struct WiFiBase * WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG *FBReq = WiFiBase->w_Request;
    ULONG len = 8*4;

    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00038001);// SetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = LE32(state);
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, WiFiBase);
    ULONG reply = mbox_recv(8, WiFiBase);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t get_power_state(uint32_t id, struct WiFiBase * WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG *FBReq = WiFiBase->w_Request;
    ULONG len = 8*4;

    FBReq[0] = LE32(4*8);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00020001);// GetClockRate
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, WiFiBase);
    ULONG reply = mbox_recv(8, WiFiBase);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_power_state(uint32_t id, uint32_t state, struct WiFiBase * WiFiBase)
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;

    ULONG *FBReq = WiFiBase->w_Request;
    ULONG len = 9*4;

    FBReq[0] = LE32(4*9);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(0x00028001);// SetClockRate
    FBReq[3] = LE32(12);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = LE32(state);
    FBReq[7] = 0;
    FBReq[8] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (ULONG)FBReq, WiFiBase);
    ULONG reply = mbox_recv(8, WiFiBase);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}
