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

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "coder.h"
#include "filtbank.h"
#include "frame.h"
#include "fft.h"
#include "util.h"

/* Sine windows, ISO/IEC 13818-7 4.6.4. Built once per process in double,
 * rounded to float when stored, and shared read-only by every handle. */
static float sin_window_long[BLOCK_LEN_LONG];
static float sin_window_short[BLOCK_LEN_SHORT];

static void FillSineWindow(float *win, int halfLen)
{
    int i;

    for (i = 0; i < halfLen; i++)
        win[i] = (float)sin((M_PI_DOUBLE / (2 * halfLen)) * (i + 0.5));
}

void FilterBankTablesInit(void)
{
    FillSineWindow(sin_window_long, BLOCK_LEN_LONG);
    FillSineWindow(sin_window_short, BLOCK_LEN_SHORT);
}

void FilterBankInit(faacEncStruct* hEncoder)
{
    unsigned int channel;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        hEncoder->freqBuff[channel] = (float*)AllocMemory(2*FRAME_LEN*sizeof(float));
        if (!hEncoder->freqBuff[channel]) return;
    }

    hEncoder->gpsyInfo.sharedWorkBuffLong = (float*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(float));
}

void FilterBankEnd(faacEncStruct* hEncoder)
{
    unsigned int channel;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        if (hEncoder->freqBuff[channel]) FreeMemory(hEncoder->freqBuff[channel]);
    }

    if (hEncoder->gpsyInfo.sharedWorkBuffLong) FreeMemory(hEncoder->gpsyInfo.sharedWorkBuffLong);
}

/* Four ICS window sequences, ISO/IEC 13818-7 4.3.2.4.
 * Applying sine windowing directly in vectorizable loops without indirect struct dispatch. */

static inline void ApplyWindowDirect(float * restrict dst,
                                     const float * restrict src,
                                     const float * restrict win,
                                     int len)
{
    int i;
    for (i = 0; i < len; i++) {
        dst[i] = src[i] * win[i];
    }
}

static inline void ApplyWindowReverse(float * restrict dst,
                                      const float * restrict src,
                                      const float * restrict win,
                                      int len)
{
    int i;
    for (i = 0; i < len; i++) {
        dst[i] = src[i] * win[len - 1 - i];
    }
}

static inline void CopyFlat(float * restrict dst, const float * restrict src, int len)
{
    memcpy(dst, src, len * sizeof(float));
}

static inline void ZeroFlat(float * restrict dst, int len)
{
    SetMemory(dst, 0, len * sizeof(float));
}

void FilterBank(faacEncStruct* hEncoder,
                CoderInfo *coderInfo,
                float * restrict p_prev_data,
                float * restrict p_in_data,
                float * restrict p_out_mdct)
{
    float * restrict overlapBuf = hEncoder->gpsyInfo.sharedWorkBuffLong;
    int block_type = coderInfo->block_type;
    int k;

    /* Assemble the 2048-sample overlap window from the previous and
       current frame's time-domain samples. */
    memcpy(overlapBuf, p_prev_data, BLOCK_LEN_LONG*sizeof(float));
    memcpy(overlapBuf+BLOCK_LEN_LONG, p_in_data, BLOCK_LEN_LONG*sizeof(float));

    switch (block_type) {
    case ONLY_LONG_WINDOW: {
        ApplyWindowDirect(p_out_mdct, overlapBuf, sin_window_long, BLOCK_LEN_LONG);
        ApplyWindowReverse(p_out_mdct+BLOCK_LEN_LONG, overlapBuf+BLOCK_LEN_LONG, sin_window_long, BLOCK_LEN_LONG);
        MDCT(&hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG, hEncoder->gpsyInfo.sharedWorkBuffLong);
        break;
    }

    case LONG_SHORT_WINDOW: {
        ApplyWindowDirect(p_out_mdct, overlapBuf, sin_window_long, BLOCK_LEN_LONG);
        CopyFlat(p_out_mdct+BLOCK_LEN_LONG, overlapBuf+BLOCK_LEN_LONG, NFLAT_LS);
        ApplyWindowReverse(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS, overlapBuf+BLOCK_LEN_LONG+NFLAT_LS, sin_window_short, BLOCK_LEN_SHORT);
        ZeroFlat(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT, NFLAT_LS);
        MDCT(&hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG, hEncoder->gpsyInfo.sharedWorkBuffLong);
        break;
    }

    case SHORT_LONG_WINDOW: {
        ZeroFlat(p_out_mdct, NFLAT_LS);
        ApplyWindowDirect(p_out_mdct+NFLAT_LS, overlapBuf+NFLAT_LS, sin_window_short, BLOCK_LEN_SHORT);
        CopyFlat(p_out_mdct+NFLAT_LS+BLOCK_LEN_SHORT, overlapBuf+NFLAT_LS+BLOCK_LEN_SHORT, NFLAT_LS);
        ApplyWindowReverse(p_out_mdct+BLOCK_LEN_LONG, overlapBuf+BLOCK_LEN_LONG, sin_window_long, BLOCK_LEN_LONG);
        MDCT(&hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG, hEncoder->gpsyInfo.sharedWorkBuffLong);
        break;
    }

    case ONLY_SHORT_WINDOW: {
        const float * restrict win = sin_window_short;
        float * restrict src = overlapBuf + NFLAT_LS;
        float * restrict dst = p_out_mdct;

        for (k = 0; k < MAX_SHORT_WINDOWS; k++) {
            ApplyWindowDirect(dst, src, win, BLOCK_LEN_SHORT);
            ApplyWindowReverse(dst+BLOCK_LEN_SHORT, src+BLOCK_LEN_SHORT, win, BLOCK_LEN_SHORT);
            MDCT(&hEncoder->fft_tables, dst, 2*BLOCK_LEN_SHORT, hEncoder->gpsyInfo.sharedWorkBuffLong);

            dst += BLOCK_LEN_SHORT;
            src += BLOCK_LEN_SHORT;
        }
        break;
    }
    }
}

void MDCT( FFT_Tables *fft_tables, float * restrict data, int N, float * restrict work )
{
    const int N2 = N >> 1;
    const int N4 = N >> 2;
    const int N8 = N >> 3;
    const int logm = (N == 2 * BLOCK_LEN_LONG) ? 9 : 6;

    const fftfloat * restrict cosT = fft_tables->mdct_cos[logm];
    const fftfloat * restrict sinT = fft_tables->mdct_sin[logm];

    float * restrict xr = work;
    float * restrict xi = work + N4;

    int i;

    /* Sign pattern flips at N/8 - the real input's symmetry folds
       differently on either side of that midpoint. */
    for (i = 0; i < N8; i++) {
        int n1 = N2 - 1 - 2*i;
        int n2 = 2*i;
        float foldedRe = data[N4 + n1] + data[N + N4 - 1 - n1];
        float foldedIm = data[N4 + n2] - data[N4 - 1 - n2];

        xr[i] = foldedRe * cosT[i] + foldedIm * sinT[i];
        xi[i] = foldedIm * cosT[i] - foldedRe * sinT[i];
    }
    for (; i < N4; i++) {
        int n1 = N2 - 1 - 2*i;
        int n2 = 2*i;
        float foldedRe = data[N4 + n1] - data[N4 - 1 - n1];
        float foldedIm = data[N4 + n2] + data[N + N4 - 1 - n2];

        xr[i] = foldedRe * cosT[i] + foldedIm * sinT[i];
        xi[i] = foldedIm * cosT[i] - foldedRe * sinT[i];
    }

    fft( fft_tables, xr, xi, logm);

    /* Unfold N/4 complex FFT outputs into N real coefficients, one write
       per output quarter. */
    for (i = 0; i < N4; i++) {
        int n2 = 2*i;
        float unfoldRe = 2.0f * (xr[i] * cosT[i] + xi[i] * sinT[i]);
        float unfoldIm = 2.0f * (xi[i] * cosT[i] - xr[i] * sinT[i]);

        data[n2]             = -unfoldRe;
        data[N2 - 1 - n2]    =  unfoldIm;
        data[N2 + n2]        = -unfoldIm;
        data[N - 1 - n2]     =  unfoldRe;
    }
}
