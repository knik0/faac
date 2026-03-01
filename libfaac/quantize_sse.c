/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2026 Nils Schimmelmann
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <immintrin.h>
#include "faac_real.h"

#define MAGIC_NUMBER 0.4054

void quantize_sse2(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix)
{
    const __m128 zero = _mm_setzero_ps();
    const __m128 sfac = _mm_set1_ps(sfacfix);
    const __m128 magic = _mm_set1_ps(MAGIC_NUMBER);
    // Mask to strip the sign bit (0x7FFFFFFF)
    const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    int cnt = 0;

    // Process 4 elements per iteration
    for (; cnt <= n - 4; cnt += 4)
    {
#ifdef FAAC_PRECISION_SINGLE
        __m128 x_orig = _mm_loadu_ps((const float*)&xr[cnt]);
#else
        // Convert 4 doubles to 4 floats via two 128-bit loads
        __m128 low  = _mm_cvtpd_ps(_mm_loadu_pd(&xr[cnt]));
        __m128 high = _mm_cvtpd_ps(_mm_loadu_pd(&xr[cnt + 2]));
        __m128 x_orig = _mm_movelh_ps(low, high);
#endif
        // Capture sign and Absolute value
        __m128 sign_mask = _mm_cmplt_ps(x_orig, zero);
        __m128 x = _mm_and_ps(x_orig, abs_mask);

        // Math: (x * sfac)^0.75 + magic
        // Logic: sqrt( (x*sfac) * sqrt(x*sfac) )
        x = _mm_mul_ps(x, sfac);
        x = _mm_mul_ps(x, _mm_sqrt_ps(x));
        x = _mm_sqrt_ps(x);
        x = _mm_add_ps(x, magic);

        // Convert to integer
        __m128i xi_vec = _mm_cvttps_epi32(x);

        // Bitwise Sign Fix: (val ^ mask) - mask
        __m128i m_int = _mm_castps_si128(sign_mask);
        xi_vec = _mm_sub_epi32(_mm_xor_si128(xi_vec, m_int), m_int);

        _mm_storeu_si128((__m128i*)&xi[cnt], xi_vec);
    }

    // Safe scalar remainder loop for widths not multiple of 4
    for (; cnt < n; cnt++)
    {
	faac_real val = xr[cnt];
        faac_real tmp = FAAC_FABS(val);
        tmp *= sfacfix;
        tmp = FAAC_SQRT(tmp * FAAC_SQRT(tmp));
        int q = (int)(tmp + (faac_real)MAGIC_NUMBER);
        xi[cnt] = (val < 0) ? -q : q;
    }
}
