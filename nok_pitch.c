/**************************************************************************

This software module was originally developed by
Nokia in the course of development of the MPEG-2 AAC/MPEG-4 
Audio standard ISO/IEC13818-7, 14496-1, 2 and 3.
This software module is an implementation of a part
of one or more MPEG-2 AAC/MPEG-4 Audio tools as specified by the
MPEG-2 aac/MPEG-4 Audio standard. ISO/IEC  gives users of the
MPEG-2aac/MPEG-4 Audio standards free license to this software module
or modifications thereof for use in hardware or software products
claiming conformance to the MPEG-2 aac/MPEG-4 Audio  standards. Those
intending to use this software module in hardware or software products
are advised that this use may infringe existing patents. The original
developer of this software module, the subsequent
editors and their companies, and ISO/IEC have no liability for use of
this software module or modifications thereof in an
implementation. Copyright is not released for non MPEG-2 aac/MPEG-4
Audio conforming products. The original developer retains full right to
use the code for the developer's own purpose, assign or donate the code to a
third party and to inhibit third party from using the code for non
MPEG-2 aac/MPEG-4 Audio conforming products. This copyright notice
must be included in all copies or derivative works.
Copyright (c)1997.  

***************************************************************************/
/**************************************************************************
  nok_pitch.c  - Long Term Prediction (LTP) subroutines.
 
  Author(s): Juha Ojanpera, Lin Yin
  	     Nokia Research Center, Speech and Audio Systems.
  *************************************************************************/


/**************************************************************************
  Version Control Information			Method: CVS
  Identifiers:
  $Revision: 1.3 $
  $Date: 2000/01/06 13:55:21 $ (check in)
  $Author: menno $
  *************************************************************************/


/**************************************************************************
  External Objects Needed
  *************************************************************************/
/*
  Standard library declarations.  */
#include <math.h>
#include <stdio.h>

/*
  Interface to related modules.  */
#include "tf_main.h"
#include "block.h"
#include "interface.h"
#include "nok_ltp_common.h"
#include "nok_ltp_enc.h"


/*************************************************************************
  External Objects Provided
  *************************************************************************/
#include "nok_pitch.h"



/**************************************************************************
  Internal Objects
  *************************************************************************/
#include "nok_ltp_common_internal.h"

static void lnqgj (double (*a)[LPC + 1]);

static void w_quantize (double *freq, int *ltp_idx);

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
      offset = NOK_LT_BLEN - flen - delay + i - (LPC - 1) / 2;
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
					short *x_buffer,
					int flen
					)
{
	int i, j;
	int delay;
	double corr[DELAY];
	double p_max, energy;

	for (i = 0; i < DELAY; i+=16)
	{
		energy = 0.0;
		corr[i] = 0.0;
		for (j = 0; j < flen; j++)
		{
			corr[i] += x_buffer[NOK_LT_BLEN - i - j - 1] * sb_samples[flen - j - 1];
			energy += x_buffer[NOK_LT_BLEN - i - j - 1] * x_buffer[NOK_LT_BLEN - i - j - 1];
		}

		if (energy != 0.0)
			corr[i] = corr[i] / sqrt (energy);
		else
			corr[i] = 0.0;
    }

	p_max = 0.0;
	delay = 0;
	for (i = 0; i < DELAY; i+=16)
		if (p_max < corr[i])
		{
			p_max = corr[i];
			delay = i;
		}

	if (delay < (LPC - 1) / 2)
		delay = (LPC - 1) / 2;

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
			offset1 = NOK_LT_BLEN - flen - delay + i - (LPC - 1) / 2;
			offset2 = NOK_LT_BLEN - flen - delay + j - (LPC - 1) / 2;
			r[i][j] = 0.0;
			for (k = 0; k < flen; k++)
				r[i][j] += x_buffer[offset1 + k] * x_buffer[offset2 + k];
		}
	for (i = 0; i < LPC; i++)
		r[i][i] = 1.010 * r[i][i];

	for (i = 0; i < LPC; i++)
    {
		offset1 = NOK_LT_BLEN - flen - delay + i - (LPC - 1) / 2; 
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
		dist = 0.0;
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

