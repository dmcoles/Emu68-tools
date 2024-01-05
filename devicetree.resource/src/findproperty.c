/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <common/compiler.h>
#include <proto/devicetree.h>

#include "devicetree.h"

APTR L_FindProperty(REGARG(of_node_t *node, "a0"), REGARG(CONST_STRPTR propname, "a1"), 
                     REGARG(struct DeviceTreeBase *DTBase, "a6"))
{
    of_property_t *p, *prop = NULL;

    if (node)
    {
        for (p=node->on_properties; p; p=p->op_next)
        {
            if (!_strcmp(p->op_name, propname))
            {
                prop = p;
                break;
            }
        }
    }
    return prop;
}

APTR L_FindPropertyRecursive(REGARG(of_node_t *node, "a0"), REGARG(CONST_STRPTR propname, "a1"), 
                             REGARG(struct DeviceTreeBase *DeviceTreeBase, "a6"))
{
    do {
        /* Find the property first */
        APTR prop = DT_FindProperty(node, propname);

        if (prop)
        {
            /* If property is found, return it */
            return prop;
        }
        
        /* Property was not found, go to the parent and repeat */
        node = (of_node_t *)DT_GetParent(node);
    } while (node);

    return NULL;
}
