/*
 * FAAC - Freeware Advanced Audio Coder
 * Huffman coding per ISO/IEC 14496-3
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

#ifndef HUFF2_H
#define HUFF2_H

#include "bitstream.h"

/* Huffman Codebooks */
enum {
    HCB_ZERO = 0,
    HCB_1, HCB_2, HCB_3, HCB_4, HCB_5,
    HCB_6, HCB_7, HCB_8, HCB_9, HCB_10,
    HCB_ESC = 11,
    HCB_DELTA = 12,   /* scalefactor-delta book (book12) */
    HCB_PNS = 13,
    HCB_INTENSITY2 = 14,
    HCB_INTENSITY = 15,
    HCB_NONE
};

/* Spectral books come in range-pairs {base, base+1} sharing a tuple shape but a
 * different codeword table; the pair's reach is its Largest Absolute Value. */
enum {
    LAV_1   = 1,    /* HCB_1, HCB_2  */
    LAV_2   = 2,    /* HCB_3, HCB_4  */
    LAV_4   = 4,    /* HCB_5, HCB_6  */
    LAV_7   = 7,    /* HCB_7, HCB_8  */
    LAV_12  = 12,   /* HCB_9, HCB_10 */
    LAV_ESC = 16    /* HCB_ESC: |q| >= 16 spills into an escape suffix */
};

/* Radix of each book's positional codeword index (so the magic 27/9/13/17 read
 * as the powers they are). 4-tuples pack as radix^3..^0, 2-tuples as radix^1..^0. */
enum {
    DIM_S4    = 3,  /* HCB_1, HCB_2:  signed 4-tuple, 3^4 = 81  */
    DIM_M4    = 3,  /* HCB_3, HCB_4:  mag 4-tuple,    3^4 = 81  */
    DIM_S2    = 9,  /* HCB_5, HCB_6:  signed 2-tuple, 9^2 = 81  */
    DIM_M2_7  = 8,  /* HCB_7, HCB_8:  mag 2-tuple,    8^2 = 64  */
    DIM_M2_12 = 13, /* HCB_9, HCB_10: mag 2-tuple,   13^2 = 169 */
    DIM_ESC   = 17  /* HCB_ESC:       mag 2-tuple,   17^2 = 289 */
};

/* Maximum value representable by HCB_ESC escape sequences.
 * Values >= 8192 would cause bitstream overflow/sync loss. */
#define MAX_HUFF_ESC_VAL 8191

/* Scalefactor step constants (ISO 14496-3 §8.3.4): one SF unit = 2^(1/4) in amplitude.
 * AMPL converts a log10 amplitude ratio to scalefactor index steps (= 4/log10(2)).
 * ENRG is AMPL/2 — use when the input ratio is energy (squared amplitude). */
#define SF_STEP_AMPL  13.287712379549461f
#define SF_STEP_ENRG  (SF_STEP_AMPL / 2.0f)

/* Scalefactor Management */
enum {
    /* Baseline scalefactor value used in bitstream */
    SF_OFFSET = 100,
    /* Minimum allowable scalefactor to prevent underflow */
    SF_MIN = 10,
    /* PNS predictor initialization offset (starts at floor) */
    SF_PNS_OFFSET = SF_OFFSET - SF_MIN,
    /* Max allowed difference between successive scalefactors (AAC spec) */
    SF_DELTA = 60,
    /* Max absolute scalefactor / global_gain (8-bit bitstream field) */
    SF_MAX_ABS = 255,
};

/**
 * Restrict scalefactor delta to the spec-defined +/- SF_DELTA range.
 * This ensures the delta remains valid for HCB_DELTA Huffman encoding.
 */
static inline int clamp_sf_diff(int diff)
{
    if (diff > SF_DELTA)
        return SF_DELTA;
    if (diff < -SF_DELTA)
        return -SF_DELTA;
    return diff;
}

/* Forward declaration for CoderInfo */
struct CoderInfo;

int huffbook(struct CoderInfo *coder, int *qs, int len);
int writebooks(struct CoderInfo *coder, BitStream *stream, int writeFlag);
int writesf(struct CoderInfo *coder, BitStream *bitStream, int writeFlag);

#endif /* HUFF2_H */
