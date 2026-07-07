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
#include <string.h>
#include <stdio.h>

_Static_assert(TNS_MAX_FILTERS == (1 << LEN_TNS_NFILTL),
               "coder.h's TnsWindowData.tnsFilter[] bound must match the LEN_TNS_NFILTL bitstream field width");

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

static int WriteICSInfo(BitStream *bs, CoderInfo *coder, bool writeFlag)
{
    if (writeFlag) {
        PutBit(bs, 0, LEN_ICS_RESERV);
        PutBit(bs, coder->block_type, LEN_WIN_SEQ);
        PutBit(bs, coder->window_shape, LEN_WIN_SH);
    }
    int bits = LEN_ICS_RESERV + LEN_WIN_SEQ + LEN_WIN_SH;

    if (coder->block_type == ONLY_SHORT_WINDOW) {
        if (writeFlag) {
            PutBit(bs, coder->sfbn, LEN_MAX_SFBS);

            int grouping_bits = 0;
            int tmp[MAX_SHORT_WINDOWS], index = 0;
            for (int i = 0; i < coder->groups.n; i++)
                for (int j = 0; j < coder->groups.len[i]; j++)
                    tmp[index++] = i;
            for (int i = 1; i < MAX_SHORT_WINDOWS; i++) {
                grouping_bits <<= 1;
                if (tmp[i] == tmp[i-1]) grouping_bits++;
            }
            PutBit(bs, grouping_bits, MAX_SHORT_WINDOWS - 1);
        }
        bits += LEN_MAX_SFBS + (MAX_SHORT_WINDOWS - 1);
    } else {
        if (writeFlag) {
            PutBit(bs, coder->sfbn, LEN_MAX_SFBL);
            PutBit(bs, 0, LEN_PRED_PRES);
        }
        bits += LEN_MAX_SFBL + LEN_PRED_PRES;
    }

    return bits;
}

static int WriteICS(BitStream *bs, CoderInfo *coder, bool commonWindow, bool writeFlag)
{
    if (writeFlag) PutBit(bs, coder->global_gain, LEN_GLOB_GAIN);
    int bits = LEN_GLOB_GAIN;

    if (!commonWindow) bits += WriteICSInfo(bs, coder, writeFlag);

    bits += writebooks(coder, bs, writeFlag);
    bits += writesf(coder, bs, writeFlag);

    if (writeFlag) PutBit(bs, 0, LEN_PULSE_PRES);
    bits += LEN_PULSE_PRES;

    TnsInfo *tns = &coder->tnsInfo;
    if (writeFlag) PutBit(bs, tns->tnsDataPresent, LEN_TNS_PRES);
    bits += LEN_TNS_PRES;

    if (tns->tnsDataPresent) {
        int nf_len = (coder->block_type == ONLY_SHORT_WINDOW) ? LEN_TNS_NFILTS : LEN_TNS_NFILTL;
        int l_len  = (coder->block_type == ONLY_SHORT_WINDOW) ? LEN_TNS_LENGTHS : LEN_TNS_LENGTHL;
        int o_len  = (coder->block_type == ONLY_SHORT_WINDOW) ? LEN_TNS_ORDERS : LEN_TNS_ORDERL;
        int num_w  = (coder->block_type == ONLY_SHORT_WINDOW) ? MAX_SHORT_WINDOWS : 1;

        for (int w = 0; w < num_w; w++) {
            TnsWindowData *win = &tns->windowData[w];
            if (writeFlag) PutBit(bs, win->numFilters, nf_len);
            bits += nf_len;

            if (win->numFilters > 0) {
                if (writeFlag) PutBit(bs, win->coefResolution - 3, LEN_TNS_COEFF_RES);
                bits += LEN_TNS_COEFF_RES;

                for (int f = 0; f < win->numFilters; f++) {
                    TnsFilterData *flt = &win->tnsFilter[f];
                    if (writeFlag) {
                        PutBit(bs, flt->length, l_len);
                        PutBit(bs, flt->order, o_len);
                    }
                    bits += l_len + o_len;

                    if (flt->order > 0) {
                        if (writeFlag) {
                            PutBit(bs, flt->direction, LEN_TNS_DIRECTION);
                            PutBit(bs, flt->coefCompress, LEN_TNS_COMPRESS);
                        }
                        bits += LEN_TNS_DIRECTION + LEN_TNS_COMPRESS;

                        int res = win->coefResolution - flt->coefCompress;
                        for (int i = 1; i <= flt->order; i++) {
                            if (writeFlag) PutBit(bs, flt->index[i] & ((1 << res) - 1), res);
                            bits += res;
                        }
                    }
                }
            }
        }
    }

    if (writeFlag) PutBit(bs, 0, LEN_GAIN_PRES);
    bits += LEN_GAIN_PRES;

    if (writeFlag) {
        BitAccumulator acc = {0};
        AccumBegin(&acc, bs);
        for (int i = 0; i < coder->datacnt; i++) {
            if (coder->s[i].len > 0) {
                AccumPutBits(&acc, (uint32_t)coder->s[i].data, coder->s[i].len);
                bits += coder->s[i].len;
            }
        }
        AccumEnd(&acc);
    } else {
        for (int i = 0; i < coder->datacnt; i++) bits += coder->s[i].len;
    }

    return bits;
}

int WriteElement(BitStream *bs, AACElement *elem, CoderInfo *coder, bool writeFlag)
{
    if (writeFlag) {
        PutBit(bs, elem->type, LEN_SE_ID);
        PutBit(bs, elem->tag, LEN_TAG);
    }
    int bits = LEN_SE_ID + LEN_TAG;

    switch (elem->type) {
        case ID_SCE:
        case ID_LFE:
            bits += WriteICS(bs, &coder[elem->channels[0]], false, writeFlag);
            break;

        case ID_CPE:
            if (writeFlag) PutBit(bs, elem->common_window, LEN_COM_WIN);
            bits += LEN_COM_WIN;

            if (elem->common_window) {
                bits += WriteICSInfo(bs, &coder[elem->channels[0]], writeFlag);
                if (writeFlag) {
                    PutBit(bs, elem->msInfo.is_present, LEN_MASK_PRES);
                    if (elem->msInfo.is_present == 1) {
                        int n = coder[elem->channels[0]].groups.n * coder[elem->channels[0]].sfbn;
                        for (int i = 0; i < n; i++) PutBit(bs, elem->msInfo.ms_used[i], LEN_MASK);
                    }
                }
                bits += LEN_MASK_PRES;
                if (elem->msInfo.is_present == 1)
                    bits += coder[elem->channels[0]].groups.n * coder[elem->channels[0]].sfbn * LEN_MASK;
            }
            bits += WriteICS(bs, &coder[elem->channels[0]], elem->common_window, writeFlag);
            bits += WriteICS(bs, &coder[elem->channels[1]], elem->common_window, writeFlag);
            break;
        default: break;
    }
    return bits;
}

static int WriteADTSHeader(struct faacEncStruct *hEncoder, BitStream *bs, bool writeFlag)
{
    if (writeFlag) {
        PutBit(bs, 0xFFF, LEN_ADTS_SYNC);
        PutBit(bs, hEncoder->config.mpegVersion, LEN_ADTS_ID);
        PutBit(bs, 0, LEN_ADTS_LAYER);
        PutBit(bs, 1, LEN_ADTS_ABSENT);
        PutBit(bs, hEncoder->config.aacObjectType - 1, LEN_ADTS_PROFILE);
        PutBit(bs, hEncoder->sampleRateIdx, LEN_ADTS_FREQ);
        PutBit(bs, 0, LEN_ADTS_PRIV);
        PutBit(bs, hEncoder->numChannels, LEN_ADTS_CH_CFG);
        PutBit(bs, 0, LEN_ADTS_ORIG);
        PutBit(bs, 0, LEN_ADTS_HOME);

        PutBit(bs, 0, LEN_ADTS_COPY_ID);
        PutBit(bs, 0, LEN_ADTS_COPY_ST);
        PutBit(bs, hEncoder->usedBytes, LEN_ADTS_FRAME);
        PutBit(bs, 0x7FF, LEN_ADTS_FULL);
        PutBit(bs, 0, LEN_ADTS_BLOCKS);
    }
    return 56;
}

static int WriteAACFillBits(BitStream *bs, int numBits, bool writeFlag)
{
    int left = numBits;
    while (left >= (LEN_SE_ID + 4)) {
        if (writeFlag) PutBit(bs, ID_FIL, LEN_SE_ID);
        left -= LEN_SE_ID;
        int bc = (left / 8 < 15) ? (left / 8) : 15;
        if (writeFlag) PutBit(bs, bc, 4);
        left -= 4;
        if (bc == 15) {
            int esc = (left / 8 - 14 < 255) ? (left / 8 - 14) : 255;
            if (writeFlag) PutBit(bs, esc, 8);
            left -= 8;
            bc = 14 + esc;
        }
        if (writeFlag) for (int i = 0; i < bc; i++) PutBit(bs, 0, 8);
        left -= bc * 8;
    }
    return left;
}

static int BuildFrame(struct faacEncStruct *hEncoder, CoderInfo *coder, AACElement *elems, int nElems, BitStream *bs, bool write)
{
    int bits = 0;
    if (hEncoder->config.outputFormat == 1) bits += WriteADTSHeader(hEncoder, bs, write);
    for (int i = 0; i < nElems; i++) bits += WriteElement(bs, &elems[i], coder, write);
    int f = (bits < (8 - LEN_SE_ID)) ? (8 - LEN_SE_ID - bits) : 0;
    f += 6;
    bits += (f - WriteAACFillBits(bs, f, write));
    if (write) PutBit(bs, ID_END, LEN_SE_ID);
    bits += LEN_SE_ID;
    int pad = (8 - (bits & 7)) & 7;
    if (write) for (int i = 0; i < pad; i++) PutBit(bs, 0, 1);
    return bits + pad;
}

int WriteBitstream(struct faacEncStruct *hEncoder, CoderInfo *coder, AACElement *elems, int nElems, BitStream *bs)
{
    int bits = BuildFrame(hEncoder, coder, elems, nElems, bs, false);
    if (bits < 0) return -1;
    hEncoder->usedBytes = (bits + 7) >> 3;
    if (hEncoder->usedBytes > bs->size) return -1;
    if (hEncoder->usedBytes > ADTS_MAX_FRAME_SIZE) return -1;
    return BuildFrame(hEncoder, coder, elems, nElems, bs, true);
}
