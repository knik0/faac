/*
 *	Long Term Prediction
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
  $Revision: 1.14 $
  $Date: 2000/10/05 13:04:05 $ (check in)
  $Author: menno $
  *************************************************************************/

/**************************************************************************
  External Objects Needed
  *************************************************************************/
/*
  Standard library declarations.  */
#include <stdio.h>

/*
  Interface to related modules.  */
#include "interface.h"
#include "transfo.h"
#include "bitstream.h"
#include "nok_ltp_common.h"
#include "nok_pitch.h"

/**************************************************************************
  External Objects Provided
  *************************************************************************/
#include "nok_ltp_enc.h"

/**************************************************************************
  Internal Objects
  *************************************************************************/
#include "nok_ltp_common_internal.h"

/* short double_to_int (double sig_in); */
#define double_to_int(sig_in) \
  ((sig_in) > 32767 ? 32767 : (\
    (sig_in) < -32768 ? -32768 : (\
      (sig_in) > 0.0 ? (sig_in)+0.5 : (\
        (sig_in) <= 0.0 ? (sig_in)-0.5 : 0))))


/**************************************************************************
  Title:	nok_init_lt_pred

  Purpose:	Initialize the history buffer for long term prediction

  Usage:        nok_init_lt_pred (lt_status)

  Input:	lt_status
  			- buffer: history buffer
                        - pred_mdct: prediction transformed to frequency 
                          domain
                        - weight_idx : 
                          3 bit number indicating the LTP coefficient in 
                          the codebook 
                        - sbk_prediction_used:
                          1 bit for each subblock indicating wheather
                          LTP is used in that subblock 
                        - sfb_prediction_used:
                          1 bit for each scalefactor band (sfb) where LTP 
                          can be used indicating whether LTP is switched 
                          on (1) /off (0) in that sfb.
                        - delay: LTP lag
                        - side_info: LTP side information

  Output:	lt_status
  			- buffer: filled with 0
                        - pred_mdct: filled with 0
                        - weight_idx : filled with 0
                        - sbk_prediction_used: filled with 0
                        - sfb_prediction_used: filled with 0
                        - delay: filled with 0
                        - side_info: filled with 1

  References:	-

  Explanation:	-

  Author(s):	Mikko Suonio
  *************************************************************************/
void
nok_init_lt_pred (NOK_LT_PRED_STATUS *lt_status)
{
	int i;

	for (i = 0; i < NOK_LT_BLEN; i++)
		lt_status->buffer[i] = 0;

	for (i = 0; i < BLOCK_LEN_LONG; i++)
		lt_status->pred_mdct[i] = 0;

	lt_status->weight_idx = 0;
	for(i=0; i<MAX_SHORT_IN_LONG_BLOCK; i++)
		lt_status->sbk_prediction_used[i] = lt_status->delay[i] = 0;

	for(i=0; i<MAX_SCFAC_BANDS; i++)
		lt_status->sfb_prediction_used[i] = 0;

	lt_status->side_info = LEN_LTP_DATA_PRESENT;
}


/**************************************************************************
  Title:	nok_ltp_enc

  Purpose:      Performs long term prediction.

  Usage:	nok_ltp_enc(p_spectrum, p_time_signal, win_type, win_shape, 
                            sfb_offset, num_of_sfb, lt_status, buffer_update)

  Input:        p_spectrum    - spectral coefficients
                p_time_signal - time domain input samples
                win_type      - window sequence (frame, block) type
                win_shape     - shape of the mdct window
                sfb_offset    - scalefactor band boundaries
                num_of_sfb    - number of scalefactor bands in each block

  Output:	p_spectrum    - residual spectrum
                lt_status     - buffer: history buffer
                              - pred_mdct:prediction transformed to frequency domain
                                for subsequent use
                              - weight_idx : 
                                3 bit number indicating the LTP coefficient in 
                                the codebook 
                              - sbk_prediction_used: 
                                1 bit for each subblock indicating wheather
                                LTP is used in that subblock 
                              - sfb_prediction_used:
                                1 bit for each scalefactor band (sfb) where LTP 
                                can be used indicating whether LTP is switched 
                                on (1) /off (0) in that sfb.
                              - delay: LTP lag
                              - side_info: LTP side information
                        
  References:	1.) estimate_delay in nok_pitch.c
                2.) pitch in nok_pitch.c
                3.) buffer2freq
                4.) snr_pred in nok_pitch.c
                5.) freq2buffer
                6.) double_to_int

  Explanation:  -

  Author(s):	Juha Ojanpera
  *************************************************************************/

int
nok_ltp_enc(double *p_spectrum, double *p_time_signal, enum WINDOW_TYPE win_type,
            Window_shape win_shape, int *sfb_offset, int num_of_sfb,
            NOK_LT_PRED_STATUS *lt_status)
{
    int i;
    int last_band;
    double num_bit[MAX_SHORT_WINDOWS];
    double predicted_samples[2 * BLOCK_LEN_LONG];

    lt_status->global_pred_flag = 0;
    lt_status->side_info = 1;

    switch(win_type)
    {
	case ONLY_LONG_WINDOW:
	case LONG_SHORT_WINDOW:
	case SHORT_LONG_WINDOW:
		last_band = (num_of_sfb < NOK_MAX_LT_PRED_LONG_SFB) ? num_of_sfb : NOK_MAX_LT_PRED_LONG_SFB;

//		lt_status->delay[0] = estimate_delay (p_time_signal, lt_status->buffer, 2 * BLOCK_LEN_LONG);
		lt_status->delay[0] = estimate_delay (p_time_signal, lt_status->buffer);

//		fprintf(stderr, "(LTP) lag : %i ", lt_status->delay[0]);

		pitch (p_time_signal, predicted_samples, lt_status->buffer,
			&lt_status->weight_idx, lt_status->delay[0], 2 * BLOCK_LEN_LONG);

		/* Transform prediction to frequency domain and save it for subsequent use. */
		buffer2freq (predicted_samples, lt_status->pred_mdct, NULL, win_type, WS_SIN, WS_SIN, MNON_OVERLAPPED);

		lt_status->side_info = LEN_LTP_DATA_PRESENT + last_band + LEN_LTP_LAG + LEN_LTP_COEF;

		num_bit[0] = snr_pred (p_spectrum, lt_status->pred_mdct,
			lt_status->sfb_prediction_used, sfb_offset,
			win_type, lt_status->side_info, last_band);

//		if (num_bit[0] > 0) {
//			fprintf(stderr, "(LTP) lag : %i ", lt_status->delay[0]);
//			fprintf(stderr, " bit gain : %f\n", num_bit[0]);
//		}

		lt_status->global_pred_flag = (num_bit[0] == 0.0) ? 0 : 1;

		if(lt_status->global_pred_flag)
			for (i = 0; i < sfb_offset[last_band]; i++)
				p_spectrum[i] -= lt_status->pred_mdct[i];
		else
			lt_status->side_info = 1;
        break;

	case ONLY_SHORT_WINDOW:
		break;
    }

    return (lt_status->global_pred_flag);
}


/**************************************************************************
  Title:	nok_ltp_reconstruct

  Purpose:      Updates LTP history buffer.

  Usage:	nok_ltp_reconstruct(p_spectrum, win_type,  win_shape, 
                                    block_size_long, block_size_medium,
                                    block_size_short, sfb_offset, 
                                    num_of_sfb, lt_status)

  Input:        p_spectrum    - reconstructed spectrum
                win_type      - window sequence (frame, block) type
                win_shape     - shape of the mdct window
                sfb_offset    - scalefactor band boundaries
                num_of_sfb    - number of scalefactor bands in each block

  Output:	p_spectrum    - reconstructed spectrum
                lt_status     - buffer: history buffer

  References:	1.) buffer2freq
                2.) freq2buffer
                3.) double_to_int

  Explanation:  -

  Author(s):	Juha Ojanpera
  *************************************************************************/

void
nok_ltp_reconstruct(double *p_spectrum, enum WINDOW_TYPE win_type, 
                    Window_shape win_shape, int *sfb_offset, int num_of_sfb,
                    NOK_LT_PRED_STATUS *lt_status)
{
	int i, j, last_band;
	double predicted_samples[2 * BLOCK_LEN_LONG];
	double overlap_buffer[2 * BLOCK_LEN_LONG];

    
	switch(win_type)
	{
	case ONLY_LONG_WINDOW:
	case LONG_SHORT_WINDOW:
	case SHORT_LONG_WINDOW:
		last_band = (num_of_sfb < NOK_MAX_LT_PRED_LONG_SFB) ? num_of_sfb : NOK_MAX_LT_PRED_LONG_SFB;

		if(lt_status->global_pred_flag)
			for (i = 0; i < sfb_offset[last_band]; i++)
				p_spectrum[i] += lt_status->pred_mdct[i];

		/* Finally update the time domain history buffer. */
		freq2buffer (p_spectrum, predicted_samples, overlap_buffer, win_type, WS_SIN, WS_SIN, MNON_OVERLAPPED);

		for (i = 0; i < NOK_LT_BLEN - BLOCK_LEN_LONG; i++)
			lt_status->buffer[i] = lt_status->buffer[i + BLOCK_LEN_LONG];

		j = NOK_LT_BLEN - 2 * BLOCK_LEN_LONG;
		for (i = 0; i < BLOCK_LEN_LONG; i++)
		{
			lt_status->buffer[i + j] =
				(short)double_to_int (predicted_samples[i] + lt_status->buffer[i + j]);
			lt_status->buffer[NOK_LT_BLEN - BLOCK_LEN_LONG + i] =
				(short)double_to_int (predicted_samples[i + BLOCK_LEN_LONG]);
		}
		break;

    case ONLY_SHORT_WINDOW:
#if 0
		for (i = 0; i < NOK_LT_BLEN - block_size_long; i++)
			lt_status->buffer[i] = lt_status->buffer[i + block_size_long];

		for (i = NOK_LT_BLEN - block_size_long; i < NOK_LT_BLEN; i++)
			lt_status->buffer[i] = 0;

		for (i = 0; i < block_size_long; i++)
			overlap_buffer[i] = 0;

		/* Finally update the time domain history buffer. */
		freq2buffer (p_spectrum, predicted_samples, overlap_buffer, win_type, block_size_long,
			block_size_medium, block_size_short, win_shape, MNON_OVERLAPPED);

		for(sw = 0; sw < MAX_SHORT_WINDOWS; sw++)
		{
			i = NOK_LT_BLEN - 2 * block_size_long + SHORT_SQ_OFFSET + sw * block_size_short;
			for (j = 0; j < 2 * block_size_short; j++)
				lt_status->buffer[i + j] = double_to_int (predicted_samples[sw * block_size_short * 2 + j] + 
				lt_status->buffer[i + j]);
		}
#endif
		break;
	}

	return;
}                      


/**************************************************************************
  Title:	nok_ltp_encode

  Purpose:      Writes LTP parameters to the bit stream.

  Usage:	nok_ltp_encode (bs, win_type, num_of_sfb, lt_status)

  Input:        bs         - bit stream
                win_type   - window sequence (frame, block) type
                num_of_sfb - number of scalefactor bands
                lt_status  - side_info:
			     1, if prediction not used in this frame
			     >1 otherwise
                           - weight_idx : 
                             3 bit number indicating the LTP coefficient in 
                             the codebook
                           - sfb_prediction_used:
                             1 bit for each scalefactor band (sfb) where LTP 
                             can be used indicating whether LTP is switched 
                             on (1) /off (0) in that sfb.
                           - delay: LTP lag

  Output:	-

  References:	1.) BsPutBit

  Explanation:  -

  Author(s):	Juha Ojanpera
  *************************************************************************/

int
nok_ltp_encode (BsBitStream *bs, enum WINDOW_TYPE win_type, int num_of_sfb, 
                NOK_LT_PRED_STATUS *lt_status, int write_flag)
{
	int i, last_band;
//	int first_subblock;
//	int prev_subblock;
	int bit_count = 0;


	bit_count += 1;
	
	if (lt_status->side_info > 1)
	{
		if(write_flag)
			BsPutBit (bs, 1, 1);    	/* LTP used */

		switch(win_type)
		{
		case ONLY_LONG_WINDOW:
		case LONG_SHORT_WINDOW:
		case SHORT_LONG_WINDOW:
			bit_count += LEN_LTP_LAG;
			bit_count += LEN_LTP_COEF;
			if(write_flag)
			{
				BsPutBit (bs, lt_status->delay[0], LEN_LTP_LAG);
				BsPutBit (bs, lt_status->weight_idx,  LEN_LTP_COEF);
			}

			last_band = (num_of_sfb < NOK_MAX_LT_PRED_LONG_SFB) ? num_of_sfb : NOK_MAX_LT_PRED_LONG_SFB;
			bit_count += last_band;
			if(write_flag)
			{
				for (i = 0; i < last_band; i++)
					BsPutBit (bs, lt_status->sfb_prediction_used[i], LEN_LTP_LONG_USED);
			}
			break;
			
		case ONLY_SHORT_WINDOW:
#if 0
			for(i=0; i < MAX_SHORT_WINDOWS; i++)
			{
				if(lt_status->sbk_prediction_used[i])
				{
					first_subblock = i;
					break;
				}
			}
			bit_count += LEN_LTP_LAG;
			bit_count += LEN_LTP_COEF;
			
			if(write_flag)
			{
				BsPutBit (bs, lt_status->delay[first_subblock], LEN_LTP_LAG);
				BsPutBit (bs, lt_status->weight_idx,  LEN_LTP_COEF);
			}

			prev_subblock = first_subblock;
			for(i = 0; i < MAX_SHORT_WINDOWS; i++)
			{
				bit_count += LEN_LTP_SHORT_USED;
				if(write_flag)
					BsPutBit (bs, lt_status->sbk_prediction_used[i], LEN_LTP_SHORT_USED);

				if(lt_status->sbk_prediction_used[i])
				{
					if(i > first_subblock)
					{
						int diff;
						
						diff = lt_status->delay[prev_subblock] - lt_status->delay[i];
						if(diff)
						{
							bit_count += 1;
							bit_count += LEN_LTP_SHORT_LAG;
							if(write_flag)
							{
								BsPutBit (bs, 1, 1);
								BsPutBit (bs, diff + NOK_LTP_LAG_OFFSET, LEN_LTP_SHORT_LAG);
							}
						}
						else
						{
							bit_count += 1;
							if(write_flag)
								BsPutBit (bs, 0, 1);
						}
					}
				}
			}
			break;
#endif
		default:
			//        CommonExit(1, "nok_ltp_encode : unsupported window sequence %i", win_type);
			break;
		}
	}
	else
		if(write_flag)
			BsPutBit (bs, 0, 1);    	/* LTP not used */

	return (bit_count);
}


/**************************************************************************
  Title:	double_to_int

  Purpose:      Converts floating point format to integer (16-bit).

  Usage:	y = double_to_int(sig_in)

  Input:	sig_in  - floating point number

  Output:	y  - integer number

  References:	-

  Explanation:  -

  Author(s):	Juha Ojanpera
  *************************************************************************/

/*short
double_to_int (double sig_in)
{
	short sig_out;

	if (sig_in > 32767)
		sig_out = 32767;
	else if (sig_in < -32768)
		sig_out = -32768;
	else if (sig_in > 0.0)
		sig_out = (short) (sig_in + 0.5);
	else if (sig_in <= 0.0)
		sig_out = (short) (sig_in - 0.5);

	return (sig_out);
}
*/
