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

APTR L_GetChild(REGARG(of_node_t *key, "a0"), REGARG(of_node_t *prev, "a1"), 
                 REGARG(struct DeviceTreeBase *DTBase, "a6"))
{
    if (prev != NULL)
        return prev->on_next;

    if (key == NULL)
        key = DTBase->dt_Root;
    
    return key->on_children;
}
