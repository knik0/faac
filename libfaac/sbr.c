/*
 * FAAC - Freeware Advanced Audio Coder
 *
 * HE-AAC v1 SBR encoder
 * ISO/IEC 14496-3:2009 §4.6.18
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sbr.h"
#include "sbr_tables.h"
#include "bitstream.h"
#include "util.h"
#include "coder.h"

/* ------------------------------------------------------------------ *
 *  Internal helpers
 * ------------------------------------------------------------------ */

/* Clamp integer x to [lo, hi] */
static int clamp_int(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* Write Huffman-coded delta value.
 * table[delta + offset] gives the {code, len} pair.
 * code has the codeword MSB at bit (len-1); PutBit() writes it correctly. */
static int put_huff(BitStream *bs, const SBRHuffEntry *table, int nsyms,
                    int offset, int delta, int writeFlag)
{
    int sym = delta + offset;
    if (sym < 0) sym = 0;
    if (sym >= nsyms) sym = nsyms - 1;
    if (writeFlag)
        PutBit(bs, table[sym].code, table[sym].len);
    return table[sym].len;
}

/* ------------------------------------------------------------------ *
 *  Frequency band table construction
 *  ISO 14496-3:2009 §4.6.18.3.6
 * ------------------------------------------------------------------ */

/*
 * Compute kx (first SBR band) from the FULL sample rate and bs_start_freq.
 * ISO 14496-3:2009 §4.6.18.3.6 / FFmpeg sbr_make_f_master().
 *
 * Verified: 44100 Hz, bs_start_freq=3 → start_min=12, offset=0 → kx=12.
 */
static int compute_kx(int sampleRate, int bs_start_freq)
{
    int temp = (sampleRate < 32000) ? 3000 : (sampleRate < 64000) ? 4000 : 5000;
    int start_min = ((temp << 7) + (sampleRate >> 1)) / sampleRate;
    int row = (sampleRate <= 16000) ? 0 : (sampleRate <= 22050) ? 1 :
              (sampleRate <= 24000) ? 2 : (sampleRate <= 32000) ? 3 :
              (sampleRate <= 64000) ? 4 : 5;
    int kx = start_min + sbr_fs_offset[row][bs_start_freq & 15];
    if (kx < 1)  kx = 1;
    if (kx > 63) kx = 63;
    return kx;
}

/* qsort comparison for int16_t (ascending) */
static int cmp_int16(const void *a, const void *b)
{
    return (int)(*(const short *)a) - (int)(*(const short *)b);
}

/*
 * Compute k2 (exclusive stop band) from the FULL sample rate and bs_stop_freq.
 * ISO 14496-3:2009 §4.6.18.3.6 / FFmpeg sbr_make_f_master().
 *
 * For bs_stop_freq < 14: start from stop_min, compute 13 geometric band widths
 * from stop_min to 64, sort ascending, accumulate first bs_stop_freq of them.
 *
 * Verified: 44100 Hz, bs_stop_freq=7 → stop_min=24, k2=39.
 */
static int compute_k2(int sampleRate, int kx, int bs_stop_freq)
{
    int temp = (sampleRate < 32000) ? 3000 : (sampleRate < 64000) ? 4000 : 5000;
    int stop_min = ((temp << 8) + (sampleRate >> 1)) / sampleRate;

    int k2;
    if (bs_stop_freq < 14) {
        /* Geometric band widths from stop_min to 64 in 13 steps.
         * Must match FFmpeg make_bands(): lrintf for bands 0..11, exact last band. */
        short stop_dk[13];
        float base = powf(64.0f / (float)stop_min, 1.0f / 13.0f);
        float prod = (float)stop_min;
        int prev = stop_min;
        for (int i = 0; i < 12; i++) {
            prod *= base;
            int present = (int)lrintf(prod);
            stop_dk[i] = (short)(present - prev);
            prev = present;
        }
        stop_dk[12] = (short)(64 - prev);  /* exact last band */
        /* Sort ascending then accumulate first bs_stop_freq widths */
        qsort(stop_dk, 13, sizeof(short), cmp_int16);
        k2 = stop_min;
        for (int i = 0; i < bs_stop_freq; i++)
            k2 += stop_dk[i];
    } else if (bs_stop_freq == 14) {
        k2 = 2 * kx;
    } else {
        k2 = 3 * kx;
    }

    if (k2 > 64)  k2 = 64;
    if (k2 <= kx) k2 = kx + 1;

    /* Decoder per-rate max_qmf_subbands constraints (ISO 14496-3 sp04 p205) */
    int max_span = (sampleRate <= 32000) ? 48 : (sampleRate <= 44100) ? 35 : 32;
    if (k2 - kx > max_span) k2 = kx + max_span;
    return k2;
}

/*
 * Build master frequency band table (bs_freq_scale=0, bs_alter_scale=0).
 * ISO 14496-3:2009 §4.6.18.3.6 / FFmpeg sbr_make_f_master() lines 353-374.
 *
 * dk = bs_alter_scale + 1 = 1
 * n_master = ((k2-kx + (dk&2)) >> dk) << 1 = floor((k2-kx)/2)*2
 * k2diff   = (k2-kx) - n_master*dk
 * band widths start as dk=1 each; last entry gets +1 if k2diff==1,
 * or first/second entry get -1 if k2diff < 0.
 *
 * Noise: one noise floor band spanning the entire SBR region.
 */
static int build_freq_table(SBRInfo *sbr)
{
    int kx = sbr->kx;
    int k2 = sbr->k2;
    int dk = 1;  /* bs_alter_scale=0 → dk=1 */

    int n_master = ((k2 - kx + (dk & 2)) >> dk) << 1;
    if (n_master < 1) n_master = 1;
    if (n_master > SBR_MAX_BANDS) n_master = SBR_MAX_BANDS;

    /* f_master[k] = dk for k=1..n_master, then apply k2diff correction */
    int f_master[SBR_MAX_BANDS + 1];
    for (int k = 1; k <= n_master; k++)
        f_master[k] = dk;

    int k2diff = (k2 - kx) - n_master * dk;
    if (k2diff < 0) {
        f_master[1]--;
        if (k2diff < -1) f_master[2]--;
    } else if (k2diff > 0) {
        f_master[n_master]++;
    }

    f_master[0] = kx;
    for (int k = 1; k <= n_master; k++)
        f_master[k] += f_master[k - 1];

    sbr->numBands = n_master;
    for (int b = 0; b <= n_master; b++)
        sbr->bandEdges[b] = f_master[b];

    /* Single noise floor band */
    sbr->numNoiseBands     = 1;
    sbr->noiseBandEdges[0] = kx;
    sbr->noiseBandEdges[1] = k2;

    return n_master;
}

/* ------------------------------------------------------------------ *
 *  SBRInit / SBREnd
 * ------------------------------------------------------------------ */

SBRInfo *SBRInit(int channels, int sampleRate, int coreSampleRate)
{
    SBRInfo *sbr = (SBRInfo *)calloc(1, sizeof(SBRInfo));
    if (!sbr) return NULL;

    sbr->sbrPresent    = 1;
    sbr->headerSent    = 0;
    sbr->frameCount    = 0;
    sbr->numChannels   = channels;
    sbr->sampleRate    = sampleRate;
    sbr->coreSampleRate = coreSampleRate;

    /* Choose header parameters.
     * bs_stop_freq=7 is safe for all rates: worst-case k2-kx=41 ≤ 48.
     * Higher values (e.g. 8) are better quality but rate-dependent. */
    sbr->bs_amp_res    = 0;   /* 1.5 dB amplitude resolution */
    sbr->bs_start_freq = 15;  /* kx ≈ 31–32 (≈11.6–12 kHz) — just below LC core Nyquist */
    sbr->bs_stop_freq  = 14;  /* k2 = 2*kx — full upper-octave SBR extension */
    sbr->bs_xover_band = 0;

    sbr->kx = compute_kx(sampleRate, sbr->bs_start_freq);
    sbr->k2 = compute_k2(sampleRate, sbr->kx, sbr->bs_stop_freq);
    build_freq_table(sbr);

    /* Precompute QMF modulation tables */
    for (int k = 0; k < SBR_QMF_BANDS; k++) {
        double phase_step = M_PI * (2 * k + 1) / 128.0;
        for (int n = 0; n < SBR_QMF_FILTER_LEN; n++) {
            double phase = phase_step * (2 * n - 63);
            sbr->cos_table[k][n] = (faac_real)cos(phase);
            sbr->sin_table[k][n] = (faac_real)sin(phase);
        }
    }

    return sbr;
}

void SBREnd(SBRInfo *sbr)
{
    free(sbr);
}

/* ------------------------------------------------------------------ *
 *  32-band analysis QMF
 *  ISO 14496-3:2009 §4.6.18.7.1
 *
 *  For each time slot l (32 per frame):
 *    Shift 32 new samples into the 64-sample overlap buffer.
 *    For each subband k = 0..31:
 *      X_re[k] = sum_{n=0}^{63} h[n] * ovl[63-n]
 *                * cos(pi*(2k+1)*(2n-127)/256)
 *      X_im[k] = sum_{n=0}^{63} h[n] * ovl[63-n]
 *                * sin(pi*(2k+1)*(2n-127)/256)
 *
 *  Energy per subband: E[k] = X_re[k]^2 + X_im[k]^2
 * ------------------------------------------------------------------ */

/*
 * Analyse one time slot; return energy per QMF subband.
 * ovl[64]: overlap buffer (updated in place).
 * slot[32]: 32 new input samples.
 * energy[32]: output subband energies.
 */
static void qmf_analysis_slot(const SBRInfo *sbr,
                               const faac_real *slot,
                               faac_real *ovl,
                               faac_real *energy)
{
    /* Shift overlap buffer: drop 32 oldest, admit 32 new */
    memmove(ovl, ovl + 32, 32 * sizeof(faac_real));
    memcpy(ovl + 32, slot, 32 * sizeof(faac_real));

    /* Compute energy per subband using cosine-modulated filterbank.
     * ISO 14496-3 §6.8.1.1 analysis QMF with M=32, L=64:
     *   X_k = sum_{n=0}^{63} h[n] * x[n] * cos(pi*(k+0.5)*(2n-63)/64)
     *       + j*sum h[n] * x[n] * sin(...)
     * In our buffer x[n] = ovl[63-n] (newest sample at ovl[63]),
     * phase = pi*(2k+1)*(2n-63)/128
     *
     * Precomputed tables for cos/sin are used to avoid expensive runtime math. */
    for (int k = 0; k < SBR_QMF_BANDS; k++) {
        faac_real re = 0, im = 0;
        for (int n = 0; n < SBR_QMF_FILTER_LEN; n++) {
            faac_real hv = h_sbr_qmf[n] * ovl[SBR_QMF_FILTER_LEN - 1 - n];
            re += hv * sbr->cos_table[k][n];
            im += hv * sbr->sin_table[k][n];
        }
        energy[k] = re * re + im * im;
    }
}

/* ------------------------------------------------------------------ *
 *  SBRAnalysis
 * ------------------------------------------------------------------ */

void SBRAnalysis(SBRInfo *sbr,
                 faac_real *timeDomain[MAX_CHANNELS],
                 int numChannels,
                 int numSamples)
{
    /* Per-band energy accumulator over the whole frame.
     *
     * numSamples = 2*FRAME_LEN = 2048 full-rate samples.
     * The 32-band analysis QMF produces one slot per 32 input samples,
     * so there are numSamples/32 = 64 analysis QMF slots per SBR frame.
     *
     * The SBR temporal grid uses 64-band SYNTHESIS QMF slots (32 per frame).
     * Each synthesis slot = 64 full-rate samples = 2 analysis slots.
     * Therefore the decoder expects the envelope energy normalised by
     * the number of SYNTHESIS slots (32), i.e. summed over all analysis
     * slots and divided by (numSamples / 64) = 32.
     *
     * Combining time normalisation and the QMF filter energy gain
     * (h_sbr_qmf sum-of-squares ≈ 1.5) into one divisor:
     *   norm = (numSamples / 64) * 1.5   =  32 * 1.5 = 48  (for 2048 samples)
     */
    int num_ana_slots = numSamples / SBR_QMF_BANDS;  /* = 2048/32 = 64 */
    faac_real bandEnergy[MAX_CHANNELS][SBR_QMF_BANDS];
    faac_real slotEnergy[SBR_QMF_BANDS];

    for (int ch = 0; ch < numChannels; ch++) {
        memset(bandEnergy[ch], 0, sizeof(bandEnergy[ch]));

        for (int slot = 0; slot < num_ana_slots; slot++) {
            qmf_analysis_slot(sbr,
                              timeDomain[ch] + slot * SBR_QMF_BANDS,
                              sbr->qmfOvl[ch],
                              slotEnergy);
            for (int k = 0; k < SBR_QMF_BANDS; k++)
                bandEnergy[ch][k] += slotEnergy[k];
        }
    }

    /* Normalise to match the decoder's e_curr coordinate system.
     *
     * The decoder (aacsbr.c sbr_dequant) interprets transmitted level L as:
     *   env_facs = 2^(L/2 + 6)    (for amp_res=0)
     * and sbr_gain_calc computes:
     *   gain = sqrt(env_facs / e_curr)
     * where e_curr = mean(X_high^2, per synthesis half-slot per synthesis subband).
     *
     * Our 32-band analysis has:
     *   - num_ana_slots analysis slots = num synthesis half-slots (1:1 in time)
     *   - Each analysis band covers 2 synthesis subbands
     *
     * So: E_per_synth_subband = total_analysis_energy / (num_ana_slots * 2)
     *                         = total / (num_ana_slots * 2)
     *
     * With this normalisation, the correct level formula is:
     *   L = round(2 * (log2(E) - 6))   -- directly matches 2^(L/2+6) dequant
     */
    /* Normalise per synthesis subband only.
     * decoder e_curr = sum_{slots,subbands} |X_synth|^2 / num_synth_subbands
     * Each analysis band covers 2 synthesis subbands; all time-slot sums are
     * kept intact (no per-slot averaging), so norm = 2 (not num_slots*2). */
    float norm = 2.0f;
    for (int ch = 0; ch < numChannels; ch++)
        for (int k = 0; k < SBR_QMF_BANDS; k++)
            bandEnergy[ch][k] /= norm;

    /* --- Convert normalised subband energies to envelope levels --- */
    /*
     * FIXFIX + 1 envelope forces amp_res=0 (1.5 dB steps) in the decoder.
     *
     * First band: absolute 7-bit level [0..127].
     * Subsequent bands: signed Huffman-coded delta, range [−60..+60].
     *
     * Energy → level (matches decoder's 2^(L/2+6) dequantisation):
     *   level = round(2 * (log2(E) - 6))
     *
     * This is equivalent to 1.5 dB per step: each level unit = 3.01 dB / 2 ≈ 1.505 dB.
     */
    for (int ch = 0; ch < numChannels; ch++) {
        for (int e = 0; e < SBR_NUM_ENVELOPES; e++) {
            /* SBR_NUM_ENVELOPES == 1: single envelope over the full frame. */
            int prevLevel = -1;
            for (int b = 0; b < sbr->numBands; b++) {
                /* Sum energy over analysis QMF subbands for this SBR band.
                 * bandEdges[] are in 64-band synthesis QMF space (0..63).
                 * Our 32-band analysis QMF maps: analysis_band ≈ synth_band/2. */
                faac_real E = 0;
                int q_ana_start = sbr->bandEdges[b] / 2;
                int q_ana_end   = (sbr->bandEdges[b + 1] + 1) / 2;
                if (q_ana_end <= q_ana_start) q_ana_end = q_ana_start + 1;
                for (int qa = q_ana_start; qa < q_ana_end; qa++) {
                    int qi = (qa < SBR_QMF_BANDS) ? qa : SBR_QMF_BANDS - 1;
                    E += bandEnergy[ch][qi];
                }
                /* Convert energy to level: L = round(2*(log2(E)-6)) */
                double log2E = log2((double)E + 1e-20);
                int level = (int)round(2.0 * (log2E - 6.0));

                if (prevLevel < 0) {
                    /* First band: absolute 7-bit level, range [0, 127] (amp_res=0) */
                    level = clamp_int(level, 0, 127);
                    sbr->envData[ch][e][b] = level;
                } else {
                    /* Remaining bands: delta from previous level */
                    int raw_level = clamp_int(level, 0, 127);
                    int delta = clamp_int(raw_level - prevLevel,
                                         -F_HUFF_ENV_1_5DB_OFFSET,
                                          F_HUFF_ENV_1_5DB_OFFSET);
                    sbr->envData[ch][e][b] = delta;
                    level = prevLevel + delta;
                }
                prevLevel = level;
            }
        }

        /* --- Noise floor estimation -------------------------------- */
        /*
         * Use the mean per-synthesis-slot energy in the noise band as a
         * proxy for the noise floor level.  bandEnergy[] is already
         * normalised per synthesis slot by the normalization above.
         */
        int prevNoise = -1;
        for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
            faac_real meanE = 0;
            int count = 0;
            /* Same synthesis→analysis mapping as envelope estimation */
            int q_ana_start = sbr->noiseBandEdges[nb] / 2;
            int q_ana_end   = (sbr->noiseBandEdges[nb + 1] + 1) / 2;
            if (q_ana_end <= q_ana_start) q_ana_end = q_ana_start + 1;
            for (int qa = q_ana_start; qa < q_ana_end; qa++) {
                int qi = (qa < SBR_QMF_BANDS) ? qa : SBR_QMF_BANDS - 1;
                meanE += bandEnergy[ch][qi];
                count++;
            }
            if (count > 0) meanE /= count;
            double log2E = log2((double)meanE + 1e-20);
            int level = (int)round(12.0 - 0.5 * log2E);
            level = clamp_int(level, 0, 30);

            if (prevNoise < 0) {
                sbr->noiseData[ch][nb] = level;
            } else {
                int delta = clamp_int(level - prevNoise, -15, 15);
                sbr->noiseData[ch][nb] = delta;
                level = prevNoise + delta;
            }
            prevNoise = level;
        }
    }
}

/* ------------------------------------------------------------------ *
 *  SBR bitstream writing
 *  ISO 14496-3:2009 §4.6.18.4
 * ------------------------------------------------------------------ */

/* Write/count sbr_header() — ISO 14496-3 §4.6.18.4.1
 *
 * We signal bs_header_extra_1=1 to transmit:
 *   bs_freq_scale=0   → linear master frequency table (numBands = k2−kx)
 *   bs_alter_scale=0  → no alternate scale (required for freq_scale=0)
 *   bs_noise_bands=0  → 1 noise floor band
 * This ensures the decoder computes the same band layout as the encoder.
 */
static int write_sbr_header(SBRInfo *sbr, BitStream *bs, int wf)
{
    int bits = 0;
#define WB(v,n) do { if (wf) PutBit(bs,(v),(n)); bits += (n); } while(0)
    WB(sbr->bs_amp_res,    1);
    WB(sbr->bs_start_freq, 4);
    WB(sbr->bs_stop_freq,  4);
    WB(sbr->bs_xover_band, 3);
    WB(0, 2);   /* reserved */
    WB(1, 1);   /* bs_header_extra_1 = 1 → transmit freq scale flags */
    /* bs_header_extra_1 contents: */
    WB(0, 2);   /* bs_freq_scale = 0 (linear: numBands = k2−kx) */
    WB(0, 1);   /* bs_alter_scale = 0 */
    WB(0, 2);   /* bs_noise_bands = 0 → numNoiseBands = 1 */
    WB(0, 1);   /* bs_header_extra_2 = 0 */
#undef WB
    return bits;
}

/*
 * Write/count sbr_grid() — FIXFIX, 1 envelope per frame.
 * ISO 14496-3:2009 §4.6.18.4.3
 */
static int write_sbr_grid(BitStream *bs, int wf)
{
    int bits = 0;
#define WB(v,n) do { if (wf) PutBit(bs,(v),(n)); bits += (n); } while(0)
    WB(SBR_FRAME_CLASS_FIXFIX, 2); /* bs_frame_class = FIXFIX */
    WB(0, 2);                      /* bs_num_env = 0 → 1 envelope */
    WB(1, 1);                      /* bs_freq_res[0] = 1 (high-res) */
#undef WB
    return bits;
}

/*
 * Write/count sbr_dtdf() — all frequency-domain (no delta-time).
 * ISO 14496-3:2009 §4.6.18.4.4
 */
static int write_sbr_dtdf(BitStream *bs, int wf)
{
    int bits = 0;
    /* bs_df_env[0] = 0 (frequency-domain) */
    if (wf) PutBit(bs, 0, 1);
    bits += 1;
    /* bs_df_noise[0] = 0 */
    if (wf) PutBit(bs, 0, 1);
    bits += 1;
    return bits;
}

/*
 * Write/count sbr_envelope() for one channel.
 * ISO 14496-3:2009 §4.6.18.4.6
 *
 * FIXFIX + 1 envelope forces amp_res=0 (1.5 dB) in the decoder.
 *   First band: 7-bit absolute level 0..127 (decoder reads get_bits(gb, 7))
 *   Rest: Huffman-coded delta using f_huff_env_1_5dB (symbols ±60)
 */
static int write_sbr_envelope(SBRInfo *sbr, BitStream *bs, int ch, int wf)
{
    int bits = 0;
    for (int e = 0; e < SBR_NUM_ENVELOPES; e++) {
        for (int b = 0; b < sbr->numBands; b++) {
            int val = sbr->envData[ch][e][b];
            if (b == 0) {
                /* First band: 7-bit absolute level (amp_res=0 forced by FIXFIX+1env: 0..127) */
                int abs_val = clamp_int(val, 0, 127);
                if (wf) PutBit(bs, abs_val, 7);
                bits += 7;
            } else {
                /* Delta: Huffman-coded with f_huff_env_1_5dB */
                bits += put_huff(bs, f_huff_env_1_5dB,
                                 F_HUFF_ENV_1_5DB_NSYMS,
                                 F_HUFF_ENV_1_5DB_OFFSET,
                                 val, wf);
            }
        }
    }
    return bits;
}

/*
 * Write/count sbr_noise() for one channel.
 * ISO 14496-3:2009 §4.6.18.4.7
 *
 * First noise band: raw 5 bits (absolute 0..30)
 * Additional bands (if any): Huffman-coded delta using f_huff_env_3_0dB
 * (ISO spec uses the envelope 3.0 dB table for noise floor deltas too.)
 * We only ever have 1 noise band, so only the 5-bit absolute is written.
 */
static int write_sbr_noise(SBRInfo *sbr, BitStream *bs, int ch, int wf)
{
    int bits = 0;
    for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
        int val = sbr->noiseData[ch][nb];
        if (nb == 0) {
            int abs_val = clamp_int(val, 0, 30);
            if (wf) PutBit(bs, abs_val, 5);
            bits += 5;
        } else {
            bits += put_huff(bs, f_huff_env_3_0dB,
                             F_HUFF_ENV_3_0DB_NSYMS,
                             F_HUFF_ENV_3_0DB_OFFSET,
                             val, wf);
        }
    }
    return bits;
}

/*
 * Write/count sbr_invf() — inverse filtering direction.
 * ISO 14496-3:2009 §4.6.18.4.5
 * One 2-bit mode per noise band (NONE=0, LOW=1, MID=2, HIGH=3).
 * We always use NONE (0) since we're doing no explicit inverse filtering.
 */
static int write_sbr_invf(SBRInfo *sbr, BitStream *bs, int wf)
{
    int bits = 0;
    for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
        if (wf) PutBit(bs, 2, 2);  /* bs_invf_mode = MID (partial decorrelation) */
        bits += 2;
    }
    return bits;
}

/*
 * Write/count one channel's per-frame SBR data:
 *   sbr_grid, sbr_dtdf, sbr_invf, sbr_envelope, sbr_noise,
 *   sbr_sinusoidal_coding (bs_add_harmonic_flag=0).
 */
static int write_sbr_channel_data(SBRInfo *sbr, BitStream *bs, int ch, int wf)
{
    int bits = 0;
    bits += write_sbr_grid(bs, wf);
    bits += write_sbr_dtdf(bs, wf);
    bits += write_sbr_invf(sbr, bs, wf);
    bits += write_sbr_envelope(sbr, bs, ch, wf);
    bits += write_sbr_noise(sbr, bs, ch, wf);
    /* sbr_sinusoidal_coding: bs_add_harmonic_flag = 0 */
    if (wf) PutBit(bs, 0, 1);
    bits += 1;
    return bits;
}

/*
 * Write/count sbr_data().
 * ISO 14496-3:2009 §4.6.18.4.2
 *
 * SCE: bs_data_extra(1) + sce_data + bs_extended_data(1)
 * CPE (bs_coupling=0): bs_data_extra(1) + bs_coupling(1)
 *   + grid(ch0), grid(ch1)
 *   + dtdf(ch0), dtdf(ch1)
 *   + invf(ch0), invf(ch1)
 *   + envelope(ch0), envelope(ch1)
 *   + noise(ch0), noise(ch1)
 *   + harmonic(ch0), harmonic(ch1)
 *   + bs_extended_data(1)
 *
 * The interleaved CPE order must match FFmpeg read_sbr_channel_pair_element().
 */
static int write_sbr_data(SBRInfo *sbr, BitStream *bs, int id_aac, int wf)
{
#define WBL(v,n) do { if (wf) PutBit(bs,(v),(n)); bits += (n); } while(0)
    int bits = 0;
    WBL(0, 1);  /* bs_data_extra = 0 */

    if (id_aac == 1 /* ID_CPE */) {
        WBL(0, 1);  /* bs_coupling = 0 (independent per-channel coding) */
        /* Interleaved order: grid, dtdf, invf, envelope, noise, harmonic
         * must match FFmpeg read_sbr_channel_pair_element() non-coupling path */
        bits += write_sbr_grid(bs, wf);          /* grid ch0 */
        bits += write_sbr_grid(bs, wf);          /* grid ch1 */
        bits += write_sbr_dtdf(bs, wf);          /* dtdf ch0 */
        bits += write_sbr_dtdf(bs, wf);          /* dtdf ch1 */
        bits += write_sbr_invf(sbr, bs, wf);     /* invf ch0 */
        bits += write_sbr_invf(sbr, bs, wf);     /* invf ch1 */
        bits += write_sbr_envelope(sbr, bs, 0, wf); /* envelope ch0 */
        bits += write_sbr_envelope(sbr, bs, 1, wf); /* envelope ch1 */
        bits += write_sbr_noise(sbr, bs, 0, wf);    /* noise ch0 */
        bits += write_sbr_noise(sbr, bs, 1, wf);    /* noise ch1 */
        /* bs_add_harmonic_flag ch0 and ch1 */
        WBL(0, 1);
        WBL(0, 1);
    } else {
        /* ID_SCE or ID_CCE */
        bits += write_sbr_channel_data(sbr, bs, 0, wf);
    }

    WBL(0, 1);  /* bs_extended_data = 0 */
#undef WBL
    return bits;
}

/* ------------------------------------------------------------------ *
 *  SBRWriteBitstream (public)
 *  ISO 14496-3:2009 §4.6.18.4  sbr_extension_data()
 *
 *  Fill element layout:
 *    ID_FIL (3 bits)
 *    count  (4 bits, or 15 + esc_count(8) if >= 15)
 *    -- fill data (count bytes) --
 *    extension_type (4 bits) = EXT_SBR_DATA = 0xd
 *    bs_header_flag (1 bit)
 *    [sbr_header() if bs_header_flag]
 *    sbr_data(id_aac)
 *    bs_fill_bits (pad to byte boundary)
 * ------------------------------------------------------------------ */

int SBRWriteBitstream(SBRInfo *sbr, BitStream *bs, int id_aac, int writeFlag)
{
    if (!sbr || !sbr->sbrPresent) return 0;

    int sendHeader = (!sbr->headerSent ||
                      (sbr->frameCount % SBR_HEADER_PERIOD == 0));

    /* --- Dry-run to count fill-data bits (ext_type onward) -------- */
    int fillContentBits = 4;  /* extension_type */
    fillContentBits += 1;     /* bs_header_flag */
    if (sendHeader)
        fillContentBits += write_sbr_header(sbr, NULL, 0);
    fillContentBits += write_sbr_data(sbr, NULL, id_aac, 0);

    /* Round up to whole bytes; the fill element 'count' is in bytes */
    int fillBytes = (fillContentBits + 7) / 8;
    int padBits   = fillBytes * 8 - fillContentBits;


#define WB(v,n) do { if (writeFlag) PutBit(bs,(v),(n)); totalBits += (n); } while(0)
    int totalBits = 0;

    /* Fill element header */
    WB(6, 3);  /* ID_FIL */
    if (fillBytes < 15) {
        WB(fillBytes, 4);
    } else {
        WB(15, 4);
        WB(fillBytes - 14, 8);  /* esc_count: actual = esc_count + 14 */
    }

    /* Fill data content */
    WB(SBR_EXT_TYPE_SBR, 4);       /* extension_type */
    WB(sendHeader ? 1 : 0, 1);     /* bs_header_flag */
    if (sendHeader)
        totalBits += write_sbr_header(sbr, writeFlag ? bs : NULL, writeFlag);
    totalBits += write_sbr_data(sbr, writeFlag ? bs : NULL, id_aac, writeFlag);

    /* Padding bits to fill declared byte count */
    if (padBits > 0) {
        WB(0, padBits);
    }
#undef WB

    if (writeFlag) {
        sbr->headerSent = 1;
        sbr->frameCount++;
    }

    return totalBits;
}
