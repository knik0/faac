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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* 2:1 FIR downsampler for HE-AAC core signal preparation.
 * Takes full-rate PCM (Fs) and produces half-rate PCM (Fs/2)
 * for the AAC-LC core encoder. */

#ifndef RESAMPLE_H
#define RESAMPLE_H

#include "faac_real.h"
#include "coder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef float resfloat;

#define RESAMPLE_FILTER_LEN 63

typedef struct Resampler {
    faac_real  buf     [MAX_CHANNELS][RESAMPLE_FILTER_LEN]; /* FIR overlap state (carries between frames) */
    faac_real  fullRate[MAX_CHANNELS][2 * FRAME_LEN];       /* full-rate input: caller fills, SBR reads, FIR consumes */
    faac_real  halfRate[MAX_CHANNELS][FRAME_LEN];           /* downsampled output: written by Resample */
    int        channels;
} Resampler;

Resampler *ResampleInit(int channels);
void ResampleEnd(Resampler *r);

int Resample(Resampler *r, int input_len);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_H */
