/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: ltp.c,v 1.3 2001/03/12 16:58:37 menno Exp $
 */

#include <stdio.h>
#include <math.h>

#include "frame.h"
#include "coder.h"
#include "ltp.h"
#include "tns.h"
#include "filtbank.h"
#include "util.h"

/* short double_to_int(double sig_in); */
#define double_to_int(sig_in) \
   ((sig_in) > 32767 ? 32767 : ( \
	   (sig_in) < -32768 ? -32768 : (sig_in)))


/*  Purpose:	Codebook for LTP weight coefficients.  */
static double codebook[CODESIZE] =
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


static double snr_pred(double *mdct_in, double *mdct_pred, int *sfb_flag, int *sfb_offset,
				int block_type, int side_info, int num_of_sfb)
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
    } else {
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

		if (temp1 > 1.e-20)
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
    } else {
		num_bit -= side_info;
	}

	return (num_bit);
}

static void prediction(short *buffer, double *predicted_samples, double *weight, int lag,
				int flen)
{
	int i, offset;
	int num_samples;

	offset = NOK_LT_BLEN - flen / 2 - lag;

	num_samples = flen;
	if(NOK_LT_BLEN - offset < flen)
		num_samples = NOK_LT_BLEN - offset;
	
	for(i = 0; i < num_samples; i++)
		predicted_samples[i] = *weight * buffer[offset++];
	for( ; i < flen; i++)
		predicted_samples[i] = 0.0;
}

static void w_quantize(double *freq, int *ltp_idx)
{
	int i;
	double dist, low;

	low = 1.0e+10;
	dist = 0.0;
	for (i = 0; i < CODESIZE; i++)
	{
		dist = (*freq - codebook[i]) * (*freq - codebook[i]);
		if (dist < low)
		{
			low = dist;
			*ltp_idx = i;
		}
	}

	*freq = codebook[*ltp_idx];
}

static int pitch(double *sb_samples, short *x_buffer, int flen, int lag0, int lag1, 
		  double *predicted_samples, double *gain, int *cb_idx)
{
	int i, j, delay;
	double corr1, corr2, lag_corr, corrtmp;
	double p_max, energy, lag_energy;

	/*
	 * Below is a figure illustrating how the lag and the
	 * samples in the buffer relate to each other.
	 *
	 * ------------------------------------------------------------------
	 * |              |               |                |                 |
	 * |    slot 1    |      2        |       3        |       4         |
	 * |              |               |                |                 |
	 * ------------------------------------------------------------------
	 *
	 * lag = 0 refers to the end of slot 4 and lag = DELAY refers to the end
	 * of slot 2. The start of the predicted frame is then obtained by
	 * adding the length of the frame to the lag. Remember that slot 4 doesn't
	 * actually exist, since it is always filled with zeros.
	 *
	 * The above short explanation was for long blocks. For short blocks the
	 * zero lag doesn't refer to the end of slot 4 but to the start of slot
	 * 4 - the frame length of a short block.
	 *
	 * Some extra code is then needed to handle those lag values that refer
	 * to slot 4.
	 */

	p_max = 0.0;
	lag_corr = lag_energy = 0.0;
	delay = lag0;

	energy = 0.0;
	corr1 = 0.0;
	for (j = lag0; j < lag1; j++)
	{
		corr1 += x_buffer[NOK_LT_BLEN - j - 1] * sb_samples[flen - j - 1];
		energy += x_buffer[NOK_LT_BLEN - j - 1] * x_buffer[NOK_LT_BLEN - j - 1];
	}
	corrtmp=corr1;
	if (energy != 0.0)
		corr2 = corr1 / sqrt(energy);
	else
		corr2 = 0.0;

	if (p_max < corr2)
	{
		p_max = corr2;
		delay = 0;
		lag_corr = corr1;
		lag_energy = energy;
	}

	/* Find the lag. */
	for (i = lag0 + 1; i < lag1; i++)
	{
		energy -= x_buffer[NOK_LT_BLEN - i] * x_buffer[NOK_LT_BLEN - i];
		energy += x_buffer[NOK_LT_BLEN - i - flen] * x_buffer[NOK_LT_BLEN - i - flen];
		corr1 = corrtmp;
		corr1 -= x_buffer[NOK_LT_BLEN - i] * sb_samples[flen - 1];
		corr1 += x_buffer[NOK_LT_BLEN - i - flen] * sb_samples[0];
		corrtmp = corr1;

		if (energy != 0.0)
			corr2 = corr1 / sqrt(energy);
		else
			corr2 = 0.0;

		if (p_max < corr2)
		{
			p_max = corr2;
			delay = i;
			lag_corr = corr1;
			lag_energy = energy;
		}
	}

	/* Compute the gain. */
	if(lag_energy != 0.0)
		*gain =  lag_corr / (1.010 * lag_energy);
	else
		*gain = 0.0;

	/* Quantize the gain. */
	w_quantize(gain, cb_idx);

	/* Get the predicted signal. */
	prediction(x_buffer, predicted_samples, gain, delay, flen);

	return (delay);
}

static double ltp_enc_tf(faacEncHandle hEncoder,
				CoderInfo *coderInfo, double *p_spectrum, double *predicted_samples, 
						 double *mdct_predicted, int *sfb_offset,
						 int num_of_sfb, int last_band, int side_info, 
						 int *sfb_prediction_used, TnsInfo *tnsInfo)
{
	double bit_gain;

	/* Transform prediction to frequency domain. */
	FilterBank(hEncoder, coderInfo, predicted_samples, mdct_predicted,
		NULL, MNON_OVERLAPPED);

	/* Apply TNS analysis filter to the predicted spectrum. */
	if(tnsInfo != NULL)
		TnsEncodeFilterOnly(tnsInfo, num_of_sfb, num_of_sfb, coderInfo->block_type, sfb_offset, 
		mdct_predicted);

	/* Get the prediction gain. */
	bit_gain = snr_pred(p_spectrum, mdct_predicted, sfb_prediction_used, 
		sfb_offset, side_info, last_band, coderInfo->nr_of_sfb);

	return (bit_gain);
}

void LtpInit(faacEncHandle hEncoder)
{
	int i;
	unsigned int channel;

	for (channel = 0; channel < hEncoder->numChannels; channel++) {
		LtpInfo *ltpInfo = &(hEncoder->coderInfo[channel].ltpInfo);

		ltpInfo->buffer = AllocMemory(NOK_LT_BLEN * sizeof(short));
		ltpInfo->mdct_predicted = AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));
		ltpInfo->time_buffer = AllocMemory(BLOCK_LEN_LONG*sizeof(double));
		ltpInfo->ltp_overlap_buffer = AllocMemory(BLOCK_LEN_LONG*sizeof(double));

		for (i = 0; i < NOK_LT_BLEN; i++)
			ltpInfo->buffer[i] = 0;

		ltpInfo->weight_idx = 0;
		for(i = 0; i < MAX_SHORT_WINDOWS; i++)
			ltpInfo->sbk_prediction_used[i] = ltpInfo->delay[i] = 0;

		for(i = 0; i < MAX_SCFAC_BANDS; i++)
			ltpInfo->sfb_prediction_used[i] = 0;

		ltpInfo->side_info = LEN_LTP_DATA_PRESENT;

		for(i = 0; i < 2 * BLOCK_LEN_LONG; i++)
			ltpInfo->mdct_predicted[i] = 0.0;
	}
}

void LtpEnd(faacEncHandle hEncoder)
{
	unsigned int channel;

	for (channel = 0; channel < hEncoder->numChannels; channel++) {
		LtpInfo *ltpInfo = &(hEncoder->coderInfo[channel].ltpInfo);

		if (ltpInfo->buffer) FreeMemory(ltpInfo->buffer);
		if (ltpInfo->mdct_predicted) FreeMemory(ltpInfo->mdct_predicted);
	}
}

int LtpEncode(faacEncHandle hEncoder,
				CoderInfo *coderInfo,
				LtpInfo *ltpInfo,
				TnsInfo *tnsInfo,
				double *p_spectrum,
				double *p_time_signal)
{
	int i, last_band;
	double num_bit[MAX_SHORT_WINDOWS];
	double *predicted_samples;

	ltpInfo->global_pred_flag = 0;
	ltpInfo->side_info = 0;

	predicted_samples = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));

	switch(coderInfo->block_type)
	{
	case ONLY_LONG_WINDOW:
	case LONG_SHORT_WINDOW:
	case SHORT_LONG_WINDOW:
		last_band = (coderInfo->nr_of_sfb < MAX_LT_PRED_LONG_SFB) ? coderInfo->nr_of_sfb : MAX_LT_PRED_LONG_SFB;

		ltpInfo->delay[0] = 
			pitch(p_time_signal, ltpInfo->buffer, 2 * BLOCK_LEN_LONG, 
				0, 2 * BLOCK_LEN_LONG, predicted_samples, &ltpInfo->weight, 
				&ltpInfo->weight_idx);

		num_bit[0] = 
			ltp_enc_tf(hEncoder, coderInfo, p_spectrum, predicted_samples,
				ltpInfo->mdct_predicted, 
				coderInfo->sfb_offset, coderInfo->nr_of_sfb,
				last_band, ltpInfo->side_info, ltpInfo->sfb_prediction_used, 
				tnsInfo);

		ltpInfo->global_pred_flag = (num_bit[0] == 0.0) ? 0 : 1;

		if(ltpInfo->global_pred_flag)
			for (i = 0; i < coderInfo->sfb_offset[last_band]; i++)
				p_spectrum[i] -= ltpInfo->mdct_predicted[i];
			else
				ltpInfo->side_info = 1;
			
			break;
			
    default:
		break;
	}

	if (predicted_samples) FreeMemory(predicted_samples);

	return (ltpInfo->global_pred_flag);
}

void LtpReconstruct(CoderInfo *coderInfo, LtpInfo *ltpInfo, double *p_spectrum)
{
	int i, last_band;

	if(ltpInfo->global_pred_flag)
	{
		switch(coderInfo->block_type)
		{
		case ONLY_LONG_WINDOW:
		case LONG_SHORT_WINDOW:
		case SHORT_LONG_WINDOW:
			last_band = (coderInfo->nr_of_sfb < MAX_LT_PRED_LONG_SFB) ?
				coderInfo->nr_of_sfb : MAX_LT_PRED_LONG_SFB;

			for (i = 0; i < coderInfo->sfb_offset[last_band]; i++)
				p_spectrum[i] += ltpInfo->mdct_predicted[i];
			break;

		default:
			break;
		}
	}
}

void  LtpUpdate(LtpInfo *ltpInfo, double *time_signal,
					 double *overlap_signal, int block_size_long)
{
	int i;

	for(i = 0; i < NOK_LT_BLEN - 2 * block_size_long; i++)
		ltpInfo->buffer[i] = ltpInfo->buffer[i + block_size_long];

	for(i = 0; i < block_size_long; i++) 
	{
		ltpInfo->buffer[NOK_LT_BLEN - 2 * block_size_long + i] = 
			(short)double_to_int(time_signal[i]);

		ltpInfo->buffer[NOK_LT_BLEN - block_size_long + i] = 
			(short)double_to_int(overlap_signal[i]);
	}
}
