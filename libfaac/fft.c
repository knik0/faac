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

/* Radix-4 twiddles laid out in the order the butterflies consume them: stage
 * by stage, six floats per butterfly (W^j, W^2j, W^3j as cos, -sin). One
 * unit-stride stream instead of three gathers keeps the kernel's inner loop
 * inside the register file. Built once per process and read-only afterwards,
 * so every encoder handle shares them. */

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
        fftfloat *tw = twiddles + TW_OFFSET(logm);
        int n2, i;

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


/* Radix-4 DIF, self-sorting (Stockham). Stage k holds B = 4^k sub-transforms
 * interleaved, element j of sub-transform c at j*B + c, so a butterfly reads
 * the four quarters of src and writes four interleaved runs of dst. The output
 * lands in natural order without a permutation pass. logm=9 (512) isn't a
 * power of 4, so it ends with one radix-2 stage.
 */

#define BUTTERFLY_SUMS \
    float t1 = r1 + r3, t2 = i1 + i3; \
    float t3 = r2 + r4, t4 = i2 + i4; \
    float t5 = r1 - r3, t6 = i1 - i3; \
    float t7 = r2 - r4, t8 = i2 - i4

/* Stage 0: a single sub-transform, so the twiddle changes every butterfly
 * and the four outputs of butterfly j are adjacent at 4j. j=0's identity
 * twiddle isn't special-cased: one butterfly in n/4 isn't worth a second
 * copy of the loop body. */
static void radix4_first_stage(
    const float * restrict sr,
    const float * restrict si,
    float * restrict dr,
    float * restrict di,
    int n4,
    const fftfloat * restrict tw)
{
    int j;

    for (j = 0; j < n4; j++)
    {
        const float c1 = tw[6 * j],     s1 = tw[6 * j + 1];
        const float c2 = tw[6 * j + 2], s2 = tw[6 * j + 3];
        const float c3 = tw[6 * j + 4], s3 = tw[6 * j + 5];
        float * restrict o = dr + 4 * j;
        float * restrict p = di + 4 * j;

        float r1 = sr[j],          i1 = si[j];
        float r2 = sr[j + n4],     i2 = si[j + n4];
        float r3 = sr[j + 2 * n4], i3 = si[j + 2 * n4];
        float r4 = sr[j + 3 * n4], i4 = si[j + 3 * n4];
        BUTTERFLY_SUMS;

        o[0] = t1 + t3;
        p[0] = t2 + t4;

        r1 = t1 - t3; i1 = t2 - t4;
        r2 = t5 + t8; i2 = t6 - t7;
        r3 = t5 - t8; i3 = t6 + t7;

        o[1] = r2 * c1 - i2 * s1;
        p[1] = r2 * s1 + i2 * c1;
        o[2] = r1 * c2 - i1 * s2;
        p[2] = r1 * s2 + i1 * c2;
        o[3] = r3 * c3 - i3 * s3;
        p[3] = r3 * s3 + i3 * c3;
    }
}

/* B butterflies sharing one twiddle: element c of each of the four input
 * quarters to element c of each of the four output runs. A restrict pointer
 * per output run tells the vectorizer the runs can't overlap. */
static inline void radix4_butterflies(
    const float * restrict sr, const float * restrict si, int n4,
    float * restrict o1r, float * restrict o1i,
    float * restrict o2r, float * restrict o2i,
    float * restrict o3r, float * restrict o3i,
    float * restrict o4r, float * restrict o4i,
    int B,
    const fftfloat * restrict tw)
{
    const float c1 = tw[0], s1 = tw[1];
    const float c2 = tw[2], s2 = tw[3];
    const float c3 = tw[4], s3 = tw[5];
    int c;

    for (c = 0; c < B; c++)
    {
        float r1 = sr[c],          i1 = si[c];
        float r2 = sr[c + n4],     i2 = si[c + n4];
        float r3 = sr[c + 2 * n4], i3 = si[c + 2 * n4];
        float r4 = sr[c + 3 * n4], i4 = si[c + 3 * n4];
        BUTTERFLY_SUMS;

        o1r[c] = t1 + t3;
        o1i[c] = t2 + t4;

        r1 = t1 - t3; i1 = t2 - t4;
        r2 = t5 + t8; i2 = t6 - t7;
        r3 = t5 - t8; i3 = t6 + t7;

        o2r[c] = r2 * c1 - i2 * s1;
        o2i[c] = r2 * s1 + i2 * c1;
        o3r[c] = r1 * c2 - i1 * s2;
        o3i[c] = r1 * s2 + i1 * c2;
        o4r[c] = r3 * c3 - i3 * s3;
        o4i[c] = r3 * s3 + i3 * c3;
    }
}

/* Later stages: B = 4*B4 sub-transforms share each twiddle, so the inner
 * loop is unit-stride over c. Passing B4 rather than B lets the vectorizer
 * see the trip count is a multiple of four and skip the epilogue. */
static void radix4_stage(
    const float * restrict sr,
    const float * restrict si,
    float * restrict dr,
    float * restrict di,
    int n4,
    int B4,
    const fftfloat * restrict tw)
{
    const int B = 4 * B4;
    int n2 = n4 / B;
    int j;

    for (j = 0; j < n2; j++, tw += 6, sr += B, si += B, dr += 4 * B, di += 4 * B)
        radix4_butterflies(sr, si, n4, dr, di, dr + B, di + B,
                           dr + 2 * B, di + 2 * B, dr + 3 * B, di + 3 * B,
                           B, tw);
}

/* odd logm: 4^k can't fill it, one radix-2 stage mops up the remainder */
static void radix2_stage(
    const float * restrict r1p, const float * restrict i1p,
    const float * restrict r2p, const float * restrict i2p,
    float * restrict o1r, float * restrict o1i,
    float * restrict o2r, float * restrict o2i,
    int n2)
{
    int c;

    for (c = 0; c < n2; c++)
    {
        float r1 = r1p[c], i1 = i1p[c];
        float r2 = r2p[c], i2 = i2p[c];
        o1r[c] = r1 + r2; o1i[c] = i1 + i2;
        o2r[c] = r1 - r2; o2i[c] = i1 - i2;
    }
}

void fft(float * restrict x, float * restrict y, int logm)
{
    const fftfloat *tw = twiddles + TW_OFFSET(logm);
    int n = 1 << logm;
    int n4 = n >> 2;
    float *src = y, *dst = x;
    int B4;

    /* Stages ping-pong x -> y -> x ...; both sizes take an odd number of
     * them, so the result lands in y. */
    radix4_first_stage(x, x + n, y, y + n, n4, tw);
    tw += 6 * n4;
    for (B4 = 1; 4 * B4 <= n4; B4 *= 4)
    {
        float *t;
        radix4_stage(src, src + n, dst, dst + n, n4, B4, tw);
        tw += 6 * (n4 / (4 * B4));
        t = src; src = dst; dst = t;
    }
    /* Odd logm stops at 4*B4 = n/2 sub-transforms of length 2, whose two
     * elements sit n/2 apart: one radix-2 stage pairs them. */
    if (4 * B4 < n)
        radix2_stage(x, x + n, x + n / 2, x + n + n / 2,
                     y, y + n, y + n / 2, y + n + n / 2, 4 * B4);
}
