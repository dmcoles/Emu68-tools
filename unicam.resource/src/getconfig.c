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

ULONG L_UnicamGetConfig(REGARG(struct UnicamBase * UnicamBase, "a6"))
{
    ULONG cfg = 0;

    if (UnicamBase->u_Integer) cfg |= UNICAMF_INTEGER;
    if (UnicamBase->u_Smooth) cfg |= UNICAMF_SMOOTHING;
    if (UnicamBase->u_StartOnBoot) cfg |= UNICAMF_BOOT;

    cfg |= (UnicamBase->u_Scaler << UNICAMB_SCALER) & UNICAMF_SCALER;
    cfg |= (UnicamBase->u_Phase << UNICAMB_PHASE) & UNICAMF_PHASE;
    
    return cfg;
}
