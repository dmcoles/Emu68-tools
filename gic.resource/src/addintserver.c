/*
    Copyright © 2024 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/interrupts.h>
#include <common/compiler.h>

#include <proto/exec.h>

#include "gic.h"

void L_GIC_AddIntServer(REGARG(ULONG intNum, "d0"), REGARG(struct Interrupt *interrupt, "a1"),
                        REGARG(struct GICBase *GICBase, "a6"))
{
    struct ExecBase *SysBase = GICBase->g_SysBase;
}
