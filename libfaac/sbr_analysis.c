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

#include "sbr_analysis.h"
#include "sbr.h"
#include "sbr_internal.h"
#include "util.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Multi-pass signal analysis: transient detection, temporal grid selection,
 * and subband energy accumulation. hot keeps it vectorized under LTO despite
 * only being reached through the cold dispatcher; SbrQmfAnalysis is inlined
 * here (not split out) to stay under GCC's LTO auto-inline threshold. */
#if defined(__GNUC__)
__attribute__((hot))
#endif
void SbrAnalyze(SignalAnalysis *sa, faac_real *fullPtrs[], int nch, int numSamples, struct SBRInfo *sbr)
{
    int num_slots = numSamples / SBR_QMF_BANDS_64;
    int sampled = (num_slots - 1) / FAAC_SBR_DECIMATION + 1;
    faac_real workspace[SBR_QMF_OVL_LEN_64 + 2 * FRAME_LEN];

    sa->valid = 1;
    sa->numSlots = num_slots;
    sa->sampled = sampled;

    /* Pass 1: Time-domain transient detection. Identifies the temporal position
     * and strength of transients across all channels. */
    for (int ch = 0; ch < nch; ch++) {
        faac_real smax = (faac_real)0.0, ssum = (faac_real)0.0;
        int smax_idx = 0;
        faac_real slot_hp_eng[128]; /* high-pass energy per slot (max slots = 2*1024/64 = 32) */

        sa->ch[ch].wantShort = 0;
        faac_real val_in = sa->ch[ch].lastVal;
        const faac_real * restrict p_in = fullPtrs[ch];
        for (int slot = 0; slot < num_slots; slot++) {
            faac_real stot = (faac_real)0.0;
            faac_real hp_stot = (faac_real)0.0;
            for (int n = 0; n < SBR_QMF_BANDS_64; n += 4) {
                faac_real v0 = p_in[0], v1 = p_in[1], v2 = p_in[2], v3 = p_in[3];
                stot += v0 * v0 + v1 * v1 + v2 * v2 + v3 * v3;
                faac_real d0 = v0 - val_in, d1 = v1 - v0, d2 = v2 - v1, d3 = v3 - v2;
                hp_stot += d0 * d0 + d1 * d1 + d2 * d2 + d3 * d3;
                val_in = v3; p_in += 4;
            }
            if (slot < 128) slot_hp_eng[slot] = hp_stot;

            if (stot > smax) {
                smax = stot;
                smax_idx = slot;
            }
            ssum += stot;
        }
        sa->ch[ch].lastVal = val_in;

        sa->ch[ch].transientStrength = smax * (faac_real)sampled / (ssum + SBR_ENERGY_FLOOR);
        sa->ch[ch].transientSlot = smax_idx;

        /* Evaluate relative energy jumps to inform block switching. */
        faac_real last_hp_eng = 0.0;
        int have_last = 0;
        for (int slot = 0; slot < num_slots; slot++) {
            if (slot >= 128) break;
            faac_real hp_eng = slot_hp_eng[slot];
            if (have_last) {
                faac_real toteng = (hp_eng < last_hp_eng) ? hp_eng : last_hp_eng;
                faac_real volchg = (hp_eng > last_hp_eng) ? (hp_eng - last_hp_eng) : (last_hp_eng - hp_eng);
                /* PSY_TD_THRESH = 0.5 */
                if (volchg > ((faac_real)0.5 * toteng)) {
                    sa->ch[ch].wantShort = 1;
                    break;
                }
            }
            last_hp_eng = hp_eng;
            have_last = 1;
        }
    }

    /* Choose the temporal grid based on the strongest transient. Synchronizes
     * envelope borders across all channels to maintain spatial imaging. */
    faac_real frameStrength = (faac_real)0.0;
    int frameSlot = 0;
    for (int ch = 0; ch < nch; ch++) {
        if (sa->ch[ch].transientStrength > frameStrength) {
            frameStrength = sa->ch[ch].transientStrength;
            frameSlot = sa->ch[ch].transientSlot;
        }
    }

    int split = num_slots;   /* default: single envelope spans the whole frame */
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
        split = clamp_int(innerSbr * num_slots / SBR_NUM_TIME_SLOTS, 1, num_slots - 1);
    } else {
        sa->numEnvelopes = 1;
        sa->frameClass = SBR_FRAME_CLASS_FIXFIX;
        sa->tEnv[0] = 0;
        sa->tEnv[1] = SBR_NUM_TIME_SLOTS;
        sa->bsPointer = 0;
    }

    /* Count slots per envelope for power normalization. */
    sa->envSampled[0] = sa->envSampled[1] = 0;
    for (int slot = 0; slot < num_slots; slot++) {
#if FAAC_SBR_DECIMATION > 1
        if (slot % FAAC_SBR_DECIMATION != 0) continue;
#endif
        int h = (sa->numEnvelopes > 1 && slot >= split) ? 1 : 0;
        sa->envSampled[h]++;
    }
    if (sa->envSampled[0] < 1) sa->envSampled[0] = 1;
    if (sa->numEnvelopes > 1 && sa->envSampled[1] < 1) sa->envSampled[1] = 1;

    /* Pass 2: Subband analysis. Accumulates energy across QMF bands within
     * the selected temporal envelopes. */
    /* Only [kx, k2) feeds the envelope quantizer; bands below kx are core-coded
     * and never read, so skip their post-FFT extraction and accumulation. */
    int kx = sbr ? sbr->kx : 0;
    int kEnd = sbr ? sbr->k2 : SBR_QMF_BANDS_64;
    for (int ch = 0; ch < nch; ch++) {
        memset(sa->ch[ch].bandHalfE, 0, sizeof(sa->ch[ch].bandHalfE));

        if (sbr) {
            memcpy(workspace, sbr->ch[ch].qmfOvl64, SBR_QMF_OVL_LEN_64 * sizeof(faac_real));
            memcpy(workspace + SBR_QMF_OVL_LEN_64, fullPtrs[ch], numSamples * sizeof(faac_real));

            for (int slot = 0; slot < num_slots; slot++) {
#if FAAC_SBR_DECIMATION > 1
                if (slot % FAAC_SBR_DECIMATION == 0)
#endif
                {
                    faac_real slotEnergy[SBR_QMF_BANDS_64];
                    SbrQmfAnalysis(sbr, workspace + slot * SBR_QMF_BANDS_64, slotEnergy, kx, kEnd);

                    int h = (sa->numEnvelopes > 1 && slot >= split) ? 1 : 0;

                    faac_real * restrict bE = sa->ch[ch].bandHalfE[h];
                    for (int k = kx; k < kEnd; k++)
                        bE[k] += slotEnergy[k];
                }
            }
        }
    }
}
