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

#ifndef _FFT_H_
#define _FFT_H_

typedef float fftfloat;

#define FFT_LOGM_SHORT 6  /* 256-sample short block MDCT, SBR QMF */
#define FFT_LOGM_LONG  9  /* 2048-sample long block MDCT */

/* Offset of a transform size's slice inside a shared two-size table. */
#define FFT_TBL_OFFSET(logm) ((logm) == FFT_LOGM_SHORT ? 0 : (1 << FFT_LOGM_SHORT))
#define FFT_TBL_LEN ((1 << FFT_LOGM_SHORT) + (1 << FFT_LOGM_LONG))

/* Builds the process-wide twiddle tables; the caller runs it exactly once. */
void fft_init(void);

/* Complex FFT of x into y, natural order; each holds the real half then the
 * imaginary half, 2 << logm floats. x is used as scratch and destroyed. */
void fft(float * restrict x, float * restrict y, int logm);

#endif
