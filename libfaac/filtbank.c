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
 * $Id: filtbank.c,v 1.2 2001/01/17 15:51:15 menno Exp $
 */

/*
 * CHANGES:
 *  2001/01/17: menno: Added frequency cut off filter.
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "coder.h"
#include "filtbank.h"
#include "frame.h"
#include "fft.h"

#include "kbd_win.h"

#define  TWOPI       6.28318530717958647692

void FilterBankInit(faacEncHandle hEncoder)
{
	unsigned int i, channel;

	for (channel = 0; channel < hEncoder->numChannels; channel++) {
		hEncoder->freqBuff[channel] = (double*)malloc(2*FRAME_LEN*sizeof(double));
		hEncoder->overlapBuff[channel] = (double*)malloc(FRAME_LEN*sizeof(double));
		memset(hEncoder->overlapBuff[channel], 0, FRAME_LEN*sizeof(double));
	}

	hEncoder->sin_window_long = (double*)malloc(BLOCK_LEN_LONG*sizeof(double));
	hEncoder->sin_window_short = (double*)malloc(BLOCK_LEN_SHORT*sizeof(double));
	hEncoder->kbd_window_long = kbd_window_long;
	hEncoder->kbd_window_short = kbd_window_short;

	for( i=0; i<BLOCK_LEN_LONG; i++ )
		hEncoder->sin_window_long[i] = sin((M_PI/(2*BLOCK_LEN_LONG)) * (i + 0.5));
	for( i=0; i<BLOCK_LEN_SHORT; i++ )
		hEncoder->sin_window_short[i] = sin((M_PI/(2*BLOCK_LEN_SHORT)) * (i + 0.5));
}

void FilterBankEnd(faacEncHandle hEncoder)
{
	unsigned int channel;

	for (channel = 0; channel < hEncoder->numChannels; channel++) {
		if (hEncoder->freqBuff[channel]) free(hEncoder->freqBuff[channel]);
		if (hEncoder->overlapBuff[channel]) free(hEncoder->overlapBuff[channel]);
	}

	if (hEncoder->sin_window_long) free(hEncoder->sin_window_long);
	if (hEncoder->sin_window_short) free(hEncoder->sin_window_short);
}

void FilterBank(faacEncHandle hEncoder,
				CoderInfo *coderInfo,
				double *p_in_data,
				double *p_out_mdct,
				double *p_overlap)
{
	double *transf_buf;
	double *p_o_buf, *first_window, *second_window;
	int k, i;
	int block_type = coderInfo->block_type;

	transf_buf = (double*)malloc(2*BLOCK_LEN_LONG*sizeof(double));

	/* create / shift old values */
	/* We use p_overlap here as buffer holding the last frame time signal*/
	memcpy(transf_buf, p_overlap, FRAME_LEN*sizeof(double));
	memcpy(transf_buf+BLOCK_LEN_LONG, p_in_data, FRAME_LEN*sizeof(double));
	memcpy(p_overlap, p_in_data, FRAME_LEN*sizeof(double));

	/*  Window shape processing */
	switch (coderInfo->prev_window_shape){
    case SINE_WINDOW:
		if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
			first_window = hEncoder->sin_window_long;
		else
			first_window = hEncoder->sin_window_short;
		break;
    case KBD_WINDOW:
		if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
			first_window = hEncoder->kbd_window_long;
		else
			first_window = hEncoder->kbd_window_short;
		break;
	}

	switch (coderInfo->window_shape){
    case SINE_WINDOW:
		if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
			second_window = hEncoder->sin_window_long;
		else
			second_window = hEncoder->sin_window_short;
		break;
    case KBD_WINDOW:
		if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
			second_window = hEncoder->kbd_window_long;
		else
			second_window = hEncoder->kbd_window_short;
		break;
	}

	/* Set ptr to transf-Buffer */
	p_o_buf = transf_buf;

	/* Separate action for each Block Type */
	switch( block_type ) {
    case ONLY_LONG_WINDOW :
		for ( i = 0 ; i < BLOCK_LEN_LONG ; i++){
			p_out_mdct[i] = p_o_buf[i] * first_window[i];
			p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
		}
		MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
		break;

    case LONG_SHORT_WINDOW :
		for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
			p_out_mdct[i] = p_o_buf[i] * first_window[i];
		memcpy(p_out_mdct+BLOCK_LEN_LONG,p_o_buf+BLOCK_LEN_LONG,NFLAT_LS*sizeof(double));
		for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
			p_out_mdct[i+BLOCK_LEN_LONG+NFLAT_LS] = p_o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
		memset(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(double));
		MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
		break;

    case SHORT_LONG_WINDOW :
		memset(p_out_mdct,0,NFLAT_LS*sizeof(double));
		for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
			p_out_mdct[i+NFLAT_LS] = p_o_buf[i+NFLAT_LS] * first_window[i];
		memcpy(p_out_mdct+NFLAT_LS+BLOCK_LEN_SHORT,p_o_buf+NFLAT_LS+BLOCK_LEN_SHORT,NFLAT_LS*sizeof(double));
		for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
			p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
		MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
		break;

    case ONLY_SHORT_WINDOW :
		p_o_buf += NFLAT_LS;
		for ( k=0; k < MAX_SHORT_WINDOWS; k++ ) {
			for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++ ){
				p_out_mdct[i] = p_o_buf[i] * first_window[i];
				p_out_mdct[i+BLOCK_LEN_SHORT] = p_o_buf[i+BLOCK_LEN_SHORT] * second_window[BLOCK_LEN_SHORT-i-1];
			}
			MDCT( p_out_mdct, 2*BLOCK_LEN_SHORT );
			p_out_mdct += BLOCK_LEN_SHORT;
			p_o_buf += BLOCK_LEN_SHORT;
			first_window = second_window;
		}
		break;
	}

	if (transf_buf) free(transf_buf);
}

void specFilter(double *freqBuff,
				int sampleRate,
				int lowpassFreq,
				int specLen
				)
{
	int lowpass,xlowpass;

	/* calculate the last line which is not zero */
	lowpass = (lowpassFreq * specLen) / (sampleRate>>1) + 1;
	xlowpass = (lowpass < specLen) ? lowpass : specLen ;

	memset(freqBuff+xlowpass,0,(specLen-xlowpass)*sizeof(double));
}

static void MDCT(double *data, int N)
{
	double *xi, *xr;
	double tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
	double freq = TWOPI / N;
	double cosfreq8, sinfreq8;
	int i, n;

	xi = (double*)malloc((N >> 2)*sizeof(double));
	xr = (double*)malloc((N >> 2)*sizeof(double));

	/* prepare for recurrence relation in pre-twiddle */
	cfreq = cos (freq);
	sfreq = sin (freq);
	cosfreq8 = cos (freq * 0.125);
	sinfreq8 = sin (freq * 0.125);
	c = cosfreq8;
	s = sinfreq8;

	for (i = 0; i < (N >> 2); i++) {
		/* calculate real and imaginary parts of g(n) or G(p) */
		n = (N >> 1) - 1 - 2 * i;

		if (i < (N >> 3))
			tempr = data [(N >> 2) + n] + data [N + (N >> 2) - 1 - n]; /* use second form of e(n) for n = N / 2 - 1 - 2i */
		else
			tempr = data [(N >> 2) + n] - data [(N >> 2) - 1 - n]; /* use first form of e(n) for n = N / 2 - 1 - 2i */

		n = 2 * i;
		if (i < (N >> 3))
			tempi = data [(N >> 2) + n] - data [(N >> 2) - 1 - n]; /* use first form of e(n) for n=2i */
		else
			tempi = data [(N >> 2) + n] + data [N + (N >> 2) - 1 - n]; /* use second form of e(n) for n=2i*/

		/* calculate pre-twiddled FFT input */
		xr[i] = tempr * c + tempi * s;
		xi[i] = tempi * c - tempr * s;

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
	}

	/* Perform in-place complex FFT of length N/4 */
	switch (N) {
    case 256:
		srfft(xr, xi, 6);
		break;
    case 2048:
		srfft(xr, xi, 9);
	}

	/* prepare for recurrence relations in post-twiddle */
	c = cosfreq8;
	s = sinfreq8;

	/* post-twiddle FFT output and then get output data */
	for (i = 0; i < (N >> 2); i++) {
		/* get post-twiddled FFT output  */
		tempr = 2. * (xr[i] * c + xi[i] * s);
		tempi = 2. * (xi[i] * c - xr[i] * s);

		/* fill in output values */
		data [2 * i] = -tempr;   /* first half even */
		data [(N >> 1) - 1 - 2 * i] = tempi;  /* first half odd */
		data [(N >> 1) + 2 * i] = -tempi;  /* second half even */
		data [N - 1 - 2 * i] = tempr;  /* second half odd */

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
	}

	if (xr) free(xr);
	if (xi) free(xi);
}
