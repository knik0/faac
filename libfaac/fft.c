/*
 * FAAC - Freeware Advanced Audio Coder
 * $Id: fft.c,v 1.12 2005/02/02 07:49:55 sur Exp $
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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <math.h>
#include <stdlib.h>

#include "fft.h"
#include "util.h"

#define LOGM_SHORT 6      /* logm for the 256-sample short block MDCT */
#define LOGM_LONG  FFT_MAXLOGM /* logm for the 2048-sample long block MDCT */

void fft_initialize(FFT_Tables *fft_tables)
{
    int i;
    fft_tables->costbl = AllocMemory((FFT_MAXLOGM + 1) * sizeof(fft_tables->costbl[0]));
    fft_tables->negsintbl = AllocMemory((FFT_MAXLOGM + 1) * sizeof(fft_tables->negsintbl[0]));
    fft_tables->reordertbl = AllocMemory((FFT_MAXLOGM + 1) * sizeof(fft_tables->reordertbl[0]));

    if (!fft_tables->costbl || !fft_tables->negsintbl || !fft_tables->reordertbl)
    {
        if (fft_tables->costbl) FreeMemory(fft_tables->costbl);
        if (fft_tables->negsintbl) FreeMemory(fft_tables->negsintbl);
        if (fft_tables->reordertbl) FreeMemory(fft_tables->reordertbl);
        fft_tables->costbl = NULL;
        fft_tables->negsintbl = NULL;
        fft_tables->reordertbl = NULL;
        return;
    }

    for (i = 0; i < FFT_MAXLOGM + 1; i++)
    {
        fft_tables->costbl[i] = NULL;
        fft_tables->negsintbl[i] = NULL;
        fft_tables->reordertbl[i] = NULL;
    }

    for (i = 0; i < FFT_MAXLOGM + 1; i++)
    {
        fft_tables->mdct_cos[i] = NULL;
        fft_tables->mdct_sin[i] = NULL;
    }

    /* Precompute MDCT pre/post-twiddles for both block sizes now, so the
       per-frame twiddle loop is a table lookup instead of a cos/sin recurrence. */
    {
        static const int logms[2] = { LOGM_SHORT, LOGM_LONG };
        int t;
        for (t = 0; t < 2; t++)
        {
            int logm = logms[t];
            int size = 1 << logm;
            faac_real freq = (faac_real)(2.0 * M_PI) / (faac_real)(4 << logm);
            fftfloat *c = AllocMemory(size * sizeof(fftfloat));
            fftfloat *s = AllocMemory(size * sizeof(fftfloat));

            if (!c || !s)
            {
                if (c) FreeMemory(c);
                if (s) FreeMemory(s);
                continue;
            }

            for (i = 0; i < size; i++)
            {
                faac_real theta = freq * ((faac_real)i + (faac_real)0.125);
                c[i] = (fftfloat)FAAC_COS(theta);
                s[i] = (fftfloat)FAAC_SIN(theta);
            }
            fft_tables->mdct_cos[logm] = c;
            fft_tables->mdct_sin[logm] = s;
        }
    }
}

void fft_terminate(FFT_Tables *fft_tables)
{
    int i;

    for (i = 0; i < FFT_MAXLOGM + 1; i++)
    {
        if (fft_tables->costbl[i] != NULL)
            FreeMemory(fft_tables->costbl[i]);

        if (fft_tables->negsintbl[i] != NULL)
            FreeMemory(fft_tables->negsintbl[i]);

        if (fft_tables->reordertbl[i] != NULL)
            FreeMemory(fft_tables->reordertbl[i]);
    }

    for (i = 0; i < FFT_MAXLOGM + 1; i++)
    {
        if (fft_tables->mdct_cos[i] != NULL)
            FreeMemory(fft_tables->mdct_cos[i]);
        if (fft_tables->mdct_sin[i] != NULL)
            FreeMemory(fft_tables->mdct_sin[i]);
        fft_tables->mdct_cos[i] = NULL;
        fft_tables->mdct_sin[i] = NULL;
    }

    FreeMemory(fft_tables->costbl);
    FreeMemory(fft_tables->negsintbl);
    FreeMemory(fft_tables->reordertbl);

    fft_tables->costbl = NULL;
    fft_tables->negsintbl = NULL;
    fft_tables->reordertbl = NULL;
}


/* Radix-4 DIF. Swapping the 2nd/3rd butterfly outputs yields plain
 * bit-reversed order at the end, avoiding a digit-reversal permutation.
 * logm=9 (512) isn't a power of 4, so it ends with one radix-2 stage.
 */

static void check_tables_radix4(FFT_Tables *fft_tables, int logm)
{
    if (fft_tables->costbl[logm] == NULL)
    {
        int size = 1 << logm;
        int i;
        /* one table serves all stages: stage k needs W_N^{k<<2k'} via tw_idx below */
        fft_tables->costbl[logm] = AllocMemory(size * sizeof(*(fft_tables->costbl[0])));
        fft_tables->negsintbl[logm] = AllocMemory(size * sizeof(*(fft_tables->negsintbl[0])));

        if (!fft_tables->costbl[logm] || !fft_tables->negsintbl[logm])
        {
            if (fft_tables->costbl[logm]) FreeMemory(fft_tables->costbl[logm]);
            if (fft_tables->negsintbl[logm]) FreeMemory(fft_tables->negsintbl[logm]);
            fft_tables->costbl[logm] = fft_tables->negsintbl[logm] = NULL;
            return;
        }

        for (i = 0; i < size; i++)
        {
            faac_real theta = 2.0 * M_PI * (faac_real)i / (faac_real)size;
            fft_tables->costbl[logm][i] = (fftfloat)FAAC_COS(theta);
            fft_tables->negsintbl[logm][i] = (fftfloat)-FAAC_SIN(theta);
        }
    }
}

static void radix4_dif_proc(
    faac_real * restrict xr,
    faac_real * restrict xi,
    int logm,
    const fftfloat * restrict costbl,
    const fftfloat * restrict sintbl)
{
    int n = 1 << logm;
    int n2 = n;
    int n1;
    int i, j, k;

    for (k = 0; k < (logm >> 1); k++)
    {
        n1 = n2;
        n2 >>= 2;
        for (i = 0; i < n; i += n1)
        {
            faac_real * restrict r1p = xr + i;
            faac_real * restrict r2p = xr + i + n2;
            faac_real * restrict r3p = xr + i + 2*n2;
            faac_real * restrict r4p = xr + i + 3*n2;
            faac_real * restrict i1p = xi + i;
            faac_real * restrict i2p = xi + i + n2;
            faac_real * restrict i3p = xi + i + 2*n2;
            faac_real * restrict i4p = xi + i + 3*n2;

            /* j=0 unrolled: skip the twiddle multiply, it's the identity here */
            {
                faac_real r1 = *r1p, i1 = *i1p;
                faac_real r2 = *r2p, i2 = *i2p;
                faac_real r3 = *r3p, i3 = *i3p;
                faac_real r4 = *r4p, i4 = *i4p;

                faac_real t1 = r1 + r3, t2 = i1 + i3;
                faac_real t3 = r2 + r4, t4 = i2 + i4;
                faac_real t5 = r1 - r3, t6 = i1 - i3;
                faac_real t7 = r2 - r4, t8 = i2 - i4;

                *r1p = t1 + t3; *i1p = t2 + t4;
                *r3p = t5 + t8; *i3p = t6 - t7;
                *r2p = t1 - t3; *i2p = t2 - t4;
                *r4p = t5 - t8; *i4p = t6 + t7;

                r1p++; r2p++; r3p++; r4p++;
                i1p++; i2p++; i3p++; i4p++;
            }

            /* unit-stride pointers, not xr[i+j+...], so the compiler can vectorize this */
            for (j = 1; j < n2; j++)
            {
                int tw_idx = j << (2 * k);
                const faac_real c1 = (faac_real)costbl[tw_idx];
                const faac_real s1 = (faac_real)sintbl[tw_idx];
                const faac_real c2 = (faac_real)costbl[2 * tw_idx];
                const faac_real s2 = (faac_real)sintbl[2 * tw_idx];
                const faac_real c3 = (faac_real)costbl[3 * tw_idx];
                const faac_real s3 = (faac_real)sintbl[3 * tw_idx];

                faac_real r1 = *r1p, i1 = *i1p;
                faac_real r2 = *r2p, i2 = *i2p;
                faac_real r3 = *r3p, i3 = *i3p;
                faac_real r4 = *r4p, i4 = *i4p;

                faac_real t1 = r1 + r3, t2 = i1 + i3;
                faac_real t3 = r2 + r4, t4 = i2 + i4;
                faac_real t5 = r1 - r3, t6 = i1 - i3;
                faac_real t7 = r2 - r4, t8 = i2 - i4;

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
        faac_real * restrict r1p = xr;
        faac_real * restrict r2p = xr + 1;
        faac_real * restrict i1p = xi;
        faac_real * restrict i2p = xi + 1;
        for (i = 0; i < n; i += 2)
        {
            faac_real r1 = *r1p, i1 = *i1p;
            faac_real r2 = *r2p, i2 = *i2p;
            *r1p = r1 + r2; *i1p = i1 + i2;
            *r2p = r1 - r2; *i2p = i1 - i2;
            r1p += 2; r2p += 2; i1p += 2; i2p += 2;
        }
    }
}

static void bit_reverse(
    faac_real * restrict xr,
    faac_real * restrict xi,
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
            faac_real tr = xr[i]; xr[i] = xr[j]; xr[j] = tr;
            faac_real ti = xi[i]; xi[i] = xi[j]; xi[j] = ti;
        }
    }
}

void fft(FFT_Tables *fft_tables, faac_real *xr, faac_real *xi, int logm)
{
    if (logm > FFT_MAXLOGM) return;
    if (logm < 1) return;

    check_tables_radix4(fft_tables, logm);

    if (fft_tables->reordertbl[logm] == NULL)
    {
        int size = 1 << logm;
        int i;
        fft_tables->reordertbl[logm] = AllocMemory(size * sizeof(*(fft_tables->reordertbl[0])));
        if (!fft_tables->reordertbl[logm]) return;

        for (i = 0; i < size; i++)
        {
            int reversed = 0;
            int b;
            int tmp = i;
            for (b = 0; b < logm; b++)
            {
                reversed = (reversed << 1) | (tmp & 1);
                tmp >>= 1;
            }
            fft_tables->reordertbl[logm][i] = (unsigned short)reversed;
        }
    }

    radix4_dif_proc(xr, xi, logm, fft_tables->costbl[logm], fft_tables->negsintbl[logm]);
    bit_reverse(xr, xi, logm, fft_tables->reordertbl[logm]);
}

