/*
    Copyright © 2024 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <common/compiler.h>

#include "unicam.h"
#include "mbox.h"

APTR L_UnicamGetFramebuffer(REGARG(struct UnicamBase * UnicamBase, "a6"))
{
    return UnicamBase->u_ReceiveBuffer;
}

ULONG L_UnicamGetFramebufferSize(REGARG(struct UnicamBase * UnicamBase, "a6"))
{
    return UnicamBase->u_ReceiveBufferSize;
}
