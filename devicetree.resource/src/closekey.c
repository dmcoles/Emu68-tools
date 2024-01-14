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

void L_CloseKey(REGARG(of_node_t *node, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"))
{
    (void)node;
    (void)DTBase;
}