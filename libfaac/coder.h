/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
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

#ifndef CODER_H
#define CODER_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define FRAME_LEN 1024
#define BLOCK_LEN_LONG 1024
#define BLOCK_LEN_SHORT 128

#define NSFB_LONG  51
#define NSFB_SHORT 15
#define MAX_SHORT_WINDOWS 8
#define MAX_SCFAC_BANDS ((NSFB_SHORT+1)*MAX_SHORT_WINDOWS)

enum WINDOW_TYPE {
    ONLY_LONG_WINDOW,
    LONG_SHORT_WINDOW,
    ONLY_SHORT_WINDOW,
    SHORT_LONG_WINDOW
};

#define TNS_MAX_ORDER 12
#define DEF_TNS_COEFF_THRESH 0.1f
#define DEF_TNS_COEFF_RES 4
#define DEF_TNS_RES_OFFSET 3
/* Bound on TnsWindowData.tnsFilter[]. Must stay in sync with the bitstream
 * field width LEN_TNS_NFILTL (channels.h) -- checked by a _Static_assert in
 * channels.c, since this header can't include channels.h without a cycle. */
#define TNS_MAX_FILTERS 4

typedef struct {
    int order;                           /* Filter order */
    int direction;                       /* Filtering direction */
    int coefCompress;                    /* Are coeffs compressed? */
    int length;                          /* Length, in bands */
    float aCoeffs[TNS_MAX_ORDER+1];       /* LPC (AR) coefficients */
    int index[TNS_MAX_ORDER+1];          /* Quantized reflection-coeff indices */
} TnsFilterData;

typedef struct {
    int numFilters;                             /* Number of filters */
    int coefResolution;                         /* Coefficient resolution */
    TnsFilterData tnsFilter[TNS_MAX_FILTERS];    /* TNS filters */
} TnsWindowData;

typedef struct {
    int tnsDataPresent;
    int tnsMinBandNumberLong;
    int tnsMaxBandsLong;
    int tnsNumSwbLong;      /* full swb count for the sample rate (decoder's num_swb) */
    TnsWindowData windowData;   /* long-only: one window per frame, not per-short-window */
} TnsInfo;

typedef struct CoderInfo {
    int window_shape;
    int prev_window_shape;
    int block_type;
    int desired_block_type;

    int global_gain;
    int sf[MAX_SCFAC_BANDS];
    int book[MAX_SCFAC_BANDS];
    int bandcnt;
    int sfbn;
    int sfb_offset[NSFB_LONG + 1];

    struct {
        int n;
        int len[MAX_SHORT_WINDOWS];
    } groups;

    /* worst case: one codeword with two escapes per two spectral lines */
#define DATASIZE (3*FRAME_LEN/2)

    struct {
        int data;
        int len;
    } s[DATASIZE];
    int datacnt;


    TnsInfo tnsInfo;
} CoderInfo;

typedef struct {
  unsigned long sampling_rate;  /* the following entries are for this sampling rate */
  int num_cb_long;
  int num_cb_short;
  int cb_width_long[NSFB_LONG];
  int cb_width_short[NSFB_SHORT];
} SR_INFO;

/* Scalefactor-band layout per sampling_rate_index, shared by frame.c and sbr.c. */
extern SR_INFO srInfo[12 + 1];

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CODER_H */
