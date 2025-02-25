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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: coder.h,v 1.13 2005/02/02 07:49:10 sur Exp $
 */

#ifndef CODER_H
#define CODER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Allow encoding of Digital Radio Mondiale (DRM) */
//#define DRM

/* Allow encoding of Digital Radio Mondiale (DRM) with transform length 1024 */
//#define DRM_1024

#define MAX_CHANNELS 64

#ifdef DRM
#ifdef DRM_1024
# define FRAME_LEN 1024
# define BLOCK_LEN_LONG 1024
# define BLOCK_LEN_SHORT 128
#else
# define FRAME_LEN 960
# define BLOCK_LEN_LONG 960
# define BLOCK_LEN_SHORT 120
#endif /* DRM_1024 */
#else
# define FRAME_LEN 1024
# define BLOCK_LEN_LONG 1024
# define BLOCK_LEN_SHORT 128
#endif

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

#define TNS_MAX_ORDER 20
#define DEF_TNS_GAIN_THRESH 1.4
#define DEF_TNS_COEFF_THRESH 0.1
#define DEF_TNS_COEFF_RES 4
#define DEF_TNS_RES_OFFSET 3
#define LEN_TNS_NFILTL 2
#define LEN_TNS_NFILTS 1

#define LPC 2

typedef struct {
    int order;                           /* Filter order */
    int direction;                       /* Filtering direction */
    int coefCompress;                    /* Are coeffs compressed? */
    int length;                          /* Length, in bands */
    double aCoeffs[TNS_MAX_ORDER+1];     /* AR Coefficients */
    double kCoeffs[TNS_MAX_ORDER+1];     /* Reflection Coefficients */
    int index[TNS_MAX_ORDER+1];          /* Coefficient indices */
} TnsFilterData;

typedef struct {
    int numFilters;                             /* Number of filters */
    int coefResolution;                         /* Coefficient resolution */
    TnsFilterData tnsFilter[1<<LEN_TNS_NFILTL]; /* TNS filters */
} TnsWindowData;

typedef struct {
    int tnsDataPresent;
    int tnsMinBandNumberLong;
    int tnsMinBandNumberShort;
    int tnsMaxBandsLong;
    int tnsMaxBandsShort;
    int tnsMaxOrderLong;
    int tnsMaxOrderShort;
    TnsWindowData windowData[MAX_SHORT_WINDOWS]; /* TNS data per window */
} TnsInfo;

typedef struct {
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

#ifdef DRM
    int num_data_cw[FRAME_LEN];
    int cur_cw;
    int all_sfb;

    int iLenLongestCW;
    int iLenReordSpData;
#endif

    TnsInfo tnsInfo;
} CoderInfo;

typedef struct {
  unsigned long sampling_rate;  /* the following entries are for this sampling rate */
  int num_cb_long;
  int num_cb_short;
  int cb_width_long[NSFB_LONG];
  int cb_width_short[NSFB_SHORT];
} SR_INFO;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CODER_H */
