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

#ifndef SBR_ANALYSIS_H
#define SBR_ANALYSIS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SBR_QMF_BANDS_64
#define SBR_QMF_BANDS_64 64
#endif

#ifndef SBR_MAX_ENVELOPES
#define SBR_MAX_ENVELOPES 2
#endif

struct SBRInfo;

typedef struct SignalAnalysisChannel {
    int       transientSlot;
    float transientStrength;
} SignalAnalysisChannel;

typedef struct SignalAnalysis {
    int numSlots;
    int sampled;

    /* Frame envelope grid configuration. Synchronized across all channels. */
    SbrFrameClass frameClass;
    int numEnvelopes;
    int tEnv[SBR_MAX_ENVELOPES + 1];
    int bsPointer;
    int envSampled[SBR_MAX_ENVELOPES];

    /* Block switching needs a decision for every core channel, so pass 1 runs
       full width. */
    SignalAnalysisChannel ch[MAX_CHANNELS];

    /* Per-envelope QMF band energy, binned over the grid above; only the first
       numEnvelopes rows are written. */
    float bandE[MAX_CHANNELS][SBR_MAX_ENVELOPES][SBR_QMF_BANDS_64];
} SignalAnalysis;

void SbrAnalyze(SignalAnalysis *sa, float *fullPtrs[], int nch, const bool *isLfe, int numSamples, struct SBRInfo *sbr);

#ifdef __cplusplus
}
#endif

#endif
