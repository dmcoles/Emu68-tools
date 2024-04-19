#ifndef __DEVICETREE_H
#define __DEVICETREE_H

/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
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
#include <stdint.h>
#include <common/compiler.h>

typedef struct of_property {
    struct of_property *op_next;
    const char *        op_name;
    uint32_t            op_length;
    const void *        op_value;
} of_property_t;

typedef struct of_node {
    struct of_node *    on_next;
    struct of_node *    on_parent;
    const char *        on_name;
    struct of_node *    on_children;
    of_property_t *     on_properties;
} of_node_t;

struct DeviceTreeBase {
    struct Library      dt_Node;
    struct ExecBase *   dt_ExecBase;
    of_node_t *         dt_Root;
    CONST_STRPTR        dt_StrNull;
    uint32_t *          dt_Data;
    CONST_STRPTR        dt_Strings;
};

extern const char deviceName[];
extern const char deviceIdString[];

#define BASE_NEG_SIZE (11 * 6)
#define BASE_POS_SIZE ((sizeof(struct DeviceTreeBase)))
#define DT_PRIORITY     127
#define DT_VERSION      1
#define DT_REVISION     4


static inline int dt_strcmp(const char *s1, const char *s2)
{   
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    
    if (*s1 == 0 && s2[-1] == '@') {
        return 0;
    }
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

static inline int _strcmp(const char *s1, const char *s2)
{   
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

static inline int _strlen(const char *buf)
{
    int len = 0;

    if (buf)
        while(*buf++)
            len++;

    return len;
}

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct fdt_reserve_entry {
    uint64_t address;
    uint64_t size;
};

struct fdt_prop_entry {
    uint32_t len;
    uint32_t nameoffset;
};

#define FDT_END         0x00000009
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004

#define FDT_MAGIC       0xd00dfeed

void Add_DT_Memory(struct ExecBase *SysBase, APTR DeviceTreeBase);
void L_CloseKey(REGARG(of_node_t *node, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
APTR L_FindProperty(REGARG(of_node_t *node, "a0"), REGARG(CONST_STRPTR propname, "a1"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
APTR L_GetChild(REGARG(of_node_t *key, "a0"), REGARG(of_node_t *prev, "a1"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
CONST_STRPTR L_GetKeyName(REGARG(of_node_t *p, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
APTR L_GetParent(REGARG(of_node_t *key, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
APTR L_GetProperty(REGARG(of_node_t *key, "a0"), REGARG(of_property_t *prev, "a1"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
ULONG L_GetPropLen(REGARG(of_property_t *p, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
CONST_STRPTR L_GetPropName(REGARG(of_property_t *p, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
CONST_STRPTR L_GetPropValue(REGARG(of_property_t *p, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
APTR L_OpenKey(REGARG(CONST_STRPTR key, "a0"), REGARG(struct DeviceTreeBase *DTBase, "a6"));
APTR L_FindPropertyRecursive(REGARG(of_node_t *node, "a0"), REGARG(CONST_STRPTR propname, "a1"), REGARG(struct DeviceTreeBase *DeviceTreeBase, "a6"));
#endif /* __DEVICETREE_H */
