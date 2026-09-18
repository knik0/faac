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

#include "channels.h"
#include "huff2.h"
#include "frame.h"
#include "sbr.h"
#include <string.h>
#include <stdio.h>

_Static_assert(TNS_MAX_FILTERS <= (1 << LEN_TNS_NFILTL) - 1,
               "TnsWindowData.tnsFilter[] holds more filters than numFilters can encode in LEN_TNS_NFILTL bits");
_Static_assert(TNS_MAX_ORDER <= (1 << LEN_TNS_ORDERL) - 1,
               "TnsFilterData order exceeds what LEN_TNS_ORDERL bits can encode");

/**
 * Maps input channels to AAC elements (SCE, CPE, LFE), per ISO/IEC 14496-3's
 * channel configuration table: one SCE up front unless exactly 2 channels
 * remain, then as many CPEs as fit two at a time, then a trailing odd
 * channel becomes an LFE (if enabled) or a final SCE.
 */
int InitElements(AACElement * __restrict elements, int *numElements, int numChannels, bool useLfe)
{
    uint8_t sceTag = 0;
    uint8_t cpeTag = 0;
    uint8_t lfeTag = 0;

    int currentElem = 0;
    int currentCh = 0;
    int channelsRemaining = numChannels;

    memset(elements, 0, sizeof(AACElement) * MAX_CHANNELS);

    // Initial SCE for Config 1, 3, 4, 5, 6, 7
    if (channelsRemaining != 2 && channelsRemaining > 0) {
        elements[currentElem].type = ID_SCE;
        elements[currentElem].tag = sceTag++;
        elements[currentElem].channels[0] = currentCh++;
        elements[currentElem].channels[1] = -1;

        currentElem++;
        channelsRemaining--;
    }

    // CPE groups
    while (channelsRemaining > 1) {
        elements[currentElem].type = ID_CPE;
        elements[currentElem].tag = cpeTag++;
        elements[currentElem].channels[0] = currentCh++;
        elements[currentElem].channels[1] = currentCh++;

        currentElem++;
        channelsRemaining -= 2;
    }

    // Residual SCE or LFE
    if (channelsRemaining == 1) {
        if (useLfe) {
            elements[currentElem].type = ID_LFE;
            elements[currentElem].tag = lfeTag++;
        } else {
            elements[currentElem].type = ID_SCE;
            elements[currentElem].tag = sceTag++;
        }
        elements[currentElem].channels[0] = currentCh++;
        elements[currentElem].channels[1] = -1;

        currentElem++;
    }

    *numElements = currentElem;
    return 0;
}

static int WriteICSInfo(BitStream *bs, CoderInfo *coder)
{
    PutBit(bs, 0, LEN_ICS_RESERV);
    PutBit(bs, coder->block_type, LEN_WIN_SEQ);
    PutBit(bs, 0, LEN_WIN_SH); /* window_shape: sine */
    int bits = LEN_ICS_RESERV + LEN_WIN_SEQ + LEN_WIN_SH;

    if (coder->block_type == ONLY_SHORT_WINDOW) {
        PutBit(bs, coder->sfbn, LEN_MAX_SFBS);

        int grouping_bits = 0;
        /* Construct the 7 scale factor window grouping bits directly from group lengths.
         * A bit is 1 if adjacent sub-windows belong to the same group, 0 if a new group starts. */
        for (int i = 0; i < coder->groups.n; i++) {
            int len = coder->groups.len[i];
            if (len > 1) {
                grouping_bits = (grouping_bits << (len - 1)) | ((1 << (len - 1)) - 1);
            }
            if (i < coder->groups.n - 1) {
                grouping_bits <<= 1;
            }
        }
        PutBit(bs, grouping_bits, MAX_SHORT_WINDOWS - 1);
        bits += LEN_MAX_SFBS + (MAX_SHORT_WINDOWS - 1);
    } else {
        PutBit(bs, coder->sfbn, LEN_MAX_SFBL);
        PutBit(bs, 0, LEN_PRED_PRES);
        bits += LEN_MAX_SFBL + LEN_PRED_PRES;
    }

    return bits;
}

static int WriteICS(BitStream *bs, CoderInfo *coder, bool commonWindow)
{
    PutBit(bs, coder->global_gain, LEN_GLOB_GAIN);
    int bits = LEN_GLOB_GAIN;

    if (!commonWindow) bits += WriteICSInfo(bs, coder);

    bits += writebooks(coder, bs);
    bits += writesf(coder, bs);

    PutBit(bs, 0, LEN_PULSE_PRES);
    bits += LEN_PULSE_PRES;

    TnsInfo *tns = &coder->tnsInfo;
    PutBit(bs, tns->tnsDataPresent, LEN_TNS_PRES);
    bits += LEN_TNS_PRES;

    /* TNS is long-only (see tns.c): tnsDataPresent is never set for
     * ONLY_SHORT_WINDOW, so there's exactly one window's worth of TNS data
     * to write, always at the long-window field widths. */
    if (tns->tnsDataPresent) {
        TnsWindowData *win = &tns->windowData;

        PutBit(bs, win->numFilters, LEN_TNS_NFILTL);
        bits += LEN_TNS_NFILTL;

        if (win->numFilters > 0) {
            PutBit(bs, win->coefResolution - DEF_TNS_RES_OFFSET, LEN_TNS_COEFF_RES);
            bits += LEN_TNS_COEFF_RES;

            for (int f = 0; f < win->numFilters; f++) {
                TnsFilterData *flt = &win->tnsFilter[f];
                PutBit(bs, flt->length, LEN_TNS_LENGTHL);
                PutBit(bs, flt->order, LEN_TNS_ORDERL);
                bits += LEN_TNS_LENGTHL + LEN_TNS_ORDERL;

                if (flt->order > 0) {
                    PutBit(bs, flt->direction, LEN_TNS_DIRECTION);
                    PutBit(bs, flt->coefCompress, LEN_TNS_COMPRESS);
                    bits += LEN_TNS_DIRECTION + LEN_TNS_COMPRESS;

                    int res = win->coefResolution - flt->coefCompress;
                    for (int i = 1; i <= flt->order; i++) {
                        PutBit(bs, flt->index[i] & ((1 << res) - 1), res);
                        bits += res;
                    }
                }
            }
        }
    }

    PutBit(bs, 0, LEN_GAIN_PRES);
    bits += LEN_GAIN_PRES;

    BitAccumulator acc = {0};
    AccumBegin(&acc, bs);
    for (int i = 0; i < coder->datacnt; i++) {
        if (coder->s[i].len > 0) {
            AccumPutBits(&acc, (uint32_t)coder->s[i].data, coder->s[i].len);
            bits += coder->s[i].len;
        }
    }
    AccumEnd(&acc);

    return bits;
}

int WriteElement(BitStream *bs, AACElement *elem, CoderInfo *coder)
{
    PutBit(bs, elem->type, LEN_SE_ID);
    PutBit(bs, elem->tag, LEN_TAG);
    int bits = LEN_SE_ID + LEN_TAG;

    switch (elem->type) {
        case ID_SCE:
        case ID_LFE:
            bits += WriteICS(bs, &coder[elem->channels[0]], false);
            break;

        case ID_CPE:
            PutBit(bs, elem->common_window, LEN_COM_WIN);
            bits += LEN_COM_WIN;

            if (elem->common_window) {
                bits += WriteICSInfo(bs, &coder[elem->channels[0]]);
                PutBit(bs, elem->msInfo.is_present, LEN_MASK_PRES);
                if (elem->msInfo.is_present == 1) {
                    int n = coder[elem->channels[0]].groups.n * coder[elem->channels[0]].sfbn;
                    for (int i = 0; i < n; i++) PutBit(bs, elem->msInfo.ms_used[i], LEN_MASK);
                }
                bits += LEN_MASK_PRES;
                if (elem->msInfo.is_present == 1)
                    bits += coder[elem->channels[0]].groups.n * coder[elem->channels[0]].sfbn * LEN_MASK;
            }
            bits += WriteICS(bs, &coder[elem->channels[0]], elem->common_window);
            bits += WriteICS(bs, &coder[elem->channels[1]], elem->common_window);
            break;
        default: break;
    }
    return bits;
}

static int WriteADTSHeader(struct faacEncStruct *hEncoder, BitStream *bs)
{
    int channelConfig = GetChannelConfig((int)hEncoder->numChannels);
    PutBit(bs, 0xFFF,                        LEN_ADTS_SYNC);
    PutBit(bs, hEncoder->config.mpegVersion, LEN_ADTS_ID);
    PutBit(bs, 0,                            LEN_ADTS_LAYER);
    PutBit(bs, 1,                            LEN_ADTS_ABSENT);
    /* profile: always LC. HE-AAC's core is LC too; SBR is implicit via fill element. */
    PutBit(bs, LOW - 1,                      LEN_ADTS_PROFILE);
    PutBit(bs, hEncoder->sampleRateIdx,       LEN_ADTS_FREQ);
    PutBit(bs, 0,                            LEN_ADTS_PRIV);
    PutBit(bs, channelConfig,                LEN_ADTS_CH_CFG);
    PutBit(bs, 0,                            LEN_ADTS_ORIG + LEN_ADTS_HOME);
    PutBit(bs, 0,                            LEN_ADTS_COPY_ID + LEN_ADTS_COPY_ST);
    PutBit(bs, hEncoder->usedBytes,          LEN_ADTS_FRAME);
    PutBit(bs, 0x7FF,                        LEN_ADTS_FULL);
    PutBit(bs, 0,                            LEN_ADTS_BLOCKS);
    return 56;
}

static int WriteAACFillBits(BitStream *bs, int numBits)
{
    int left = numBits;
    while (left >= (LEN_SE_ID + 4)) {
        PutBit(bs, ID_FIL, LEN_SE_ID);
        left -= LEN_SE_ID;
        int bc = (left / 8 < 15) ? (left / 8) : 15;
        PutBit(bs, bc, 4);
        left -= 4;
        if (bc == 15) {
            int esc = (left / 8 - 14 < 255) ? (left / 8 - 14) : 255;
            PutBit(bs, esc, 8);
            left -= 8;
            bc = 14 + esc;
        }
        for (int i = 0; i < bc; i++) PutBit(bs, 0, 8);
        left -= bc * 8;
    }
    return left;
}

static int BuildFrame(struct faacEncStruct *hEncoder, CoderInfo *coder, AACElement *elems, int nElems, BitStream *bs)
{
    int bits = 0;
    if (hEncoder->config.outputFormat == 1) bits += WriteADTSHeader(hEncoder, bs);
    /* SBR follows each SCE/CPE in a fill element; rate control charges only
     * the core, so its total is kept aside. */
    int sbrBits = 0;
    for (int i = 0; i < nElems; i++) {
        bits += WriteElement(bs, &elems[i], coder);
        sbrBits += SbrContextGetBits(hEncoder->sbrContext, bs,
                                     &elems[i], (int)hEncoder->config.aacObjectType);
    }
    hEncoder->rc.sbrBits = sbrBits;
    bits += sbrBits;
    int f = (bits < (8 - LEN_SE_ID)) ? (8 - LEN_SE_ID - bits) : 0;
    f += 6;

    /* Stuff to the reservoir floor; only the end marker still follows. */
    hEncoder->rc.stuffedBits = 0;
    if (hEncoder->rc.resMinBits > 0) {
        int hdr = (hEncoder->config.outputFormat == 1) ? ADTS_HEADER_SIZE * 8 : 0;
        int need = hEncoder->rc.resMinBits - (bits - hdr + f + LEN_SE_ID);
        if (need > 0) {
            /* Fill is byte-granular and rounds down; never land under the floor. */
            need += 7;
            f += need;
            hEncoder->rc.stuffedBits = need;
        }
    }
    bits += (f - WriteAACFillBits(bs, f));

    PutBit(bs, ID_END, LEN_SE_ID);
    bits += LEN_SE_ID;
    int pad = (8 - (bits & 7)) & 7;
    for (int i = 0; i < pad; i++) PutBit(bs, 0, 1);
    return bits + pad;
}

/* Frame length and buffer_fullness are known only once the frame is written;
 * patching the 7 fixed-layout bytes keeps BuildFrame to one pass. Must match
 * WriteADTSHeader apart from those two fields. buffer_fullness is the reservoir
 * after this frame in 32-bit words (ISO/IEC 13818-7 6.2.2); 0x7FF = none. */
static void PatchADTSHeader(struct faacEncStruct *hEncoder, BitStream *bs, int frameBytes)
{
    if (hEncoder->config.outputFormat == 1 && bs->data) {
        int fullness = 0x7FF;
        int channelConfig = GetChannelConfig((int)hEncoder->numChannels);
        if (hEncoder->rc.resMean)
            fullness = RateControlReservoirAfter(&hEncoder->rc, (frameBytes - ADTS_HEADER_SIZE) * 8) >> 5;
        bs->data[0] = 0xFF;
        bs->data[1] = 0xF0 | (hEncoder->config.mpegVersion << 3) | 1;
        bs->data[2] = ((LOW - 1) << 6) | (hEncoder->sampleRateIdx << 2) | (channelConfig >> 2);
        bs->data[3] = ((channelConfig & 3) << 6) | (frameBytes >> 11);
        bs->data[4] = (frameBytes >> 3) & 0xFF;
        bs->data[5] = ((frameBytes & 7) << 5) | (fullness >> 6);
        bs->data[6] = (fullness & 0x3F) << 2;
    }
}

int WriteBitstream(struct faacEncStruct *hEncoder, CoderInfo *coder, AACElement *elems, int nElems, BitStream *bs)
{
    /* Zero so the header's own length field is written as zero, then patched. */
    hEncoder->usedBytes = 0;
    bs->currentBit = 0;
    int bits = BuildFrame(hEncoder, coder, elems, nElems, bs);
    if (bits < 0) return -1;

    /* Safe to bounds-check after writing: PutBit refuses to write past
     * bs->size, so an oversized frame truncates rather than overflowing. */
    hEncoder->usedBytes = (bits + 7) >> 3;
    if (hEncoder->usedBytes > bs->size) return -1;
    if (hEncoder->usedBytes > ADTS_MAX_FRAME_SIZE) return -1;

    PatchADTSHeader(hEncoder, bs, hEncoder->usedBytes);
    return bits;
}
