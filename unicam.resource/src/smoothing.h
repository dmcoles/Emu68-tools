#ifndef _SMOOTHING_H
#define _SMOOTHING_H

#include <exec/types.h>

void compute_scaling_kernel(ULONG *dlist_memory, LONG b, LONG c);
void compute_nearest_neighbour_kernel(ULONG *dlist_memory);

#endif /* _SMOOTHING_H */
