/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <common/compiler.h>
#include "devicetree.h"

ULONG L_GetPropLen(REGARG(of_property_t *p, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"))
{
    /* If property address is given, look into it */
    if (p)
    {
        /* If property has a name (it must!) return it, otherwise return null string */
        if (p->op_length)
        {
            return p->op_length;
        }
        else
        {
            return 0;
        }
    }
    else
        return 0;
}