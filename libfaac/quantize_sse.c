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
 */

#include <immintrin.h>
#include <math.h>
#include "quantize.h"

int quantize_sse2(const float * __restrict xr, int * __restrict xi, int n, float sfacfix)
{
    const __m128 zero = _mm_setzero_ps();
    const __m128 sfac = _mm_set1_ps(sfacfix);
    const __m128 magic = _mm_set1_ps(MAGIC_NUMBER);
    // Mask to strip the sign bit (0x7FFFFFFF)
    const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    __m128i max_vec = _mm_setzero_si128();
    int cnt = 0;

    // Process 4 elements per iteration
    for (; cnt <= n - 4; cnt += 4)
    {
        __m128 x_orig = _mm_loadu_ps((const float*)&xr[cnt]);
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
        __m128i mask = _mm_cmpgt_epi32(xi_vec, max_vec);
        max_vec = _mm_or_si128(_mm_and_si128(mask, xi_vec), _mm_andnot_si128(mask, max_vec));

        // Bitwise Sign Fix: (val ^ mask) - mask
        __m128i m_int = _mm_castps_si128(sign_mask);
        xi_vec = _mm_sub_epi32(_mm_xor_si128(xi_vec, m_int), m_int);

        _mm_storeu_si128((__m128i*)&xi[cnt], xi_vec);
    }

    int maxq_arr[4];
    _mm_storeu_si128((__m128i*)maxq_arr, max_vec);
    int maxq = maxq_arr[0];
    if (maxq_arr[1] > maxq) maxq = maxq_arr[1];
    if (maxq_arr[2] > maxq) maxq = maxq_arr[2];
    if (maxq_arr[3] > maxq) maxq = maxq_arr[3];

    // Safe scalar remainder loop for widths not multiple of 4
    for (; cnt < n; cnt++)
    {
        float val = xr[cnt];
        float tmp = fabsf(val);
        tmp *= sfacfix;
        tmp = sqrtf(tmp * sqrtf(tmp));
        int q = (int)(tmp + (float)MAGIC_NUMBER);
        if (q > maxq) maxq = q;
        xi[cnt] = (val < 0) ? -q : q;
    }

    return maxq;
}
