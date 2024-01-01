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

void L_UnicamStart(REGARG(ULONG *address, "a0"), REGARG(UBYTE lanes, "d0"), REGARG(UBYTE datatype, "d1"),
                 REGARG(ULONG width, "d2"), REGARG(ULONG height, "d3"), REGARG(UBYTE bpp, "d4"),
                 REGARG(struct UnicamBase * UnicamBase, "a6"))
{
    unicam_run(address, lanes, datatype, width, height, bpp, UnicamBase);
}
