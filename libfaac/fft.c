/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2002 Krzysztof Nikiel
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

#include <math.h>

#include "fft.h"
#include "util.h"

/* Bit-reversal order for the two transform sizes, short slice first, and the
 * radix-4 twiddles laid out in the order the butterflies consume them: stage
 * by stage, six floats per butterfly (W^j, W^2j, W^3j as cos, -sin). One
 * unit-stride stream instead of three gathers keeps the kernel's inner loop
 * inside the register file. Built once per process and read-only afterwards,
 * so every encoder handle shares them. */
static unsigned short reordertbl[FFT_TBL_LEN];

/* Butterflies per transform: n/4 + n/16 + ... over the radix-4 stages. */
#define TW_SHORT (16 + 4 + 1)
#define TW_LONG  (128 + 32 + 8 + 2)
#define TW_OFFSET(logm) (6 * ((logm) == FFT_LOGM_SHORT ? 0 : TW_SHORT))
static fftfloat twiddles[6 * (TW_SHORT + TW_LONG)];

void fft_init(void)
{
    static const unsigned char logms[2] = { FFT_LOGM_SHORT, FFT_LOGM_LONG };
    int t;

    for (t = 0; t < 2; t++)
    {
        int logm = logms[t];
        int size = 1 << logm;
        unsigned short *r = reordertbl + FFT_TBL_OFFSET(logm);
        fftfloat *tw = twiddles + TW_OFFSET(logm);
        int n2, i;

        for (i = 0; i < size; i++)
        {
            int reversed = 0;
            int tmp = i;
            int b;
            for (b = 0; b < logm; b++)
            {
                reversed = (reversed << 1) | (tmp & 1);
                tmp >>= 1;
            }
            r[i] = (unsigned short)reversed;
        }

        for (n2 = size >> 2; n2 >= 1; n2 >>= 2)
        {
            /* stage twiddle W_N^(j*size/(4*n2)) = exp(-2*pi*i*j/(4*n2)) */
            for (i = 0; i < n2; i++)
            {
                int m;
                for (m = 1; m <= 3; m++)
                {
                    double theta = 2.0 * M_PI_DOUBLE * (double)(m * i) / (double)(4 * n2);
                    *tw++ = (fftfloat)cos(theta);
                    *tw++ = (fftfloat)-sin(theta);
                }
            }
        }
    }
}


/* Radix-4 DIF. Swapping the 2nd/3rd butterfly outputs yields plain
 * bit-reversed order at the end, avoiding a digit-reversal permutation.
 * logm=9 (512) isn't a power of 4, so it ends with one radix-2 stage.
 */

static void radix4_dif_proc(
    float * restrict xr,
    float * restrict xi,
    int logm,
    const fftfloat * restrict stage_tw)
{
    int n = 1 << logm;
    int n2 = n;
    int n1;
    int i, j, k;

    for (k = 0; k < (logm >> 1); k++, stage_tw += 6 * n2)
    {
        n1 = n2;
        n2 >>= 2;
        for (i = 0; i < n; i += n1)
        {
            const fftfloat * restrict tw = stage_tw + 6;

            float * restrict r1p = xr + i;
            float * restrict r2p = xr + i + n2;
            float * restrict r3p = xr + i + 2*n2;
            float * restrict r4p = xr + i + 3*n2;
            float * restrict i1p = xi + i;
            float * restrict i2p = xi + i + n2;
            float * restrict i3p = xi + i + 2*n2;
            float * restrict i4p = xi + i + 3*n2;

            /* j=0 unrolled: skip the twiddle multiply, it's the identity here */
            {
                float r1 = *r1p, i1 = *i1p;
                float r2 = *r2p, i2 = *i2p;
                float r3 = *r3p, i3 = *i3p;
                float r4 = *r4p, i4 = *i4p;

                float t1 = r1 + r3, t2 = i1 + i3;
                float t3 = r2 + r4, t4 = i2 + i4;
                float t5 = r1 - r3, t6 = i1 - i3;
                float t7 = r2 - r4, t8 = i2 - i4;

                *r1p = t1 + t3; *i1p = t2 + t4;
                *r3p = t5 + t8; *i3p = t6 - t7;
                *r2p = t1 - t3; *i2p = t2 - t4;
                *r4p = t5 - t8; *i4p = t6 + t7;

                r1p++; r2p++; r3p++; r4p++;
                i1p++; i2p++; i3p++; i4p++;
            }

            /* unit-stride pointers, not xr[i+j+...], keep the address arithmetic out of the loop */
            for (j = 1; j < n2; j++, tw += 6)
            {
                const float c1 = tw[0], s1 = tw[1];
                const float c2 = tw[2], s2 = tw[3];
                const float c3 = tw[4], s3 = tw[5];

                float r1 = *r1p, i1 = *i1p;
                float r2 = *r2p, i2 = *i2p;
                float r3 = *r3p, i3 = *i3p;
                float r4 = *r4p, i4 = *i4p;

                float t1 = r1 + r3, t2 = i1 + i3;
                float t3 = r2 + r4, t4 = i2 + i4;
                float t5 = r1 - r3, t6 = i1 - i3;
                float t7 = r2 - r4, t8 = i2 - i4;

                *r1p = t1 + t3;
                *i1p = t2 + t4;

                r1 = t1 - t3; i1 = t2 - t4;
                r2 = t5 + t8; i2 = t6 - t7;
                r3 = t5 - t8; i3 = t6 + t7;

                *r3p = r2 * c1 - i2 * s1;
                *i3p = r2 * s1 + i2 * c1;
                *r2p = r1 * c2 - i1 * s2;
                *i2p = r1 * s2 + i1 * c2;
                *r4p = r3 * c3 - i3 * s3;
                *i4p = r3 * s3 + i3 * c3;

                r1p++; r2p++; r3p++; r4p++;
                i1p++; i2p++; i3p++; i4p++;
            }
        }
    }

    /* odd logm: 4^k can't fill it, one radix-2 stage mops up the remainder */
    if (logm & 1)
    {
        float * restrict r1p = xr;
        float * restrict r2p = xr + 1;
        float * restrict i1p = xi;
        float * restrict i2p = xi + 1;
        for (i = 0; i < n; i += 2)
        {
            float r1 = *r1p, i1 = *i1p;
            float r2 = *r2p, i2 = *i2p;
            *r1p = r1 + r2; *i1p = i1 + i2;
            *r2p = r1 - r2; *i2p = i1 - i2;
            r1p += 2; r2p += 2; i1p += 2; i2p += 2;
        }
    }
}

static void bit_reverse(
    float * restrict xr,
    float * restrict xi,
    int logm,
    const unsigned short * restrict r)
{
    int i;
    int size = 1 << logm;

    for (i = 0; i < size; i++)
    {
        int j = (int)r[i];
        if (j > i)
        {
            float tr = xr[i]; xr[i] = xr[j]; xr[j] = tr;
            float ti = xi[i]; xi[i] = xi[j]; xi[j] = ti;
        }
    }
}

void fft(float *xr, float *xi, int logm)
{
    radix4_dif_proc(xr, xi, logm, twiddles + TW_OFFSET(logm));
    bit_reverse(xr, xi, logm, reordertbl + FFT_TBL_OFFSET(logm));
}
