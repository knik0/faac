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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quantize.h"
#include "huff2.h"
#include "cpu_compute.h"

typedef void (*QuantizeFunc)(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix);

#if defined(HAVE_SSE2)
extern void quantize_sse2(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix);
#endif

static void quantize_scalar(const faac_real * __restrict xr, int * __restrict xi, int n, faac_real sfacfix)
{
    const faac_real magic = MAGIC_NUMBER;
    int i;
    for (i = 0; i < n; i++)
    {
        faac_real val = xr[i];
        faac_real tmp = FAAC_FABS(val) * sfacfix;
        tmp = FAAC_SQRT(tmp * FAAC_SQRT(tmp));
        int q = (int)(tmp + magic);
        xi[i] = (val < 0) ? -q : q;
    }
}

static QuantizeFunc qfunc = quantize_scalar;
static faac_real sfstep;
static faac_real max_quant_limit;

#define SF_CHAIN_UNSET INT_MIN

void QuantizeInit(void)
{
#if defined(HAVE_SSE2)
    CPUCaps caps = get_cpu_caps();
    if (caps & CPU_CAP_SSE2)
        qfunc = quantize_sse2;
    else
#endif
        qfunc = quantize_scalar;

    sfstep = SF_STEP_AMPL;
    max_quant_limit = FAAC_POW((faac_real)MAX_HUFF_ESC_VAL + 1.0 - MAGIC_NUMBER, 4.0/3.0);
}

/* sfac and gain are coupled; clamping one forces a recompute of the other. */
static faac_real gain_with_overflow_clamp(int *sfac, faac_real band_peak)
{
    faac_real gain = FAAC_POW(10, *sfac / sfstep);
    if (band_peak > 0.0 && gain * band_peak > max_quant_limit)
    {
        gain = max_quant_limit / band_peak;
        *sfac = (int)FAAC_FLOOR(FAAC_LOG10(gain) * sfstep);
        gain = FAAC_POW(10, *sfac / sfstep);
    }
    return gain;
}

// masking target per scalefactor band: 0 marks a band inaudible
#define SILENCE_RMS            0.4     // per-sample RMS gate for silence
#define AVG_ENERGY_WEIGHT      0.2     // noise-like (average-energy) share of the target
#define PEAK_ENERGY_WEIGHT     0.45    // tonal (peak-energy) share of the remainder
#define SHORT_BLOCK_TIGHTEN    0.45    // short blocks get a tighter target per unit of energy
#define LOUDNESS_EXPONENT      0.4     // Zwicker-ish loudness compression
#define AVG_ENERGY_FLOOR_FRAC  0.0010  // -30 dB floor, keeps quiet bands from collapsing the target
#define PEAK_ENERGY_FLOOR_FRAC 0.0050  // ~-23 dB floor, same purpose for peak energy

typedef struct
{
    faac_real sum;      /* energy summed across group windows */
    faac_real peak_amp; /* sqrt of the largest single-coefficient energy seen */
} BandEnergy;

static void measure_band_energy(const CoderInfo * __restrict ci, const faac_real * __restrict xr0,
                                 int gnum, BandEnergy * __restrict out)
{
    int gsize = ci->groups.len[gnum];
    int sfb;

    for (sfb = 0; sfb < ci->sfbn; sfb++)
    {
        int lo = ci->sfb_offset[sfb], hi = ci->sfb_offset[sfb + 1];
        faac_real sum = 0.0, peak = 0.0;
        int w;

        for (w = 0; w < gsize; w++)
        {
            const faac_real *line = xr0 + w * BLOCK_LEN_SHORT + lo;
            int k;
            for (k = 0; k < hi - lo; k++)
            {
                faac_real e = line[k] * line[k];
                sum += e;
                if (e > peak) peak = e;
            }
        }
        out[sfb].sum = sum;
        out[sfb].peak_amp = FAAC_SQRT(peak);
    }
}

static faac_real loudness(faac_real energy_ratio)
{
    return FAAC_POW(energy_ratio, LOUDNESS_EXPONENT);
}

// masking sensitivity drops above ~4 kHz; de-emphasize bands toward Nyquist
static faac_real treble_rolloff(int lo, int hi, faac_real inv_block_len)
{
    return 10.0 / (1.0 + (faac_real)(lo + hi) * inv_block_len);
}

static void derive_masking_targets(CoderInfo * __restrict ci, int gnum, faac_real quality,
                                    const BandEnergy * __restrict be,
                                    faac_real * __restrict target_out, faac_real * __restrict avg_out)
{
    int gsize = ci->groups.len[gnum];
    int total_len = ci->sfb_offset[ci->sfbn];
    faac_real group_total = 0.0;
    int sfb;

    for (sfb = 0; sfb < ci->sfbn; sfb++)
        group_total += be[sfb].sum;

    // whole group below the silence gate: force every band to a zero target
    if (group_total < (SILENCE_RMS * SILENCE_RMS) * (faac_real)(gsize * total_len))
    {
        for (sfb = 0; sfb < ci->sfbn; sfb++)
        {
            target_out[sfb] = 0.0;
            avg_out[sfb] = 0.0;
        }
        return;
    }

    int block_len = (ci->block_type == ONLY_SHORT_WINDOW) ? BLOCK_LEN_SHORT : BLOCK_LEN_LONG;
    faac_real inv_block_len = 1.0 / (faac_real)block_len;

    for (sfb = 0; sfb < ci->sfbn; sfb++)
    {
        int lo = ci->sfb_offset[sfb], hi = ci->sfb_offset[sfb + 1];
        faac_real avg = be[sfb].sum;
        faac_real peak = be[sfb].peak_amp * be[sfb].peak_amp;
        faac_real ref = (group_total * inv_block_len) * (hi - lo);
        faac_real target;

        // floor before pow(): formula is monotonic, so this floors the output too
        if (avg < ref * AVG_ENERGY_FLOOR_FRAC) avg = ref * AVG_ENERGY_FLOOR_FRAC;
        if (peak < ref * PEAK_ENERGY_FLOOR_FRAC) peak = ref * PEAK_ENERGY_FLOOR_FRAC;

        target = AVG_ENERGY_WEIGHT * loudness(avg / ref)
               + (1.0 - AVG_ENERGY_WEIGHT) * PEAK_ENERGY_WEIGHT * loudness(peak / ref);
        if (ci->block_type == ONLY_SHORT_WINDOW)
            target *= SHORT_BLOCK_TIGHTEN;
        target *= treble_rolloff(lo, hi, inv_block_len);

        target_out[sfb] = target * quality;
        avg_out[sfb] = be[sfb].sum;
    }
}

// per-band codebook assignment: zero / PNS / regular+Huffman

/* Re-derives gain after each clamp stage since scalefactor and gain are
 * coupled. Reports the final relative (bitstream-delta) and absolute
 * scalefactors. */
static faac_real resolve_band_gain(int sfac, int sf_bias, faac_real band_peak, int last_abs,
                                    int * __restrict out_sf_rel, int * __restrict out_sf_abs)
{
    faac_real gain = gain_with_overflow_clamp(&sfac, band_peak);
    int sf_rel = SF_OFFSET - sfac;
    int sf_abs = sf_bias + sf_rel;

    if (last_abs != SF_CHAIN_UNSET)
    {
        int wanted = sf_abs - last_abs;
        int allowed = clamp_sf_diff(wanted);
        if (allowed != wanted)
        {
            sf_abs = last_abs + allowed;
            sf_rel = sf_abs - sf_bias;
            sfac = SF_OFFSET - sf_rel;
            gain = gain_with_overflow_clamp(&sfac, band_peak);
            sf_rel = SF_OFFSET - sfac;
            sf_abs = sf_bias + sf_rel;
        }
    }

    if (sf_abs < 0 || sf_abs > SF_MAX_ABS)
    {
        sf_abs = (sf_abs < 0) ? 0 : SF_MAX_ABS;
        sf_rel = sf_abs - sf_bias;
        sfac = SF_OFFSET - sf_rel;
        gain = gain_with_overflow_clamp(&sfac, band_peak);
        sf_rel = SF_OFFSET - sfac;
        sf_abs = sf_bias + sf_rel;
    }

    *out_sf_rel = sf_rel;
    *out_sf_abs = sf_abs;
    return gain;
}

static void assign_band_codebooks(CoderInfo * __restrict ci, const faac_real * __restrict xr0,
                                   const faac_real * __restrict target, const faac_real * __restrict bandenrg,
                                   const faac_real * __restrict bandpeak, int gnum, int pnslevel,
                                   int * __restrict p_last_abs)
{
    int gsize = ci->groups.len[gnum];
    faac_real pns_threshold = 0.1 * (faac_real)pnslevel;
    int sb;

    for (sb = 0; sb < ci->sfbn && ci->bandcnt < MAX_SCFAC_BANDS; sb++)
    {
        int band = ci->bandcnt;

        if (ci->book[band] != HCB_NONE)
        {
            ci->bandcnt++;
            continue;
        }

        int lo = ci->sfb_offset[sb], hi = ci->sfb_offset[sb + 1];
        int width = hi - lo;
        faac_real avg_per_window = bandenrg[sb] / (faac_real)gsize;
        faac_real rms = FAAC_SQRT(avg_per_window / width);

        if (rms < SILENCE_RMS || target[sb] == 0.0)
        {
            ci->book[band] = HCB_ZERO;
            ci->bandcnt++;
            continue;
        }

        if (target[sb] < pns_threshold)
        {
            ci->book[band] = HCB_PNS;
            ci->sf[band] += FAAC_LRINT(FAAC_LOG10(avg_per_window) * SF_STEP_ENRG);
            ci->bandcnt++;
            continue;
        }

        int sfac = FAAC_LRINT(FAAC_LOG10(target[sb] / rms) * sfstep);
        int sf_rel = SF_OFFSET - sfac;
        int sf_bias = ci->sf[band];

        if (sf_rel < SF_MIN)
        {
            ci->book[band] = HCB_ZERO;
        }
        else
        {
            int sf_abs;
            faac_real gain = resolve_band_gain(sfac, sf_bias, bandpeak[sb], *p_last_abs, &sf_rel, &sf_abs);
            int xi[FRAME_LEN];
            int win;

            for (win = 0; win < gsize; win++)
                qfunc(xr0 + win * BLOCK_LEN_SHORT + lo, xi + win * width, width, gain);
            huffbook(ci, xi, gsize * width);
            *p_last_abs = sf_abs;
        }

        ci->sf[ci->bandcnt++] += sf_rel;
    }
}

void ResetCoderSections(CoderInfo *coder)
{
    int i, n = coder->groups.n * coder->sfbn;
    for (i = 0; i < n; i++)
    {
        coder->book[i] = HCB_NONE;
        coder->sf[i] = 0;
    }
}

int BlocQuant(CoderInfo * __restrict coder, faac_real * __restrict xr, AACQuantCfg *aacquantCfg)
{
    faac_real target[MAX_SCFAC_BANDS], bandenrg[MAX_SCFAC_BANDS];
    BandEnergy be[NSFB_LONG];
    faac_real bandpeak[MAX_SCFAC_BANDS];
    int i, lastsf = SF_CHAIN_UNSET;
    faac_real *gxr = xr;

    coder->bandcnt = coder->datacnt = 0;
    for (i = 0; i < coder->groups.n; i++)
    {
        int sfb;

        measure_band_energy(coder, gxr, i, be);
        for (sfb = 0; sfb < coder->sfbn; sfb++)
            bandpeak[sfb] = be[sfb].peak_amp;
        derive_masking_targets(coder, i, (faac_real)aacquantCfg->quality / DEFQUAL, be, target, bandenrg);
        assign_band_codebooks(coder, gxr, target, bandenrg, bandpeak, i, aacquantCfg->pnslevel, &lastsf);
        gxr += coder->groups.len[i] * BLOCK_LEN_SHORT;
    }

    // global_gain must come from a regular band: it's an 8-bit bitstream field,
    // and intensity/PNS bands store stereo-position/noise-energy on a different
    // (possibly negative) scale that would truncate and desync the decoder.
    coder->global_gain = 0;
    for (i = 0; i < coder->bandcnt; i++)
    {
        int b = coder->book[i];
        if (b && b != HCB_INTENSITY && b != HCB_INTENSITY2 && b != HCB_PNS)
        {
            coder->global_gain = coder->sf[i];
            break;
        }
    }

    int lastis = 0, lastpns = coder->global_gain - SF_PNS_OFFSET;
    for (i = 0; i < coder->bandcnt; i++)
    {
        int b = coder->book[i];
        if (b == HCB_INTENSITY || b == HCB_INTENSITY2)
        {
            int diff = clamp_sf_diff(coder->sf[i] - lastis);
            lastis += diff;
            coder->sf[i] = lastis;
        }
        else if (b == HCB_PNS)
        {
            int diff = clamp_sf_diff(coder->sf[i] - lastpns);
            lastpns += diff;
            coder->sf[i] = lastpns;
        }
    }
    return 1;
}

void CalcBW(unsigned *bw, int rate, SR_INFO *sr, AACQuantCfg *aacquantCfg)
{
    int i, l = 0, max = *bw * (BLOCK_LEN_SHORT << 1) / rate;
    for (i = 0; i < sr->num_cb_short && l < max; i++)
        l += sr->cb_width_short[i];
    aacquantCfg->max_cbs = i;
    if (aacquantCfg->pnslevel) *bw = (faac_real)l * rate / (BLOCK_LEN_SHORT << 1);

    l = 0, max = *bw * (BLOCK_LEN_LONG << 1) / rate;
    for (i = 0; i < sr->num_cb_long && l < max; i++)
        l += sr->cb_width_long[i];
    aacquantCfg->max_cbl = i;
    aacquantCfg->max_l = l;
    *bw = (faac_real)l * rate / (BLOCK_LEN_LONG << 1);
}

// short-window grouping: keep spectrally-similar windows together so they
// share scalefactors; a transient onset starts a fresh group instead
#define GROUP_MIN_SFB     2    // bands below this are too coarse/DC-heavy to inform grouping
#define GROUP_ONSET_RATIO 3.0  // running max/min energy ratio that counts as a transient

static void window_band_energy(const CoderInfo * __restrict ci, const faac_real * __restrict w,
                                int from_sfb, int to_sfb, faac_real * __restrict e_out)
{
    int sfb;
    for (sfb = from_sfb; sfb < to_sfb; sfb++)
    {
        faac_real e = 0.0;
        int k;
        for (k = ci->sfb_offset[sfb]; k < ci->sfb_offset[sfb + 1]; k++)
            e += w[k] * w[k];
        e_out[sfb] = e;
    }
}

void BlocGroup(faac_real *xr, CoderInfo *coderInfo, AACQuantCfg *cfg)
{
    if (coderInfo->block_type != ONLY_SHORT_WINDOW)
    {
        coderInfo->groups.n = 1;
        coderInfo->groups.len[0] = 1;
        return;
    }

    int maxsfb = cfg->max_cbs;
    int cutoff = cfg->max_l / 8;
    int active_bands = maxsfb - GROUP_MIN_SFB;
    int onset_quorum = (active_bands * 3) >> 2;

    faac_real band_e[NSFB_SHORT], run_min[NSFB_SHORT], run_max[NSFB_SHORT];
    int win, group_start = 0;

    coderInfo->groups.n = 0;

    for (win = 0; win < MAX_SHORT_WINDOWS; win++)
    {
        faac_real *w = xr + win * BLOCK_LEN_SHORT;
        int k, sfb;

        for (k = cutoff; k < coderInfo->sfb_offset[maxsfb]; k++)
            w[k] = 0.0;

        window_band_energy(coderInfo, w, GROUP_MIN_SFB, maxsfb, band_e);

        if (win == group_start)
        {
            for (sfb = GROUP_MIN_SFB; sfb < maxsfb; sfb++)
                run_min[sfb] = run_max[sfb] = band_e[sfb];
            continue;
        }

        int onset_votes = 0;
        for (sfb = GROUP_MIN_SFB; sfb < maxsfb; sfb++)
        {
            if (band_e[sfb] < run_min[sfb]) run_min[sfb] = band_e[sfb];
            if (band_e[sfb] > run_max[sfb]) run_max[sfb] = band_e[sfb];
            if (run_max[sfb] > GROUP_ONSET_RATIO * run_min[sfb]) onset_votes++;
        }

        if (onset_votes > onset_quorum)
        {
            coderInfo->groups.len[coderInfo->groups.n++] = win - group_start;
            group_start = win;
            for (sfb = GROUP_MIN_SFB; sfb < maxsfb; sfb++)
                run_min[sfb] = run_max[sfb] = band_e[sfb];
        }
    }
    coderInfo->groups.len[coderInfo->groups.n++] = MAX_SHORT_WINDOWS - group_start;
}
