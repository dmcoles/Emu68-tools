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

APTR L_OpenKey(REGARG(CONST_STRPTR key, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"))
{
    char ptrbuf[64];
    int i;
    of_node_t *node, *ret = NULL;
    
    if (key[0] == '/' && key[1] == 0)
        return DTBase->dt_Root;
    
    if (*key == '/')
    {   
        ret = DTBase->dt_Root;
        
        while(*key)
        {   
            int found = 0;

            if (*key == '/' )
                key++;

            for (i=0; i < 63; i++)
            {   
                ptrbuf[i] = key[i];

                if (key[i] == '/' || key[i] == 0)
                    break;
            }
            
            ptrbuf[i] = 0;
            
            for (node = ret->on_children; node; node = node->on_next)
            {   
                if (!dt_strcmp(ptrbuf, node->on_name))
                {   
                    ret = node;
                    found = 1;
                    break;
                }
            }

            if (!found)
                return NULL;

            key += _strlen(ptrbuf);
        }
    }

    return ret;
}
