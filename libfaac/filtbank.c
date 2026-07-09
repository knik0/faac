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

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "coder.h"
#include "filtbank.h"
#include "frame.h"
#include "fft.h"
#include "util.h"

/* Sine and Kaiser-Bessel-Derived windows, ISO/IEC 13818-7 Annex 4.6.4.
 * KBD argument uses the product form i*(halfLen-i) (a difference-of-
 * squares factoring of the spec's (2i/halfLen-1)^2 shape term) so the
 * Bessel argument is one multiply instead of a subtract-then-square.
 * These tables are built once at encoder init, so the series and
 * normalization run in double (rounding to float only when stored into
 * win[]) for a correctly-rounded table at zero runtime cost. */

static double BesselI0(double x)
{
    const double tolerance = DBL_EPSILON;
    double halfX = x * 0.5;
    double term = 1.0;
    double series = 1.0;
    int k = 1;

    do {
        double ratio = halfX / (double)k;
        term *= ratio * ratio;
        series += term;
        k++;
    } while (term > tolerance * series);

    return series;
}

static void FillSineWindow(float *win, int halfLen)
{
    int i;

    for (i = 0; i < halfLen; i++)
        win[i] = (float)sin((M_PI_DOUBLE / (2 * halfLen)) * (i + 0.5));
}

static void FillKbdWindow(float *win, int halfLen, double alpha)
{
    const double omega = alpha * M_PI_DOUBLE / (double)halfLen;
    const double alpha2 = 4.0 * omega * omega;
    const int quarterLen = halfLen / 2;
    double shapeTerm[BLOCK_LEN_LONG / 2 + 1];
    double weightedTotal = 0.0;
    double running = 0.0;
    double scale;
    int i;

    /* Symmetric around quarterLen, so interior terms count twice below. */
    for (i = 0; i <= quarterLen; i++) {
        double symmetric = (double)i * (double)(halfLen - i) * alpha2;
        int isInterior = (i > 0) && (i < quarterLen);

        shapeTerm[i] = BesselI0(sqrt(symmetric));
        weightedTotal += shapeTerm[i] * (isInterior ? 2 : 1);
    }
    scale = 1.0 / (weightedTotal + 1.0);

    for (i = 0; i <= quarterLen; i++) {
        running += shapeTerm[i];
        win[i] = (float)sqrt(running * scale);
    }
    /* Past the midpoint, reuse the mirrored term instead of recomputing it. */
    for (; i < halfLen; i++) {
        running += shapeTerm[halfLen - i];
        win[i] = (float)sqrt(running * scale);
    }
}

typedef struct {
    float *sine;
    float *kbd;
} WindowPair;

static void BuildWindowPair(WindowPair *wp, int halfLen, double kbdAlpha)
{
    FillSineWindow(wp->sine, halfLen);
    FillKbdWindow(wp->kbd, halfLen, kbdAlpha);
}

void FilterBankInit(faacEncStruct* hEncoder)
{
    unsigned int channel;
    WindowPair longPair, shortPair;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        hEncoder->freqBuff[channel] = (float*)AllocMemory(2*FRAME_LEN*sizeof(float));
        if (!hEncoder->freqBuff[channel]) return;
    }

    hEncoder->sin_window_long = (float*)AllocMemory(BLOCK_LEN_LONG*sizeof(float));
    hEncoder->sin_window_short = (float*)AllocMemory(BLOCK_LEN_SHORT*sizeof(float));
    hEncoder->kbd_window_long = (float*)AllocMemory(BLOCK_LEN_LONG*sizeof(float));
    hEncoder->kbd_window_short = (float*)AllocMemory(BLOCK_LEN_SHORT*sizeof(float));

    if (!hEncoder->sin_window_long || !hEncoder->sin_window_short ||
        !hEncoder->kbd_window_long || !hEncoder->kbd_window_short)
        return;

    longPair.sine = hEncoder->sin_window_long;
    longPair.kbd = hEncoder->kbd_window_long;
    shortPair.sine = hEncoder->sin_window_short;
    shortPair.kbd = hEncoder->kbd_window_short;

    BuildWindowPair(&longPair, BLOCK_LEN_LONG, 4.0);
    BuildWindowPair(&shortPair, BLOCK_LEN_SHORT, 6.0);

    hEncoder->gpsyInfo.sharedWorkBuffLong = (float*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(float));
}

void FilterBankEnd(faacEncStruct* hEncoder)
{
    unsigned int channel;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        if (hEncoder->freqBuff[channel]) FreeMemory(hEncoder->freqBuff[channel]);
    }

    if (hEncoder->sin_window_long) FreeMemory(hEncoder->sin_window_long);
    if (hEncoder->sin_window_short) FreeMemory(hEncoder->sin_window_short);
    if (hEncoder->kbd_window_long) FreeMemory(hEncoder->kbd_window_long);
    if (hEncoder->kbd_window_short) FreeMemory(hEncoder->kbd_window_short);

    if (hEncoder->gpsyInfo.sharedWorkBuffLong) FreeMemory(hEncoder->gpsyInfo.sharedWorkBuffLong);
}

/* Four ICS window sequences, ISO/IEC 13818-7 4.3.2.4. */

typedef struct {
    float *dst;
    const float *src;
    const float *win;
    int len;
    bool reverse;
} WindowSeg;

static void ApplyWindowSeg(const WindowSeg *seg)
{
    int i;

    if (seg->reverse) {
        for (i = 0; i < seg->len; i++)
            seg->dst[i] = seg->src[i] * seg->win[seg->len - 1 - i];
    } else {
        for (i = 0; i < seg->len; i++)
            seg->dst[i] = seg->src[i] * seg->win[i];
    }
}

static void CopyFlat(float *dst, const float *src, int len)
{
    memcpy(dst, src, len * sizeof(float));
}

static void ZeroFlat(float *dst, int len)
{
    SetMemory(dst, 0, len * sizeof(float));
}

static const float *SelectWindow(faacEncStruct *hEncoder, int shape, bool isLong)
{
    if (shape == KBD_WINDOW)
        return isLong ? hEncoder->kbd_window_long : hEncoder->kbd_window_short;
    return isLong ? hEncoder->sin_window_long : hEncoder->sin_window_short;
}

void FilterBank(faacEncStruct* hEncoder,
                CoderInfo *coderInfo,
                float * restrict p_prev_data,
                float * restrict p_in_data,
                float * restrict p_out_mdct)
{
    float * restrict overlapBuf = hEncoder->gpsyInfo.sharedWorkBuffLong;
    int block_type = coderInfo->block_type;
    const float *leftWin, *rightWin;
    int k;

    /* Assemble the 2048-sample overlap window from the previous and
       current frame's time-domain samples. */
    memcpy(overlapBuf, p_prev_data, BLOCK_LEN_LONG*sizeof(float));
    memcpy(overlapBuf+BLOCK_LEN_LONG, p_in_data, BLOCK_LEN_LONG*sizeof(float));

    /* isLong is a literal per case below, not carried in from before the
       switch, so SelectWindow's dispatch folds to a single compare. */
    switch (block_type) {
    case ONLY_LONG_WINDOW: {
        WindowSeg left  = { p_out_mdct, overlapBuf,
                             SelectWindow(hEncoder, coderInfo->prev_window_shape, true), BLOCK_LEN_LONG, false };
        WindowSeg right = { p_out_mdct+BLOCK_LEN_LONG, overlapBuf+BLOCK_LEN_LONG,
                             SelectWindow(hEncoder, coderInfo->window_shape, true), BLOCK_LEN_LONG, true };

        ApplyWindowSeg(&left);
        ApplyWindowSeg(&right);
        MDCT(&hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG, hEncoder->gpsyInfo.sharedWorkBuffLong);
        break;
    }

    case LONG_SHORT_WINDOW: {
        WindowSeg left  = { p_out_mdct, overlapBuf,
                             SelectWindow(hEncoder, coderInfo->prev_window_shape, true), BLOCK_LEN_LONG, false };
        WindowSeg right = { p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS, overlapBuf+BLOCK_LEN_LONG+NFLAT_LS,
                             SelectWindow(hEncoder, coderInfo->window_shape, false), BLOCK_LEN_SHORT, true };

        ApplyWindowSeg(&left);
        CopyFlat(p_out_mdct+BLOCK_LEN_LONG, overlapBuf+BLOCK_LEN_LONG, NFLAT_LS);
        ApplyWindowSeg(&right);
        ZeroFlat(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT, NFLAT_LS);
        MDCT(&hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG, hEncoder->gpsyInfo.sharedWorkBuffLong);
        break;
    }

    case SHORT_LONG_WINDOW: {
        WindowSeg left  = { p_out_mdct+NFLAT_LS, overlapBuf+NFLAT_LS,
                             SelectWindow(hEncoder, coderInfo->prev_window_shape, false), BLOCK_LEN_SHORT, false };
        WindowSeg right = { p_out_mdct+BLOCK_LEN_LONG, overlapBuf+BLOCK_LEN_LONG,
                             SelectWindow(hEncoder, coderInfo->window_shape, true), BLOCK_LEN_LONG, true };

        ZeroFlat(p_out_mdct, NFLAT_LS);
        ApplyWindowSeg(&left);
        CopyFlat(p_out_mdct+NFLAT_LS+BLOCK_LEN_SHORT, overlapBuf+NFLAT_LS+BLOCK_LEN_SHORT, NFLAT_LS);
        ApplyWindowSeg(&right);
        MDCT(&hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG, hEncoder->gpsyInfo.sharedWorkBuffLong);
        break;
    }

    case ONLY_SHORT_WINDOW: {
        float *src = overlapBuf + NFLAT_LS;
        float *dst = p_out_mdct;

        leftWin  = SelectWindow(hEncoder, coderInfo->prev_window_shape, false);
        rightWin = SelectWindow(hEncoder, coderInfo->window_shape, false);

        for (k = 0; k < MAX_SHORT_WINDOWS; k++) {
            WindowSeg left  = { dst, src, leftWin, BLOCK_LEN_SHORT, false };
            WindowSeg right = { dst+BLOCK_LEN_SHORT, src+BLOCK_LEN_SHORT, rightWin, BLOCK_LEN_SHORT, true };

            ApplyWindowSeg(&left);
            ApplyWindowSeg(&right);
            MDCT(&hEncoder->fft_tables, dst, 2*BLOCK_LEN_SHORT, hEncoder->gpsyInfo.sharedWorkBuffLong);

            dst += BLOCK_LEN_SHORT;
            src += BLOCK_LEN_SHORT;
            leftWin = rightWin;
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
