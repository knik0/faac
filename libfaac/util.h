/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#ifndef UTIL_H
#define UTIL_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdlib.h>
#include <string.h>

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

static inline int clamp_int(int x, int lo, int hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

#ifndef M_PI
#define M_PI        3.14159265358979323846f
#endif
#ifndef M_SQRT2
#define M_SQRT2     1.41421356237309504880f
#endif

/* Double-precision pi, for one-time table/window generation at encoder init
 * (FFT/MDCT twiddles, SBR QMF twiddles, MDCT window shapes) where the extra
 * precision is free — these run once, never in the per-sample hot path. */
#define M_PI_DOUBLE 3.14159265358979323846

/* Memory functions */
#define AllocMemory(size) malloc(size)
#define FreeMemory(block) free(block)
#define SetMemory(block, value, size) memset(block, value, size)

int GetSRIndex(unsigned int sampleRate);
unsigned int MaxBitrate(unsigned long sampleRate);
unsigned int MinBitrate(void);
int CountLeadingZeros(unsigned int x);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* UTIL_H */
