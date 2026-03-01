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
#include <math.h>
#include "coder.h"

#define MAGIC_NUMBER_REAL ((faac_real)0.4054)

void quantize_sse2(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix)
{
#ifdef FAAC_PRECISION_DOUBLE
    const __m128d sfac = _mm_set1_pd(sfacfix);
    const __m128d magic = _mm_set1_pd(MAGIC_NUMBER_REAL);
    const __m128d zero = _mm_setzero_pd();
    // Mask to clear the sign bit for absolute value: 0x7FFFFFFFFFFFFFFF
    const __m128i abs_mask_i = _mm_set_epi32(0x7FFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF);
    const __m128d abs_mask = _mm_castsi128_pd(abs_mask_i);
    int cnt = 0;

    // 16-wide loop (8 x __m128d) to hide latency of sqrtpd
    if (n >= 16)
    {
        int n16 = n & ~15;
        for (; cnt < n16; cnt += 16)
        {
            __m128d x0 = _mm_loadu_pd(xr + cnt);
            __m128d x1 = _mm_loadu_pd(xr + cnt + 2);
            __m128d x2 = _mm_loadu_pd(xr + cnt + 4);
            __m128d x3 = _mm_loadu_pd(xr + cnt + 6);
            __m128d x4 = _mm_loadu_pd(xr + cnt + 8);
            __m128d x5 = _mm_loadu_pd(xr + cnt + 10);
            __m128d x6 = _mm_loadu_pd(xr + cnt + 12);
            __m128d x7 = _mm_loadu_pd(xr + cnt + 14);

            __m128i s0 = _mm_castpd_si128(_mm_cmplt_pd(x0, zero));
            __m128i s1 = _mm_castpd_si128(_mm_cmplt_pd(x1, zero));
            __m128i s2 = _mm_castpd_si128(_mm_cmplt_pd(x2, zero));
            __m128i s3 = _mm_castpd_si128(_mm_cmplt_pd(x3, zero));
            __m128i s4 = _mm_castpd_si128(_mm_cmplt_pd(x4, zero));
            __m128i s5 = _mm_castpd_si128(_mm_cmplt_pd(x5, zero));
            __m128i s6 = _mm_castpd_si128(_mm_cmplt_pd(x6, zero));
            __m128i s7 = _mm_castpd_si128(_mm_cmplt_pd(x7, zero));

            __m128d t0 = _mm_mul_pd(_mm_and_pd(x0, abs_mask), sfac);
            __m128d t1 = _mm_mul_pd(_mm_and_pd(x1, abs_mask), sfac);
            __m128d t2 = _mm_mul_pd(_mm_and_pd(x2, abs_mask), sfac);
            __m128d t3 = _mm_mul_pd(_mm_and_pd(x3, abs_mask), sfac);
            __m128d t4 = _mm_mul_pd(_mm_and_pd(x4, abs_mask), sfac);
            __m128d t5 = _mm_mul_pd(_mm_and_pd(x5, abs_mask), sfac);
            __m128d t6 = _mm_mul_pd(_mm_and_pd(x6, abs_mask), sfac);
            __m128d t7 = _mm_mul_pd(_mm_and_pd(x7, abs_mask), sfac);

            // Compute x^0.75 as sqrt(x * sqrt(x))
            t0 = _mm_sqrt_pd(_mm_mul_pd(t0, _mm_sqrt_pd(t0)));
            t1 = _mm_sqrt_pd(_mm_mul_pd(t1, _mm_sqrt_pd(t1)));
            t2 = _mm_sqrt_pd(_mm_mul_pd(t2, _mm_sqrt_pd(t2)));
            t3 = _mm_sqrt_pd(_mm_mul_pd(t3, _mm_sqrt_pd(t3)));
            t4 = _mm_sqrt_pd(_mm_mul_pd(t4, _mm_sqrt_pd(t4)));
            t5 = _mm_sqrt_pd(_mm_mul_pd(t5, _mm_sqrt_pd(t5)));
            t6 = _mm_sqrt_pd(_mm_mul_pd(t6, _mm_sqrt_pd(t6)));
            t7 = _mm_sqrt_pd(_mm_mul_pd(t7, _mm_sqrt_pd(t7)));

            __m128i q0 = _mm_cvttpd_epi32(_mm_add_pd(t0, magic));
            __m128i q1 = _mm_cvttpd_epi32(_mm_add_pd(t1, magic));
            __m128i q2 = _mm_cvttpd_epi32(_mm_add_pd(t2, magic));
            __m128i q3 = _mm_cvttpd_epi32(_mm_add_pd(t3, magic));
            __m128i q4 = _mm_cvttpd_epi32(_mm_add_pd(t4, magic));
            __m128i q5 = _mm_cvttpd_epi32(_mm_add_pd(t5, magic));
            __m128i q6 = _mm_cvttpd_epi32(_mm_add_pd(t6, magic));
            __m128i q7 = _mm_cvttpd_epi32(_mm_add_pd(t7, magic));

            // Branchless sign restoration using the 64-bit mask on 32-bit elements.
            // cvttpd_epi32 results in [0, 0, q1, q0]. mask is [m1_64, m0_64].
            // (q ^ s) - s handles both elements correctly.
            q0 = _mm_sub_epi32(_mm_xor_si128(q0, s0), s0);
            q1 = _mm_sub_epi32(_mm_xor_si128(q1, s1), s1);
            q2 = _mm_sub_epi32(_mm_xor_si128(q2, s2), s2);
            q3 = _mm_sub_epi32(_mm_xor_si128(q3, s3), s3);
            q4 = _mm_sub_epi32(_mm_xor_si128(q4, s4), s4);
            q5 = _mm_sub_epi32(_mm_xor_si128(q5, s5), s5);
            q6 = _mm_sub_epi32(_mm_xor_si128(q6, s6), s6);
            q7 = _mm_sub_epi32(_mm_xor_si128(q7, s7), s7);

            // Pack two [0, 0, q1, q0] vectors into one [q3, q2, q1, q0] vector for storage
            _mm_storeu_si128((__m128i*)(xi + cnt),      _mm_unpacklo_epi64(q0, q1));
            _mm_storeu_si128((__m128i*)(xi + cnt + 4),  _mm_unpacklo_epi64(q2, q3));
            _mm_storeu_si128((__m128i*)(xi + cnt + 8),  _mm_unpacklo_epi64(q4, q5));
            _mm_storeu_si128((__m128i*)(xi + cnt + 12), _mm_unpacklo_epi64(q6, q7));
        }
    }
#else
    const __m128 sfac = _mm_set1_ps(sfacfix);
    const __m128 magic = _mm_set1_ps((float)MAGIC_NUMBER_REAL);
    const __m128 zero = _mm_setzero_ps();
    const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    int cnt = 0;

    // 16-wide loop to hide latency of sqrtps
    if (n >= 16)
    {
        int n16 = n & ~15;
        for (; cnt < n16; cnt += 16)
        {
            __m128 x0 = _mm_loadu_ps(xr + cnt);
            __m128 x1 = _mm_loadu_ps(xr + cnt + 4);
            __m128 x2 = _mm_loadu_ps(xr + cnt + 8);
            __m128 x3 = _mm_loadu_ps(xr + cnt + 12);

            __m128i s0 = _mm_castps_si128(_mm_cmplt_ps(x0, zero));
            __m128i s1 = _mm_castps_si128(_mm_cmplt_ps(x1, zero));
            __m128i s2 = _mm_castps_si128(_mm_cmplt_ps(x2, zero));
            __m128i s3 = _mm_castps_si128(_mm_cmplt_ps(x3, zero));

            __m128 t0 = _mm_mul_ps(_mm_and_ps(x0, abs_mask), sfac);
            __m128 t1 = _mm_mul_ps(_mm_and_ps(x1, abs_mask), sfac);
            __m128 t2 = _mm_mul_ps(_mm_and_ps(x2, abs_mask), sfac);
            __m128 t3 = _mm_mul_ps(_mm_and_ps(x3, abs_mask), sfac);

            // Compute x^0.75 as sqrt(x * sqrt(x))
            t0 = _mm_sqrt_ps(_mm_mul_ps(t0, _mm_sqrt_ps(t0)));
            t1 = _mm_sqrt_ps(_mm_mul_ps(t1, _mm_sqrt_ps(t1)));
            t2 = _mm_sqrt_ps(_mm_mul_ps(t2, _mm_sqrt_ps(t2)));
            t3 = _mm_sqrt_ps(_mm_mul_ps(t3, _mm_sqrt_ps(t3)));

            __m128i i0 = _mm_cvttps_epi32(_mm_add_ps(t0, magic));
            __m128i i1 = _mm_cvttps_epi32(_mm_add_ps(t1, magic));
            __m128i i2 = _mm_cvttps_epi32(_mm_add_ps(t2, magic));
            __m128i i3 = _mm_cvttps_epi32(_mm_add_ps(t3, magic));

            // Branchless sign restoration
            i0 = _mm_sub_epi32(_mm_xor_si128(i0, s0), s0);
            i1 = _mm_sub_epi32(_mm_xor_si128(i1, s1), s1);
            i2 = _mm_sub_epi32(_mm_xor_si128(i2, s2), s2);
            i3 = _mm_sub_epi32(_mm_xor_si128(i3, s3), s3);

            _mm_storeu_si128((__m128i*)(xi + cnt), i0);
            _mm_storeu_si128((__m128i*)(xi + cnt + 4), i1);
            _mm_storeu_si128((__m128i*)(xi + cnt + 8), i2);
            _mm_storeu_si128((__m128i*)(xi + cnt + 12), i3);
        }
    }
#endif

    // Safe scalar remainder loop for widths not multiple of 4
    for (; cnt < n; cnt++)
    {
        faac_real val = xr[cnt];
        faac_real tmp = FAAC_FABS(val);
        tmp *= sfacfix;
        tmp = FAAC_SQRT(tmp * FAAC_SQRT(tmp));
        int q = (int)(tmp + MAGIC_NUMBER_REAL);
        xi[cnt] = (val < 0) ? -q : q;
    }
}
