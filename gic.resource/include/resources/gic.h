#ifndef RESOURCES_GIC_H
#define RESOURCES_GIC_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

enum
{
    ARM_GIC_IRQ = 0,
    ARM_GIC_FIQ,
    ARM_GIC_ERR
};

#endif /* RESOURCES_GIC_H */
