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

#include "sbr.h"
#include "sbr_analysis.h"
#include "sbr_internal.h"
#include "util.h"
#include <string.h>

/* Which envelope a QMF slot falls in; slots before tEnv[0] fold into
 * envelope 0 rather than dropping their energy. */
static inline int sbr_env_of_slot(int numEnvelopes, const int *envStart, int slot)
{
    int e = 0;
    while (e + 1 < numEnvelopes && slot >= envStart[e + 1]) e++;
    return e;
}

/* Multi-pass signal analysis: transient detection, temporal grid selection,
 * and subband energy accumulation. */
void SbrAnalyze(SignalAnalysis *sa, float *fullPtrs[], int nch, const bool *isLfe, int numSamples, struct SBRInfo *sbr)
{
    int num_slots = numSamples / SBR_QMF_BANDS_64;
    int sampled = (num_slots - 1) / FAAC_SBR_DECIMATION + 1;
    float workspace[SBR_QMF_HIST_LEN + 2 * FRAME_LEN];

    sa->numSlots = num_slots;
    sa->sampled = sampled;

    /* Pass 1: Time-domain transient detection. Identifies the temporal position
     * and strength of transients across all channels. */
    for (int ch = 0; ch < nch; ch++) {
        float smax = 0.0f, ssum = 0.0f;
        int smax_idx = 0;
        for (int slot = 0; slot < num_slots; slot++) {
            /* The analysed frame starts SBR_ANALYSIS_DELAY back, in the saved history. */
            int pos = slot * SBR_QMF_BANDS_64 - SBR_ANALYSIS_DELAY;
            const float * restrict p_in = (pos < 0) ? sbr->ch[ch].qmfOvl64 + SBR_QMF_HIST_LEN + pos
                                                    : fullPtrs[ch] + pos;
            float stot = 0.0f;
            for (int n = 0; n < SBR_QMF_BANDS_64; n += 4) {
                float v0 = p_in[0], v1 = p_in[1], v2 = p_in[2], v3 = p_in[3];
                stot += v0 * v0 + v1 * v1 + v2 * v2 + v3 * v3;
                p_in += 4;
            }

            if (stot > smax) {
                smax = stot;
                smax_idx = slot;
            }
            ssum += stot;
        }

        sa->ch[ch].transientStrength = smax * (float)num_slots / (ssum + SBR_ENERGY_FLOOR);
        sa->ch[ch].transientSlot = smax_idx;
    }

    /* Choose the temporal grid based on the strongest transient. Synchronizes
     * envelope borders across all channels to maintain spatial imaging. The
     * LFE carries no SBR, so it gets no vote. */
    float frameStrength = 0.0f;
    int frameSlot = 0;
    for (int ch = 0; ch < nch; ch++) {
        if (isLfe[ch]) continue;
        if (sa->ch[ch].transientStrength > frameStrength) {
            frameStrength = sa->ch[ch].transientStrength;
            frameSlot = sa->ch[ch].transientSlot;
        }
    }

    if (frameStrength > SBR_TRANSIENT_THRESH_DEFAULT) {
        int Ts = (num_slots > 0) ? frameSlot * SBR_NUM_TIME_SLOTS / num_slots : 0; /* 0..16 */
        int rel = clamp_int((Ts - 2) / 2, 0, 3);
        int innerSbr = 2 * rel + 2;                  /* {2,4,6,8} */
        sa->numEnvelopes = 2;
        sa->frameClass = SBR_FRAME_CLASS_VARFIX;
        sa->tEnv[0] = 0;
        sa->tEnv[1] = innerSbr;
        sa->tEnv[2] = SBR_NUM_TIME_SLOTS;
        sa->bsPointer = 0;
    } else {
        sa->numEnvelopes = 1;
        sa->frameClass = SBR_FRAME_CLASS_FIXFIX;
        sa->tEnv[0] = 0;
        sa->tEnv[1] = SBR_NUM_TIME_SLOTS;
        sa->bsPointer = 0;
    }

    /* Envelope borders in QMF slots, for binning the per-slot energies below. */
    int envStart[SBR_MAX_ENVELOPES + 1];
    for (int e = 0; e <= sa->numEnvelopes; e++)
        envStart[e] = sa->tEnv[e] * num_slots / SBR_NUM_TIME_SLOTS;

    /* Count slots per envelope for power normalization. */
    for (int e = 0; e < sa->numEnvelopes; e++) sa->envSampled[e] = 0;
    for (int slot = 0; slot < num_slots; slot++) {
#if FAAC_SBR_DECIMATION > 1
        if (slot % FAAC_SBR_DECIMATION != 0) continue;
#endif
        sa->envSampled[sbr_env_of_slot(sa->numEnvelopes, envStart, slot)]++;
    }
    for (int e = 0; e < sa->numEnvelopes; e++)
        if (sa->envSampled[e] < 1) sa->envSampled[e] = 1;

    /* Pass 2: subband analysis, accumulating QMF band energy per envelope.
     * Only [kx, k2) feeds the quantizer, so skip bands below kx. */
    if (sbr) {
        int kx = sbr->kx;
        int kEnd = sbr->k2;
        for (int ch = 0; ch < nch; ch++) {
            if (isLfe[ch]) continue;
            memset(sa->bandE[ch], 0, sizeof(sa->bandE[ch]));

            memcpy(workspace, sbr->ch[ch].qmfOvl64, SBR_QMF_HIST_LEN * sizeof(float));
            memcpy(workspace + SBR_QMF_HIST_LEN, fullPtrs[ch], numSamples * sizeof(float));

            for (int slot = 0; slot < num_slots; slot++) {
#if FAAC_SBR_DECIMATION > 1
                if (slot % FAAC_SBR_DECIMATION == 0)
#endif
                {
                    float slotEnergy[SBR_QMF_BANDS_64];
                    SbrQmfAnalysis(sbr, workspace + slot * SBR_QMF_BANDS_64, slotEnergy, kx, kEnd);

                    int e = sbr_env_of_slot(sa->numEnvelopes, envStart, slot);

                    float * restrict bE = sa->bandE[ch][e];
                    for (int k = kx; k < kEnd; k++)
                        bE[k] += slotEnergy[k];
                }
            }
        }
    }
}
