#ifndef __MAILBOX_H
#define __MAILBOX_H

/*
    Copyright © 2024 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <stdint.h>
#include <common/compiler.h>

struct MailboxBase {
    struct Library          mb_Node;
    struct ExecBase *       mb_ExecBase;
    struct SignalSemaphore  mb_Lock;
    APTR                    mb_RequestBase;
    ULONG *                 mb_Request;
    APTR                    mb_MailBox;
    APTR                    mb_PeripheralBase;
};

extern const char deviceName[];
extern const char deviceIdString[];

#define MB_FUNC_COUNT 3
#define BASE_NEG_SIZE ((MB_FUNC_COUNT) * 6)
#define BASE_POS_SIZE ((sizeof(struct MailboxBase)))
#define MB_PRIORITY     119
#define MB_VERSION      1
#define MB_REVISION     0

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

void kprintf(REGARG(const char * msg, "a0"), REGARG(void * args, "a1"));

#define bug(string, ...) \
    do { ULONG args[] = {0, __VA_ARGS__}; kprintf(string, &args[1]); } while(0)

#define MBOX_TX_FULL (1UL << 31)
#define MBOX_RX_EMPTY (1UL << 30)
#define MBOX_CHANMASK 0xF

#endif /* __MAILBOX_H */
