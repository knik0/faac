/*
 *	Function prototypes for LTP
 *
 *	Copyright (c) 1999 M. Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**************************************************************************
  Version Control Information			Method: CVS
  Identifiers:
  $Revision: 1.1 $
  $Date: 2000/10/06 14:47:27 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef _LTP_ENC_H
#define _LTP_ENC_H

#include "interface.h"
#include "bitstream.h"

/*
  Macro:	LT_BLEN
  Purpose:	Length of the history buffer.
  Explanation:	Has to hold two long windows of time domain data.  */
#ifndef	LT_BLEN
#define LT_BLEN (4 * BLOCK_LEN_LONG)
#endif

/*  Purpose:      Number of LTP coefficients. */
#define LPC 1
/*  Purpose:      Maximum LTP lag.  */
#define DELAY 2048
/*  Purpose:	Length of the bitstream element ltp_data_present.  */
#define	LEN_LTP_DATA_PRESENT 1
/*  Purpose:	Length of the bitstream element ltp_lag.  */
#define	LEN_LTP_LAG 11
/*  Purpose:	Length of the bitstream element ltp_coef.  */
#define	LEN_LTP_COEF 3
/*  Purpose:	Length of the bitstream element ltp_short_used.  */
#define	LEN_LTP_SHORT_USED 1
/*  Purpose:	Length of the bitstream element ltp_short_lag_present.  */
#define	LEN_LTP_SHORT_LAG_PRESENT 1
/*  Purpose:	Length of the bitstream element ltp_short_lag.  */
#define	LEN_LTP_SHORT_LAG 5
/*  Purpose:	Offset of the lags written in the bitstream.  */
#define	LTP_LAG_OFFSET 16
/*  Purpose:	Length of the bitstream element ltp_long_used.  */
#define	LEN_LTP_LONG_USED 1
/* Purpose:	Upper limit for the number of scalefactor bands
    		which can use lt prediction with long windows.
   Explanation:	Bands 0..MAX_LT_PRED_SFB-1 can use lt prediction.  */
#define	MAX_LT_PRED_LONG_SFB 40
/* Purpose:	Upper limit for the number of scalefactor bands
    		which can use lt prediction with short windows.
   Explanation:	Bands 0..MAX_LT_PRED_SFB-1 can use lt prediction.  */
#define	MAX_LT_PRED_SHORT_SFB 13
/* Purpose:      Buffer offset to maintain block alignment.
   Explanation:  This is only used for a short window sequence.  */
#define SHORT_SQ_OFFSET (BLOCK_LEN_LONG-(BLOCK_LEN_SHORT*4+BLOCK_LEN_SHORT/2))
/*  Purpose:	Number of codes for LTP weight. */
#define CODESIZE 8
/* Purpose:      Float type for external data
   Explanation:  - */
typedef double float_ext;

/* Type:		LT_PRED_STATUS
   Purpose:	Type of the struct holding the LTP encoding parameters.
   Explanation:	-  */
typedef struct
{
    short buffer[LT_BLEN];
    double pred_mdct[2 * BLOCK_LEN_LONG];
    int weight_idx;
    double weight;
    int sbk_prediction_used[MAX_SHORT_WINDOWS];
    int sfb_prediction_used[MAX_SCFAC_BANDS];
    int *delay;
    int global_pred_flag;
    int side_info;
} LT_PRED_STATUS;

double snr_pred (double *mdct_in, double *mdct_pred, int *sfb_flag,
                        int *sfb_offset, enum WINDOW_TYPE block_type, int side_info,
                        int num_of_sfb);

void prediction (short *buffer, double *predicted_samples, double *weight,
                        int delay, int flen);

int estimate_delay (double *sb_samples, short *x_buffer);

void pitch (double *sb_samples, double *sb_samples_pred, short *x_buffer,
                   int *ltp_coef, int delay, int flen);

void init_lt_pred(LT_PRED_STATUS * lt_status);

int ltp_enc(double *p_spectrum, double *p_time_signal,
		       enum WINDOW_TYPE win_type, Window_shape win_shape,
		       int *sfb_offset, int num_of_sfb,
		       LT_PRED_STATUS *lt_status);

void ltp_reconstruct(double *p_spectrum, enum WINDOW_TYPE win_type, 
                            Window_shape win_shape, 
                            int *sfb_offset, int num_of_sfb,
                            LT_PRED_STATUS *lt_status);

int ltp_encode (BsBitStream *bs, enum WINDOW_TYPE win_type, int num_of_sfb, 
                       LT_PRED_STATUS *lt_status, int write_flag);

#endif /* not defined _LTP_ENC_H */

