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
  $Revision: 1.2 $
  $Date: 2000/11/01 14:05:32 $ (check in)
  $Author: menno $
  *************************************************************************/

#include <stdio.h>
#include <math.h>

#include "quant.h"
#include "interface.h"
#include "transfo.h"
#include "bitstream.h"
#include "ltp_enc.h"


/* short double_to_int (double sig_in); */
#define double_to_int(sig_in) \
  ((sig_in) > 32767 ? 32767 : (\
    (sig_in) < -32768 ? -32768 : (\
      (sig_in) > 0.0 ? (sig_in)+0.5 : (\
        (sig_in) <= 0.0 ? (sig_in)-0.5 : 0))))


void lnqgj (double (*a)[LPC + 1]);

void w_quantize (double *freq, int *ltp_idx);

/*  Purpose:	Codebook for LTP weight coefficients.  */
double codebook[CODESIZE] =
{
  0.570829,
  0.696616,
  0.813004,
  0.911304,
  0.984900,
  1.067894,
  1.194601,
  1.369533
};

/**************************************************************************
  Title:	snr_pred

  Purpose:	Determines is it feasible to employ long term prediction in 
                the encoder.

  Usage:        y = snr_pred(mdct_in, mdct_pred, sfb_flag, sfb_offset, 
                             block_type, side_info, num_of_sfb)

  Input:        mdct_in     -  spectral coefficients
                mdct_pred   -  predicted spectral coefficients
                sfb_flag    -  an array of flags indicating whether LTP is 
                               switched on (1) /off (0). One bit is reseved 
                               for each sfb.
                sfb_offset  -  scalefactor boundaries
                block_type  -  window sequence type
		side_info   -  LTP side information
		num_of_sfb  -  number of scalefactor bands

  Output:	y - number of bits saved by using long term prediction     

  References:	-

  Explanation:	-

  Author(s):	Juha Ojanpera, Lin Yin
  *************************************************************************/

double
snr_pred (double *mdct_in,
	  double *mdct_pred,
	  int *sfb_flag,
	  int *sfb_offset,
	  enum WINDOW_TYPE block_type,
	  int side_info,
	  int num_of_sfb
)
{
	int i, j, flen;
	double snr_limit;
	double num_bit, snr[NSFB_LONG];
	double temp1, temp2;
	double energy[BLOCK_LEN_LONG], snr_p[BLOCK_LEN_LONG];


	if (block_type != ONLY_SHORT_WINDOW)
    {
		flen = BLOCK_LEN_LONG;
		snr_limit = 1.e-30;
    }
	else
    {
		flen = BLOCK_LEN_SHORT;
		snr_limit = 1.e-20;
    }

	for (i = 0; i < flen; i++)
    {
		energy[i] = mdct_in[i] * mdct_in[i];
		snr_p[i] = (mdct_in[i] - mdct_pred[i]) * (mdct_in[i] - mdct_pred[i]);
    }

	num_bit = 0.0;

	for (i = 0; i < num_of_sfb; i++)
    {
		temp1 = 0.0;
		temp2 = 0.0;
		for (j = sfb_offset[i]; j < sfb_offset[i + 1]; j++)
		{
			temp1 += energy[j];
			temp2 += snr_p[j];
		}

		if (temp2 < snr_limit)
			temp2 = snr_limit;

		if (temp1 > /*1.e-20*/0.0)
			snr[i] = -10. * log10 (temp2 / temp1);
		else
			snr[i] = 0.0;

		sfb_flag[i] = 1;

		if (block_type != ONLY_SHORT_WINDOW)
		{
			if (snr[i] <= 0.0)
			{
				sfb_flag[i] = 0;
				for (j = sfb_offset[i]; j < sfb_offset[i + 1]; j++)
					mdct_pred[j] = 0.0;
			} else {
				num_bit += snr[i] / 6. * (sfb_offset[i + 1] - sfb_offset[i]);
			}
		}
    }

	if (num_bit < side_info)
    {
		num_bit = 0.0;
		for (j = 0; j < flen; j++)
			mdct_pred[j] = 0.0;
		for (i = 0; i < num_of_sfb; i++)
			sfb_flag[i] = 0;
    }
	else
		num_bit -= side_info;

	return (num_bit);
}


/**************************************************************************
  Title:	prediction

  Purpose:	Predicts current frame from the past output samples.

  Usage:        prediction(buffer, predicted_samples, weight, delay, flen)

  Input:	buffer	          - past reconstructed output samples
                weight            - LTP gain (scaling factor)
		delay             - LTP lag (optimal delay)
                flen              - length of the frame

  Output:	predicted_samples - predicted samples

  References:	-

  Explanation:  -

  Author(s):	Juha Ojanpera, Lin Yin
  *************************************************************************/

void
prediction (short *buffer,
	    double *predicted_samples,
	    double *weight,
	    int delay,
	    int flen
)
{

  int i, j;
  int offset;

  for (i = 0; i < flen; i++)
    {
      offset = LT_BLEN - flen - delay + i - (LPC - 1) / 2;
      predicted_samples[i] = 0.0;
      for (j = 0; j < LPC; j++)
	predicted_samples[i] += weight[j] * buffer[offset + j];
    }

}


/**************************************************************************
  Title:	estimate_delay

  Purpose:      Estimates optimal delay between current frame and past
                reconstructed output samples. The lag between 0...DELAY-1 
                with the maximum correlation becomes the LTP lag (or delay).

  Usage:        y = estimate_delay(sb_samples, x_buffer, flen)

  Input:	sb_samples - current frame
                x_buffer   - past reconstructed output samples
                flen       - length of the frame

  Output:	y  - LTP lag

  References:	-

  Explanation:	-

  Author(s):	Juha Ojanpera, Lin Yin
  *************************************************************************/

int estimate_delay (double *sb_samples,
		    short *x_buffer
//		    ,int flen
		    )
{
	int i, j;
	int delay;
	double corr[DELAY],corrtmp;
	double p_max, energy;

	p_max = 0.0;
	delay = 0;
		energy = 0.0;
		corr[0] = 0.0;
		for (j = 1; j < 2049; j++)
		{
			corr[0] += x_buffer[LT_BLEN - j] * sb_samples[2048 - j];
			energy += x_buffer[LT_BLEN - j] * x_buffer[LT_BLEN - j];
		}
		corrtmp=corr[0];
		if (energy != 0.0)
			corr[0] = corr[0] / sqrt(energy);
		else
			corr[0] = 0.0;

		if (p_max < corr[0])
		{
			p_max = corr[0];
			delay = 0;
		}

	/* Used to look like this:
	   for (i = 1; i < DELAY; i+=16)
	   Because the new code by Oxygene2000 is so fast, we can now look for the
	   delay in steps of 1, instead of 16. Thus giving a more accurate delay estimation
	*/
	for (i = 1; i < DELAY; i++)
	{
		energy -= x_buffer[LT_BLEN - i] * x_buffer[LT_BLEN - i];
		energy += x_buffer[LT_BLEN - i - 2048] * x_buffer[LT_BLEN - i - 2048]; //2048=j_max
		corr[i] = corrtmp;
		corr[i] -= x_buffer[LT_BLEN - i] * sb_samples[2047];
		corr[i] += x_buffer[LT_BLEN - i - 2048] * sb_samples[0];
		corrtmp=corr[i];
//flen=2048
//		for (j = 1; j < 2049; j++) //2049=flen+1
//		{
//			corr[i] += x_buffer[LT_BLEN - i - j] * sb_samples[2048 - j];
//			energy += x_buffer[LT_BLEN - i - j] * x_buffer[LT_BLEN - i - j];
//		}
		if (energy != 0.0)
			corr[i] = corr[i] / sqrt(energy);
		else
			corr[i] = 0.0;

		if (p_max < corr[i])
		{
			p_max = corr[i];
			delay = i;
		}
	}

//	if (delay < (LPC - 1) / 2)
//		delay = (LPC - 1) / 2;
	if (delay<0) delay=0; //for LPC=1

	/*
	fprintf(stdout, "delay : %d ... ", delay);
	*/

	return delay;
}


/**************************************************************************
  Title:	pitch

  Purpose:	Calculates LTP gains, quantizes them and finally scales
                the past output samples (from 'delay' to 'delay'+'flen') 
                to get the predicted frame.

  Usage:	pitch(sb_samples, sb_samples_pred, x_buffer, ltp_coef, 
                      delay, flen)

  Input:        sb_samples      - current frame
                x_buffer        - past reconstructed output samples
                delay           - LTP lag
                flen            - length of the frame

  Output:	sb_samples_pred - predicted frame
                ltp_coef        - indices of the LTP gains

  References:	1.)  lnqgj
                2.)  w_quantize
                3.)  prediction

  Explanation:	-

  Author(s):	Juha Ojanpera, Lin Yin
  *************************************************************************/

void
pitch (double *sb_samples,
       double *sb_samples_pred,
       short *x_buffer,
       int *ltp_coef,
       int delay,
       int flen
)
{
	int i, k, j;
	int offset1, offset2;
	double weight[LPC];
	double r[LPC][LPC + 1];


	for (i = 0; i < LPC; i++)
		for (j = 0; j < LPC; j++)
		{
			offset1 = LT_BLEN - flen - delay + i - (LPC - 1) / 2;
			offset2 = LT_BLEN - flen - delay + j - (LPC - 1) / 2;
			r[i][j] = 0.0;
			for (k = 0; k < flen; k++)
				r[i][j] += x_buffer[offset1 + k] * x_buffer[offset2 + k];
		}
	for (i = 0; i < LPC; i++)
		r[i][i] = 1.010 * r[i][i];

	for (i = 0; i < LPC; i++)
    {
		offset1 = LT_BLEN - flen - delay + i - (LPC - 1) / 2; 
		r[i][LPC] = 0.0;
		for (k = 0; k < flen; k++)
			r[i][LPC] += x_buffer[offset1 + k] * sb_samples[k];
    }

	for (i = 0; i < LPC; i++)
		weight[i] = 0.0;

	if (r[(LPC - 1) / 2][(LPC - 1) / 2] != 0.0)
		weight[(LPC - 1) / 2] = r[(LPC - 1) / 2][LPC] / r[(LPC - 1) / 2][(LPC - 1) / 2];

	lnqgj (r);

	for (i = 0; i < LPC; i++)
		weight[i] = r[i][LPC];

	/* 
	fprintf(stdout, "unquantized weight : %f ... ", weight[0]);
	*/

	w_quantize (weight, ltp_coef);

	/*
	fprintf(stdout, "quantized weight : %f\n", weight[0]);    
	*/

	prediction (x_buffer, sb_samples_pred, weight, delay, flen);
}


/**************************************************************************
  Title:	lnqgj

  Purpose:	Calculates LTP gains.

  Usage:	lnqgj(a)

  Input:	a - auto-correlation matrix

  Output:	a - LTP gains (in the last column)   

  References:	-

  Explanation:	-

  Author(s):	Juha Ojanpera, Lin Yin
  *************************************************************************/

void
lnqgj (double (*a)[LPC + 1])
{
	int nn, nnpls1, ip, i, j;
	double p, w;

	for (nn = 0; nn < LPC; nn++)
    {
		p = 0.0;
		nnpls1 = nn + 1;
		for (i = nn; i < LPC; i++)
			if (p - fabs (a[i][nn]) < 0)
			{
				p = fabs (a[i][nn]);
				ip = i;
			}

		if (p - 1.e-10 <= 0)
			return;

		if (p - 1.e-10 > 0)
		{
			for (j = nn; j <= LPC; j++)
			{
				w = a[nn][j];
				a[nn][j] = a[ip][j];
				a[ip][j] = w;
			}

			for (j = nnpls1; j <= LPC; j++)
				a[nn][j] = a[nn][j] / a[nn][nn];

			for (i = 0; i < LPC; i++)
				if ((i - nn) != 0)
					for (j = nnpls1; j <= LPC; j++)
						a[i][j] = a[i][j] - a[i][nn] * a[nn][j];
		}
    }
}


/**************************************************************************
  Title:	w_quantize

  Purpose:	Quantizes LTP gain(s).

  Usage:	w_quantize(freq, ltp_idx)

  Input:	freq    - original LTP gain(s)

  Output:	freq    - LTP gain(s) selected from the codebook
                ltp_idx - corresponding indices of the LTP gain(s)

  References:	-

  Explanation:	The closest value from the codebook is selected to be the
                quantized LTP gain.

  Author(s):	Juha Ojanpera, Lin Yin
  *************************************************************************/

void
w_quantize (double *freq, int *ltp_idx)
{
	int i, j;
	double dist, low;


	low = 1.0e+10;
	for (i = 0; i < LPC; i++)
    {
//		dist = 0.0;
		for (j = 0; j < CODESIZE; j++)
		{
			dist = (freq[i] - codebook[j]) * (freq[i] - codebook[j]);
			if (dist < low)
			{
				low = dist;
				ltp_idx[i] = j;
			}
		}
    }

	for (j = 0; j < LPC; j++)
		freq[j] = codebook[ltp_idx[j]];
}


/**************************************************************************
  Title:	init_lt_pred

  Purpose:	Initialize the history buffer for long term prediction

  Usage:        init_lt_pred (lt_status)

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
init_lt_pred(LT_PRED_STATUS *lt_status)
{
	int i;

	for (i = 0; i < LT_BLEN; i++)
		lt_status->buffer[i] = 0;

	for (i = 0; i < BLOCK_LEN_LONG; i++)
		lt_status->pred_mdct[i] = 0;

	lt_status->weight_idx = 0;
	for(i=0; i < MAX_SHORT_WINDOWS; i++)
		lt_status->sbk_prediction_used[i] = lt_status->delay[i] = 0;

	for(i=0; i<MAX_SCFAC_BANDS; i++)
		lt_status->sfb_prediction_used[i] = 0;

	lt_status->side_info = LEN_LTP_DATA_PRESENT;
}


/**************************************************************************
  Title:	ltp_enc

  Purpose:      Performs long term prediction.

  Usage:	ltp_enc(p_spectrum, p_time_signal, win_type, win_shape, 
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
                        
  References:	1.) estimate_delay in pitch.c
                2.) pitch in pitch.c
                3.) buffer2freq
                4.) snr_pred in pitch.c
                5.) freq2buffer
                6.) double_to_int

  Explanation:  -

  Author(s):	Juha Ojanpera
  *************************************************************************/

int
ltp_enc(double *p_spectrum, double *p_time_signal, enum WINDOW_TYPE win_type,
            Window_shape win_shape, int *sfb_offset, int num_of_sfb,
            LT_PRED_STATUS *lt_status)
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
		last_band = (num_of_sfb < MAX_LT_PRED_LONG_SFB) ? num_of_sfb : MAX_LT_PRED_LONG_SFB;

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
  Title:	ltp_reconstruct

  Purpose:      Updates LTP history buffer.

  Usage:	ltp_reconstruct(p_spectrum, win_type,  win_shape, 
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
ltp_reconstruct(double *p_spectrum, enum WINDOW_TYPE win_type, 
                Window_shape win_shape, int *sfb_offset, int num_of_sfb,
                LT_PRED_STATUS *lt_status)
{
	int i, j, last_band;
	double predicted_samples[2 * BLOCK_LEN_LONG];
	double overlap_buffer[2 * BLOCK_LEN_LONG];

    
	switch(win_type)
	{
	case ONLY_LONG_WINDOW:
	case LONG_SHORT_WINDOW:
	case SHORT_LONG_WINDOW:
		last_band = (num_of_sfb < MAX_LT_PRED_LONG_SFB) ? num_of_sfb : MAX_LT_PRED_LONG_SFB;

		if(lt_status->global_pred_flag)
			for (i = 0; i < sfb_offset[last_band]; i++)
				p_spectrum[i] += lt_status->pred_mdct[i];

		/* Finally update the time domain history buffer. */
		freq2buffer (p_spectrum, predicted_samples, overlap_buffer, win_type, WS_SIN, WS_SIN, MNON_OVERLAPPED);

		for (i = 0; i < LT_BLEN - BLOCK_LEN_LONG; i++)
			lt_status->buffer[i] = lt_status->buffer[i + BLOCK_LEN_LONG];

		j = LT_BLEN - 2 * BLOCK_LEN_LONG;
		for (i = 0; i < BLOCK_LEN_LONG; i++)
		{
			lt_status->buffer[i + j] =
				(short)double_to_int (predicted_samples[i] + lt_status->buffer[i + j]);
			lt_status->buffer[LT_BLEN - BLOCK_LEN_LONG + i] =
				(short)double_to_int (predicted_samples[i + BLOCK_LEN_LONG]);
		}
		break;

    case ONLY_SHORT_WINDOW:
#if 0
		for (i = 0; i < LT_BLEN - block_size_long; i++)
			lt_status->buffer[i] = lt_status->buffer[i + block_size_long];

		for (i = LT_BLEN - block_size_long; i < LT_BLEN; i++)
			lt_status->buffer[i] = 0;

		for (i = 0; i < block_size_long; i++)
			overlap_buffer[i] = 0;

		/* Finally update the time domain history buffer. */
		freq2buffer (p_spectrum, predicted_samples, overlap_buffer, win_type, block_size_long,
			block_size_medium, block_size_short, win_shape, MNON_OVERLAPPED);

		for(sw = 0; sw < MAX_SHORT_WINDOWS; sw++)
		{
			i = LT_BLEN - 2 * block_size_long + SHORT_SQ_OFFSET + sw * block_size_short;
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
  Title:	ltp_encode

  Purpose:      Writes LTP parameters to the bit stream.

  Usage:	ltp_encode (bs, win_type, num_of_sfb, lt_status)

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
ltp_encode (AACQuantInfo *quantInfo, BsBitStream *bs, int write_flag)
{
	int i, last_band;
//	int first_subblock;
//	int prev_subblock;
	int bit_count = 0;


	bit_count += 1;
	
	if (quantInfo->ltpInfo.side_info > 1)
	{
		if(write_flag)
			BsPutBit (bs, 1, 1);    	/* LTP used */

		switch(quantInfo->block_type)
		{
		case ONLY_LONG_WINDOW:
		case LONG_SHORT_WINDOW:
		case SHORT_LONG_WINDOW:
			bit_count += LEN_LTP_LAG;
			bit_count += LEN_LTP_COEF;
			if(write_flag)
			{
				BsPutBit (bs, quantInfo->ltpInfo.delay[0], LEN_LTP_LAG);
				BsPutBit (bs, quantInfo->ltpInfo.weight_idx,  LEN_LTP_COEF);
			}

			last_band = (quantInfo->nr_of_sfb < MAX_LT_PRED_LONG_SFB) ? quantInfo->nr_of_sfb : MAX_LT_PRED_LONG_SFB;
			bit_count += last_band;
			if(write_flag)
			{
				for (i = 0; i < last_band; i++)
					BsPutBit (bs, quantInfo->ltpInfo.sfb_prediction_used[i], LEN_LTP_LONG_USED);
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
								BsPutBit (bs, diff + LTP_LAG_OFFSET, LEN_LTP_SHORT_LAG);
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
			//        CommonExit(1, "ltp_encode : unsupported window sequence %i", win_type);
			break;
		}
	}
	else
		if(write_flag)
			BsPutBit (bs, 0, 1);    	/* LTP not used */

	return (bit_count);
}
