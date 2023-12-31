/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>

#include "unicam.h"
#include "smoothing.h"

// All computations use 24.8 fixed point

static LONG mitchell_netravali(LONG x, LONG b, LONG c)
{
    LONG k;

    if (x < 0)
        x = -x;
    
    if (x < 256) {
        LONG a1 = (12 * 256 - 9 * b - 6 * c);
        LONG a2 = (-18 * 256 + 12 * b + 6 * c);
        LONG a3 = (6 * 256 - 2 * b);
        k = ((((((a1 * x + 128) >> 8) * x + 128) >> 8) * x + 128) >> 8) + 
            ((((a2 * x + 128) >> 8) * x + 128) >> 8) + 
            a3;
    }
    else if (x < 512) {
        LONG a1 = (-b - 6 * c);
        LONG a2 = (6 * b + 30 * c);
        LONG a3 = (-12 * b - 48 * c);
        k = ((((((a1 * x + 128) >> 8) * x + 128) >> 8) * x + 128) >> 8) +
            ((((a2 * x + 128) >> 8) * x + 128) >> 8) + 
            ((a3 * x + 128) >> 8) + 
            8 * b + 
            24 * c;
    }
    else
        k = 0;
    
    k = (((255 * k) / 6) + 128) >> 8;

    return k;
}

void compute_scaling_kernel(ULONG *dlist_memory, LONG b, LONG c)
{
    ULONG half_kernel[6] = {0, 0, 0, 0, 0, 0};

    for (int i=0; i < 16; i++) {
        int val = mitchell_netravali(2 * 256 - (i * 2560) / 75, b, c);
        half_kernel[i / 3] |= (val & 0x1ff) << (9 * (i % 3));
    }
    half_kernel[5] |= half_kernel[5] << 9;

    for (int i=0; i<11; i++) {
        if (i < 6) {
            dlist_memory[i] = LE32(half_kernel[i]);
        } else {
            dlist_memory[i] = LE32(half_kernel[11 - i - 1]);
        }
    }
}

void compute_nearest_neighbour_kernel(ULONG *dlist_memory)
{
    ULONG half_kernel[6] = {0, 0, 0, 0, 0, 0};

    for (int i=0; i < 16; i++) {
        int val = i < 8 ? 0 : 255;
        half_kernel[i / 3] |= (val & 0x1ff) << (9 * (i % 3));
    }
    half_kernel[5] |= half_kernel[5] << 9;

    for (int i=0; i<11; i++) {
        if (i < 6) {
            dlist_memory[i] = LE32(half_kernel[i]);
        } else {
            dlist_memory[i] = LE32(half_kernel[11 - i - 1]);
        }
    }
}
