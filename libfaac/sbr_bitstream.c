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

#include <string.h>
#include <assert.h>

#include "sbr.h"
#include "sbr_internal.h"
#include "sbr_tables.h"
#include "bitstream.h"
#include "util.h"
#include "faac_internal.h"

static void put_huff(BitStream *bs, const SBRHuffEntry *table, int nsyms, int offset, int delta)
{
    int sym = clamp_int(delta + offset, 0, nsyms - 1);
    PutBit(bs, table[sym].code, table[sym].len);
}

static void write_sbr_header(SBRInfo *sbr, BitStream *bs)
{
    /* ISO 14496-3:2009 §4.6.18.5 sbr_header() (21 bits) */
    PutBit(bs, sbr->bs_amp_res,     1); /* bs_amp_res: 0=1.5dB, 1=3dB */
    PutBit(bs, sbr->bs_start_freq,  4); /* bs_start_freq: crossover index */
    PutBit(bs, sbr->bs_stop_freq,   4); /* bs_stop_freq: high-band ceil */
    PutBit(bs, sbr->bs_xover_band,  3); /* bs_xover_band: low-res split (0=none) */
    PutBit(bs, 0,                   2); /* bs_reserved */
    PutBit(bs, 1,                   1); /* bs_header_extra_1 = 1 (alter_scale present) */
    PutBit(bs, 0,                   1); /* bs_header_extra_2 = 0 (limiter fields absent) */
    /* bs_header_extra_1 fields: */
    PutBit(bs, 0,                   2); /* bs_freq_scale = 0 (linear master table) */
    PutBit(bs, sbr->bs_alter_scale, 1); /* bs_alter_scale: 1=coarser at low bitrate */
    PutBit(bs, 0,                   2); /* bs_noise_bands = 0 (→ 1 noise band) */
}

/* Width of the transient pointer field, indexed by number of envelopes. */
static const int sbr_ceil_log2[] = { 0, 1, 2, 2, 3, 3 };

static void write_sbr_grid(SBRInfo *sbr, BitStream *bs)
{
    if (sbr->frameClass == SBR_FRAME_CLASS_VARFIX) {
        /* VARFIX: variable leading borders, fixed trailing border. Mirrors the
         * inverse of FFmpeg read_sbr_grid()'s VARFIX case: t_env[0]=bs_var_bord_0,
         * each lead border adds 2*bs_rel+2, the trailing border is numTimeSlots
         * (not transmitted), then bs_pointer and per-envelope bs_freq_res. */
        int num_env = sbr->numEnvelopes;
        PutBit(bs, SBR_FRAME_CLASS_VARFIX, 2);
        PutBit(bs, sbr->tEnv[0], 2);                 /* bs_var_bord_0 */
        PutBit(bs, num_env - 1, 2);                  /* bs_num_rel_0   */
        for (int i = 0; i < num_env - 1; i++)
            PutBit(bs, (sbr->tEnv[i + 1] - sbr->tEnv[i] - 2) / 2, 2); /* bs_rel_bord */
        PutBit(bs, sbr->bsPointer, sbr_ceil_log2[num_env]);
        for (int i = 0; i < num_env; i++)            /* bs_freq_res[1..num_env] */
            PutBit(bs, sbr->bs_freq_res, 1);
    } else {
        /* FIXFIX: equal-spaced borders, one bs_freq_res for all envelopes. */
        PutBit(bs, SBR_FRAME_CLASS_FIXFIX, 2);
        PutBit(bs, sbr->numEnvelopes > 1 ? 1 : 0, 2);
        PutBit(bs, sbr->bs_freq_res, 1);
    }
}

static void write_sbr_dtdf(SBRInfo *sbr, BitStream *bs)
{
    int n_q = sbr->numEnvelopes > 1 ? 2 : 1;
    int bits = sbr->numEnvelopes + n_q;
    for (int i = 0; i < bits; i++) PutBit(bs, 0, 1);
}

static void write_sbr_invf(SBRInfo *sbr, BitStream *bs, int ch)
{
    for (int nb = 0; nb < sbr->numNoiseBands; nb++)
        PutBit(bs, sbr->ch[ch].invfMode, 2);
}

static void write_sbr_envelope(SBRInfo *sbr, BitStream *bs, int ch)
{
    const SBRHuffEntry *table = sbr->eff_amp_res ? f_huff_env_3_0dB : f_huff_env_1_5dB;
    int nsyms = sbr->eff_amp_res ? F_HUFF_ENV_3_0DB_NSYMS : F_HUFF_ENV_1_5DB_NSYMS;
    int offset = sbr->eff_amp_res ? F_HUFF_ENV_3_0DB_OFFSET : F_HUFF_ENV_1_5DB_OFFSET;

    for (int e = 0; e < sbr->numEnvelopes; e++) {
        for (int b = 0; b < sbr->numBands; b++) {
            int val = sbr->ch[ch].envData[e][b];
            if (b == 0) {
                int first_bits = sbr->eff_amp_res ? 6 : 7;
                PutBit(bs, clamp_int(val, 0, (1 << first_bits) - 1), first_bits);
            } else {
                put_huff(bs, table, nsyms, offset, val);
            }
        }
    }
}

static void write_sbr_noise(SBRInfo *sbr, BitStream *bs, int ch)
{
    int n_q = sbr->numEnvelopes > 1 ? 2 : 1;
    for (int ne = 0; ne < n_q; ne++) {
        for (int nb = 0; nb < sbr->numNoiseBands; nb++) {
            int val = sbr->ch[ch].noiseData[ne][nb];
            if (nb == 0) PutBit(bs, clamp_int(val, 0, 30), 5);
            else put_huff(bs, f_huff_env_3_0dB, F_HUFF_ENV_3_0DB_NSYMS, F_HUFF_ENV_3_0DB_OFFSET, val);
        }
    }
}

static void write_sbr_data(SBRInfo *sbr, BitStream *bs, int id_aac)
{
    if (id_aac == ID_CPE) {
        PutBit(bs, 0, 1); PutBit(bs, 0, 1);     /* bs_coupling=0, reserved */
        write_sbr_grid(sbr, bs);
        write_sbr_grid(sbr, bs);
        write_sbr_dtdf(sbr, bs);
        write_sbr_dtdf(sbr, bs);
        write_sbr_invf(sbr, bs, 0);
        write_sbr_invf(sbr, bs, 1);
        write_sbr_envelope(sbr, bs, 0);
        write_sbr_envelope(sbr, bs, 1);
        write_sbr_noise(sbr, bs, 0);
        write_sbr_noise(sbr, bs, 1);
        PutBit(bs, 0, 1); PutBit(bs, 0, 1); PutBit(bs, 0, 1); /* add_harmonic / extended data flags */
    } else {
        PutBit(bs, 0, 1);                        /* reserved */
        write_sbr_grid(sbr, bs);
        write_sbr_dtdf(sbr, bs);
        write_sbr_invf(sbr, bs, 0);
        write_sbr_envelope(sbr, bs, 0);
        write_sbr_noise(sbr, bs, 0);
        PutBit(bs, 0, 1); PutBit(bs, 0, 1);      /* add_harmonic / extended data flags */
    }
}

/* Emit the full extension_payload body for EXT_SBR_DATA: the 4-bit extension
 * type, the 1-bit header flag, the optional header, and the channel data. */
static void emit_sbr_payload(SBRInfo *sbr, BitStream *bs, int id_aac, int sendHeader)
{
    PutBit(bs, SBR_EXT_TYPE_SBR, 4);
    PutBit(bs, sendHeader, 1);
    if (sendHeader) write_sbr_header(sbr, bs);
    write_sbr_data(sbr, bs, id_aac);
}

/* Replay a byte-aligned, MSB-first payload (as packed into payloadBuf by PutBit)
 * into the real bitstream. This is the "dumb write" — no grid/Huffman logic, so it
 * reproduces the cached emit bit-for-bit. */
static void blit_payload(BitStream *bs, const unsigned char *buf, int nbits)
{
    int fullBytes = nbits >> 3, rem = nbits & 7;
    for (int i = 0; i < fullBytes; i++) PutBit(bs, buf[i], 8);
    if (rem) PutBit(bs, buf[fullBytes] >> (8 - rem), rem);
}

int SbrWrite(SBRInfo *sbr, BitStream *bs, int id_aac, int writeFlag)
{
    if (!sbr || !sbr->sbrPresent) return 0;

    /* Cache SBR payload to avoid redundant Huffman encoding during rate control.
     * The payload remains invariant once the subband analysis is complete. */
    if (!sbr->payloadValid) {
        sbr->cachedSendHeader = (!sbr->headerSent || (sbr->frameCount % SBR_HEADER_PERIOD == 0));
        BitStream cb;
        memset(&cb, 0, sizeof(cb));
        cb.data = sbr->payloadBuf;
        cb.size = sizeof(sbr->payloadBuf);
        cb.maxBit = (long)sizeof(sbr->payloadBuf) * 8;
        emit_sbr_payload(sbr, &cb, id_aac, sbr->cachedSendHeader);
        sbr->payloadBits = (int)cb.numBit;          /* ext_type + flag + header + data */
        sbr->payloadValid = 1;
    }

    int payloadBits = sbr->payloadBits;
    int fillBytes = (payloadBits + 7) / 8;
    int padBits = fillBytes * 8 - payloadBits;

    /* The fill_element count escapes through an 8-bit field, so a single
     * extension_payload tops out at 15 + 255 - 1 = 269 bytes. A larger SBR
     * payload would silently truncate esc_count and corrupt the boundary. */
    assert(fillBytes <= 14 + 255);

    int totalBits = 0;
#define WB(v,n) do { if (writeFlag) PutBit(bs,(v),(n)); totalBits += (n); } while(0)
    /* fill_element(): id, then 4-bit count with optional 8-bit escape. The
     * decoder reconstructs cnt = 15 + esc_count - 1, hence esc_count = N - 14. */
    WB(ID_FIL, 3);
    if (fillBytes < 15) WB(fillBytes, 4);
    else { WB(15, 4); WB(fillBytes - 14, 8); }

    /* Blit the cached payload for real (counting-only callers just account for it). */
    if (writeFlag) blit_payload(bs, sbr->payloadBuf, payloadBits);
    totalBits += payloadBits;
    if (padBits > 0) WB(0, padBits);
#undef WB

    if (writeFlag) { sbr->headerSent = 1; sbr->frameCount++; }
    return totalBits;
}

int SbrContextGetBits(SBRContext *sCtx, BitStream *bs, int channels, int aacObjectType, int writeFlag)
{
    if (aacObjectType == HE_V1 && sCtx) {
        if (sCtx->sbrInfo) {
            int id_aac = (channels > 1) ? ID_CPE : ID_SCE;
            return SbrWrite(sCtx->sbrInfo, bs, id_aac, writeFlag);
        }
    }
    return 0;
}
