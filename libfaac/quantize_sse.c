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

int quantize_sse2(const float * __restrict xr, int * __restrict xi, int n4, float sfacfix)
{
    const __m128 sfac = _mm_set1_ps(sfacfix);
    const __m128 magic = _mm_set1_ps(MAGIC_NUMBER);
    // Mask to strip the sign bit (0x7FFFFFFF)
    const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    __m128i max_vec = _mm_setzero_si128();
    int maxq_arr[4];
    int cnt, maxq;

    // Process 4 elements per iteration; band widths are multiples of 4
    for (cnt = 0; cnt < 4 * n4; cnt += 4)
    {
        __m128 x_orig = _mm_loadu_ps(&xr[cnt]);
        // Absolute value of the scaled input
        __m128 x = _mm_and_ps(_mm_mul_ps(x_orig, sfac), abs_mask);
        __m128i q, mask;

        // Math: (x * sfac)^0.75 + magic
        // Logic: sqrt( (x*sfac) * sqrt(x*sfac) )
        x = _mm_mul_ps(x, _mm_sqrt_ps(x));
        x = _mm_sqrt_ps(x);
        x = _mm_add_ps(x, magic);

        // Convert to integer
        q = _mm_cvttps_epi32(x);
        mask = _mm_cmpgt_epi32(q, max_vec);
        max_vec = _mm_or_si128(_mm_and_si128(mask, q), _mm_andnot_si128(mask, max_vec));

        // Bitwise Sign Fix: (val ^ mask) - mask, mask = sign bit of the input
        mask = _mm_srai_epi32(_mm_castps_si128(x_orig), 31);
        q = _mm_sub_epi32(_mm_xor_si128(q, mask), mask);

        _mm_storeu_si128((__m128i*)&xi[cnt], q);
    }

    _mm_storeu_si128((__m128i*)maxq_arr, max_vec);
    maxq = maxq_arr[0];
    if (maxq_arr[1] > maxq) maxq = maxq_arr[1];
    if (maxq_arr[2] > maxq) maxq = maxq_arr[2];
    if (maxq_arr[3] > maxq) maxq = maxq_arr[3];

    return maxq;
}
