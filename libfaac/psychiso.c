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
 * $Id: psychiso.c,v 1.3 2002/10/08 18:53:02 menno Exp $
 */

#include <stdlib.h>
#include <memory.h>
#include <math.h>

#if defined(_DEBUG)

#include <stdio.h>

#endif

#include "psych.h"
#include "coder.h"
#include "fft.h"
#include "util.h"


typedef struct {
	/* FFT data */

	/* Magnitude */
	double *fftMagPlus2;
	double *fftMagPlus1;
	double *fftMag;
	double *fftMagMin1;
	double *fftMagMin2;

	double *fftMagPlus2S[8];
	double *fftMagPlus1S[8];
	double *fftMagS[8];
	double *fftMagMin1S[8];

	/* Phase */
	double *fftPhPlus2;
	double *fftPhPlus1;
	double *fftPh;
	double *fftPhMin1;
	double *fftPhMin2;

	double *fftPhPlus2S[8];
	double *fftPhPlus1S[8];
	double *fftPhS[8];
	double *fftPhMin1S[8];

	/* Unpredictability */
	double *cw;
	double *cwS[8];

	double lastPe;
	double lastEnr;
	int threeInARow;

	/* Final threshold values */
	double *maskThrNext;
	double *maskEnNext;
	double *maskThrNextS[8];
	double *maskEnNextS[8];

	double *lastNb;
	double *lastNbMS;

	double *maskThrNextMS;
	double *maskEnNextMS;
	double *maskThrNextSMS[8];
	double *maskEnNextSMS[8];
} psydata_t;

typedef struct {
	/* Stereo demasking thresholds */
	double *mld;
	double *mldS;

	/* Spreading functions */
	double spreading[MAX_SCFAC_BANDS][MAX_SCFAC_BANDS];
	double spreadingS[MAX_SCFAC_BANDS][MAX_SCFAC_BANDS];
	double *rnorm;
	double *rnormS;

	/* Absolute threshold of hearing */
	double *ath;
	double *athS;
} gpsydata_t;


static void Hann(GlobalPsyInfo *gpsyInfo, double *inSamples, int N);
static void PsyUnpredictability(PsyInfo *psyInfo);
static void PsyThreshold(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, int *cb_width_long,
						 int num_cb_long, int *cb_width_short, int num_cb_short);
static void PsyThresholdMS(ChannelInfo *channelInfoL, GlobalPsyInfo *gpsyInfo,
						   PsyInfo *psyInfoL, PsyInfo *psyInfoR, int *cb_width_long,
						   int num_cb_long, int *cb_width_short, int num_cb_short);
static double freq2bark(double freq);
static double ATHformula(double f);

static void PsyInit(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels,
			 unsigned int sampleRate, int *cb_width_long, int num_cb_long,
			 int *cb_width_short, int num_cb_short)
{
	unsigned int channel;
	int i, j, b, bb, high, low, size;
	double tmpx,tmpy,tmp,x,b1,b2;
	double bval[MAX_SCFAC_BANDS];
	gpsydata_t *gpsydata;

	gpsyInfo->data = AllocMemory(sizeof(gpsydata_t));
        gpsydata = gpsyInfo->data;

	gpsydata->ath = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsydata->athS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsydata->rnorm = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsydata->rnormS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsydata->mld = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsydata->mldS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->hannWindow = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));
	gpsyInfo->hannWindowS = (double*)AllocMemory(2*BLOCK_LEN_SHORT*sizeof(double));

	for(i = 0; i < BLOCK_LEN_LONG*2; i++)
		gpsyInfo->hannWindow[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_LONG*2)));
	for(i = 0; i < BLOCK_LEN_SHORT*2; i++)
		gpsyInfo->hannWindowS[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_SHORT*2)));
	gpsyInfo->sampleRate = (double)sampleRate;

	for (channel = 0; channel < numChannels; channel++)
	{
	  psydata_t *psydata = AllocMemory(sizeof(psydata_t));
	  psyInfo[channel].data = psydata;
	}
	size = BLOCK_LEN_LONG;
	for (channel = 0; channel < numChannels; channel++) {
		psydata_t *psydata = psyInfo[channel].data;

		psyInfo[channel].size = size;

		psydata->lastPe = 0.0;
		psydata->lastEnr = 0.0;
		psydata->threeInARow = 0;
		psydata->cw = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].maskThr = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEn = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psydata->maskThrNext = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psydata->maskEnNext = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psydata->maskThrNextMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psydata->maskEnNextMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].prevSamples = (double*)AllocMemory(size*sizeof(double));
		memset(psyInfo[channel].prevSamples, 0, size*sizeof(double));

		psydata->lastNb = (double*)AllocMemory(size*sizeof(double));
		psydata->lastNbMS = (double*)AllocMemory(size*sizeof(double));
		for (j = 0; j < size; j++) {
			psydata->lastNb[j] = 2.;
			psydata->lastNbMS[j] = 2.;
		}

		psydata->fftMagPlus2 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftMagPlus1 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftMag = (double*)AllocMemory(size*sizeof(double));
		psydata->fftMagMin1 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftMagMin2 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftPhPlus2 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftPhPlus1 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftPh = (double*)AllocMemory(size*sizeof(double));
		psydata->fftPhMin1 = (double*)AllocMemory(size*sizeof(double));
		psydata->fftPhMin2 = (double*)AllocMemory(size*sizeof(double));
	}

	size = BLOCK_LEN_SHORT;
	for (channel = 0; channel < numChannels; channel++) {
		psydata_t *psydata = psyInfo[channel].data;

		psyInfo[channel].sizeS = size;

		psyInfo[channel].prevSamplesS = (double*)AllocMemory(size*sizeof(double));
		memset(psyInfo[channel].prevSamplesS, 0, size*sizeof(double));

		for (j = 0; j < 8; j++) {
			psydata->cwS[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].maskThrS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psydata->maskThrNextS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psydata->maskEnNextS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psydata->maskThrNextSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psydata->maskEnNextSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));

			psydata->fftMagPlus2S[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftMagPlus1S[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftMagS[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftMagMin1S[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftPhPlus2S[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftPhPlus1S[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftPhS[j] = (double*)AllocMemory(size*sizeof(double));
			psydata->fftPhMin1S[j] = (double*)AllocMemory(size*sizeof(double));
		}
	}

	size = BLOCK_LEN_LONG;
	high = 0;
	for(b = 0; b < num_cb_long; b++) {
		low = high;
		high += cb_width_long[b];

		bval[b] = 0.5 * (freq2bark(gpsyInfo->sampleRate*low/(2*size)) + 
			freq2bark(gpsyInfo->sampleRate*(high-1)/(2*size)));
	}

	for(b = 0; b < num_cb_long; b++) {
		b2 = bval[b];
		for(bb = 0; bb < num_cb_long; bb++) {
			b1 = bval[bb];

			if (b>=bb) tmpx = (b2 - b1)*3.0;
			else tmpx = (b2 - b1)*1.5;

			if(tmpx>=0.5 && tmpx<=2.5)
			{
				tmp = tmpx - 0.5;
				x = 8.0 * (tmp*tmp - 2.0 * tmp);
			}
			else x = 0.0;
			tmpx += 0.474;
			tmpy = 15.811389 + 7.5*tmpx - 17.5*sqrt(1.0+tmpx*tmpx);

			if (tmpy <= -100.0) gpsydata->spreading[b][bb] = 0.0;
			else gpsydata->spreading[b][bb] = exp((x + tmpy)*0.2302585093);
		}
	}

    for( b = 0; b < num_cb_long; b++){
		tmp = 0.0;
		for( bb = 0; bb < num_cb_long; bb++)
			tmp += gpsydata->spreading[bb][b];
		gpsydata->rnorm[b] = 1.0/tmp;
    }

	j = 0;
    for( b = 0; b < num_cb_long; b++){
		gpsydata->ath[b] = 1.e37;

		for (bb = 0; bb < cb_width_long[b]; bb++, j++) {
			double freq = gpsyInfo->sampleRate*j/(1000.0*2*size);
			double level;
			level = ATHformula(freq*1000) - 20;
			level = pow(10., 0.1*level);
			level *= cb_width_long[b];
			if (level < gpsydata->ath[b])
				gpsydata->ath[b] = level;
		}
    }

	low = 0;
	for (b = 0; b < num_cb_long; b++) {
		tmp = freq2bark(gpsyInfo->sampleRate*low/(2*size));
		tmp = (min(tmp, 15.5)/15.5);

		gpsydata->mld[b] = pow(10.0, 1.25*(1-cos(M_PI*tmp))-2.5);
		low += cb_width_long[b];
	}


	size = BLOCK_LEN_SHORT;
	high = 0;
	for(b = 0; b < num_cb_short; b++) {
		low = high;
		high += cb_width_short[b];

		bval[b] = 0.5 * (freq2bark(gpsyInfo->sampleRate*low/(2*size)) + 
			freq2bark(gpsyInfo->sampleRate*(high-1)/(2*size)));
	}

	for(b = 0; b < num_cb_short; b++) {
		b2 = bval[b];
		for(bb = 0; bb < num_cb_short; bb++) {
			b1 = bval[bb];

			if (b>=bb) tmpx = (b2 - b1)*3.0;
			else tmpx = (b2 - b1)*1.5;

			if(tmpx>=0.5 && tmpx<=2.5)
			{
				tmp = tmpx - 0.5;
				x = 8.0 * (tmp*tmp - 2.0 * tmp);
			}
			else x = 0.0;
			tmpx += 0.474;
			tmpy = 15.811389 + 7.5*tmpx - 17.5*sqrt(1.0+tmpx*tmpx);

			if (tmpy <= -100.0) gpsydata->spreadingS[b][bb] = 0.0;
			else gpsydata->spreadingS[b][bb] = exp((x + tmpy)*0.2302585093);
		}
	}

	j = 0;
    for( b = 0; b < num_cb_short; b++){
		gpsydata->athS[b] = 1.e37;

		for (bb = 0; bb < cb_width_short[b]; bb++, j++) {
			double freq = gpsyInfo->sampleRate*j/(1000.0*2*size);
			double level;
			level = ATHformula(freq*1000) - 20;
			level = pow(10., 0.1*level);
			level *= cb_width_short[b];
			if (level < gpsydata->athS[b])
				gpsydata->athS[b] = level;
		}
    }

    for( b = 0; b < num_cb_short; b++){
		tmp = 0.0;
		for( bb = 0; bb < num_cb_short; bb++)
			tmp += gpsydata->spreadingS[bb][b];
		gpsydata->rnormS[b] = 1.0/tmp;
    }

	low = 0;
	for (b = 0; b < num_cb_short; b++) {
		tmp = freq2bark(gpsyInfo->sampleRate*low/(2*size));
		tmp = (min(tmp, 15.5)/15.5);

		gpsydata->mldS[b] = pow(10.0, 1.25*(1-cos(M_PI*tmp))-2.5);
		low += cb_width_short[b];
	}
}

static void PsyEnd(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels)
{
	unsigned int channel;
	int j;
	gpsydata_t *gpsydata = gpsyInfo->data;

	if (gpsydata->ath) FreeMemory(gpsydata->ath);
	if (gpsydata->athS) FreeMemory(gpsydata->athS);
	if (gpsydata->rnorm) FreeMemory(gpsydata->rnorm);
	if (gpsydata->rnormS) FreeMemory(gpsydata->rnormS);
	if (gpsydata->mld) FreeMemory(gpsydata->mld);
	if (gpsydata->mldS) FreeMemory(gpsydata->mldS);
	if (gpsyInfo->hannWindow) FreeMemory(gpsyInfo->hannWindow);
	if (gpsyInfo->hannWindowS) FreeMemory(gpsyInfo->hannWindowS);

	for (channel = 0; channel < numChannels; channel++) {
		psydata_t *psydata = psyInfo[channel].data;

		if (psyInfo[channel].prevSamples) FreeMemory(psyInfo[channel].prevSamples);
		if (psydata->cw) FreeMemory(psydata->cw);
		if (psyInfo[channel].maskThr) FreeMemory(psyInfo[channel].maskThr);
		if (psyInfo[channel].maskEn) FreeMemory(psyInfo[channel].maskEn);
		if (psydata->maskThrNext) FreeMemory(psydata->maskThrNext);
		if (psydata->maskEnNext) FreeMemory(psydata->maskEnNext);
		if (psyInfo[channel].maskThrMS) FreeMemory(psyInfo[channel].maskThrMS);
		if (psyInfo[channel].maskEnMS) FreeMemory(psyInfo[channel].maskEnMS);
		if (psydata->maskThrNextMS) FreeMemory(psydata->maskThrNextMS);
		if (psydata->maskEnNextMS) FreeMemory(psydata->maskEnNextMS);
		
		if (psydata->lastNb) FreeMemory(psydata->lastNb);
		if (psydata->lastNbMS) FreeMemory(psydata->lastNbMS);

		if (psydata->fftMagPlus2) FreeMemory(psydata->fftMagPlus2);
		if (psydata->fftMagPlus1) FreeMemory(psydata->fftMagPlus1);
		if (psydata->fftMag) FreeMemory(psydata->fftMag);
		if (psydata->fftMagMin1) FreeMemory(psydata->fftMagMin1);
		if (psydata->fftMagMin2) FreeMemory(psydata->fftMagMin2);
		if (psydata->fftPhPlus2) FreeMemory(psydata->fftPhPlus2);
		if (psydata->fftPhPlus1) FreeMemory(psydata->fftPhPlus1);
		if (psydata->fftPh) FreeMemory(psydata->fftPh);
		if (psydata->fftPhMin1) FreeMemory(psydata->fftPhMin1);
		if (psydata->fftPhMin2) FreeMemory(psydata->fftPhMin2);
	}

	for (channel = 0; channel < numChannels; channel++) {
		psydata_t *psydata = psyInfo[channel].data;

		if(psyInfo[channel].prevSamplesS) FreeMemory(psyInfo[channel].prevSamplesS);
		for (j = 0; j < 8; j++) {
			if (psydata->cwS[j]) FreeMemory(psydata->cwS[j]);
			if (psyInfo[channel].maskThrS[j]) FreeMemory(psyInfo[channel].maskThrS[j]);
			if (psyInfo[channel].maskEnS[j]) FreeMemory(psyInfo[channel].maskEnS[j]);
			if (psydata->maskThrNextS[j]) FreeMemory(psydata->maskThrNextS[j]);
			if (psydata->maskEnNextS[j]) FreeMemory(psydata->maskEnNextS[j]);
			if (psyInfo[channel].maskThrSMS[j]) FreeMemory(psyInfo[channel].maskThrSMS[j]);
			if (psyInfo[channel].maskEnSMS[j]) FreeMemory(psyInfo[channel].maskEnSMS[j]);
			if (psydata->maskThrNextSMS[j]) FreeMemory(psydata->maskThrNextSMS[j]);
			if (psydata->maskEnNextSMS[j]) FreeMemory(psydata->maskEnNextSMS[j]);

			if (psydata->fftMagPlus2S[j]) FreeMemory(psydata->fftMagPlus2S[j]);
			if (psydata->fftMagPlus1S[j]) FreeMemory(psydata->fftMagPlus1S[j]);
			if (psydata->fftMagS[j]) FreeMemory(psydata->fftMagS[j]);
			if (psydata->fftMagMin1S[j]) FreeMemory(psydata->fftMagMin1S[j]);
			if (psydata->fftPhPlus2S[j]) FreeMemory(psydata->fftPhPlus2S[j]);
			if (psydata->fftPhPlus1S[j]) FreeMemory(psydata->fftPhPlus1S[j]);
			if (psydata->fftPhS[j]) FreeMemory(psydata->fftPhS[j]);
			if (psydata->fftPhMin1S[j]) FreeMemory(psydata->fftPhMin1S[j]);
		}
	}

	for (channel = 0; channel < numChannels; channel++)
	{
	  if (psyInfo[channel].data)
	    FreeMemory(psyInfo[channel].data);
	}

    if (gpsyInfo->data) FreeMemory(gpsyInfo->data);
}

/* Do psychoacoustical analysis */
static void PsyCalculate(ChannelInfo *channelInfo, GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
				  int *cb_width_long, int num_cb_long, int *cb_width_short,
				  int num_cb_short, unsigned int numChannels)
{
	unsigned int channel;

	for (channel = 0; channel < numChannels; channel++) {
		if (channelInfo[channel].present) {

			if (channelInfo[channel].cpe &&
				channelInfo[channel].ch_is_left) { /* CPE */

				int leftChan = channel;
				int rightChan = channelInfo[channel].paired_ch;

				/* Calculate the unpredictability */
				PsyUnpredictability(&psyInfo[leftChan]);
				PsyUnpredictability(&psyInfo[rightChan]);

				/* Calculate the threshold */
				PsyThreshold(gpsyInfo, &psyInfo[leftChan], cb_width_long, num_cb_long,
					cb_width_short, num_cb_short);
				PsyThreshold(gpsyInfo, &psyInfo[rightChan], cb_width_long, num_cb_long,
					cb_width_short, num_cb_short);

				/* And for MS */
				PsyThresholdMS(&channelInfo[leftChan], gpsyInfo, &psyInfo[leftChan],
					&psyInfo[rightChan], cb_width_long, num_cb_long, cb_width_short,
					num_cb_short);

			} else if (!channelInfo[channel].cpe &&
				channelInfo[channel].lfe) { /* LFE */

				/* NOT FINISHED */

			} else if (!channelInfo[channel].cpe) { /* SCE */

				/* Calculate the unpredictability */
				PsyUnpredictability(&psyInfo[channel]);

				/* Calculate the threshold */
				PsyThreshold(gpsyInfo, &psyInfo[channel], cb_width_long, num_cb_long,
					cb_width_short, num_cb_short);
			}
		}
	}
}

static void Hann(GlobalPsyInfo *gpsyInfo, double *inSamples, int size)
{
	int i;

	/* Applying Hann window */
	if (size == BLOCK_LEN_LONG*2) {
		for(i = 0; i < size; i++)
			inSamples[i] *= gpsyInfo->hannWindow[i];
	} else {
		for(i = 0; i < size; i++)
			inSamples[i] *= gpsyInfo->hannWindowS[i];
	}
}

static void PsyBufferUpdate(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
		     double *newSamples, unsigned int bandwidth)
{
	int i, j;
	double a, b;
	double *transBuff, *transBuffS, *tmp;
	psydata_t *psydata = psyInfo->data;

	transBuff = (double*)AllocMemory(2*psyInfo->size*sizeof(double));

	memcpy(transBuff, psyInfo->prevSamples, psyInfo->size*sizeof(double));
	memcpy(transBuff + psyInfo->size, newSamples, psyInfo->size*sizeof(double));


	/* In 2 frames this will be the frequencies where
	   the psychoacoustics are calculated for */
	Hann(gpsyInfo, transBuff, 2*psyInfo->size);
	rfft(transBuff, 11);


	/* shift all buffers 1 frame ahead */
	tmp = psydata->fftMagMin2;
	psydata->fftMagMin2 = psydata->fftMagMin1;
	psydata->fftMagMin1 = psydata->fftMag;
	psydata->fftMag = psydata->fftMagPlus1;
	psydata->fftMagPlus1 = psydata->fftMagPlus2;
	psydata->fftMagPlus2 = tmp;

	tmp = psydata->fftPhMin2;
	psydata->fftPhMin2 = psydata->fftPhMin1;
	psydata->fftPhMin1 = psydata->fftPh;
	psydata->fftPh = psydata->fftPhPlus1;
	psydata->fftPhPlus1 = psydata->fftPhPlus2;
	psydata->fftPhPlus2 = tmp;


	/* Calculate magnitude and phase of new data */
	for (i = 0; i < psyInfo->size; i++) {
		a = transBuff[i];
		b = transBuff[i + psyInfo->size];
		psydata->fftMagPlus2[i] = sqrt(a*a + b*b);

		if(a > 0.0){
			if(b >= 0.0)
				psydata->fftPhPlus2[i] = atan2(b, a);
			else
				psydata->fftPhPlus2[i] = atan2(b, a) + M_PI * 2.0;
		} else if(a < 0.0) {
			psydata->fftPhPlus2[i] = atan2(b, a) + M_PI;
		} else {
			if(b > 0.0)
				psydata->fftPhPlus2[i] = M_PI * 0.5;
			else if( b < 0.0 )
				psydata->fftPhPlus2[i] = M_PI * 1.5;
			else
				psydata->fftPhPlus2[i] = 0.0;
		}
	}

	transBuffS = (double*)AllocMemory(2*psyInfo->sizeS*sizeof(double));

	memcpy(transBuff, psyInfo->prevSamples, psyInfo->size*sizeof(double));
	memcpy(transBuff + psyInfo->size, newSamples, psyInfo->size*sizeof(double));

	for (j = 0; j < 8; j++) {

		memcpy(transBuffS, transBuff+(j*128)+(1024-128), 2*psyInfo->sizeS*sizeof(double));

		/* In 2 frames this will be the frequencies where
		   the psychoacoustics are calculated for */
		Hann(gpsyInfo, transBuffS, 2*psyInfo->sizeS);
		rfft(transBuffS, 8);


		/* shift all buffers 1 frame ahead */
		tmp = psydata->fftMagMin1S[j];
		psydata->fftMagMin1S[j] = psydata->fftMagS[j];
		psydata->fftMagS[j] = psydata->fftMagPlus1S[j];
		psydata->fftMagPlus1S[j] = psydata->fftMagPlus2S[j];
		psydata->fftMagPlus2S[j] = tmp;

		tmp = psydata->fftPhMin1S[j];
		psydata->fftPhMin1S[j] = psydata->fftPhS[j];
		psydata->fftPhS[j] = psydata->fftPhPlus1S[j];
		psydata->fftPhPlus1S[j] = psydata->fftPhPlus2S[j];
		psydata->fftPhPlus2S[j] = tmp;


		/* Calculate magnitude and phase of new data */
		for (i = 0; i < psyInfo->sizeS; i++) {
			a = transBuffS[i];
			b = transBuffS[i + psyInfo->sizeS];
			psydata->fftMagPlus2S[j][i] = sqrt(a*a + b*b);

			if(a > 0.0){
				if(b >= 0.0)
					psydata->fftPhPlus2S[j][i] = atan2(b, a);
				else
					psydata->fftPhPlus2S[j][i] = atan2(b, a) + M_PI * 2.0;
			} else if(a < 0.0) {
				psydata->fftPhPlus2S[j][i] = atan2(b, a) + M_PI;
			} else {
				if(b > 0.0)
					psydata->fftPhPlus2S[j][i] = M_PI * 0.5;
				else if( b < 0.0 )
					psydata->fftPhPlus2S[j][i] = M_PI * 1.5;
				else
					psydata->fftPhPlus2S[j][i] = 0.0;
			}
		}
	}

	memcpy(psyInfo->prevSamples, newSamples, psyInfo->size*sizeof(double));

	if (transBuff) FreeMemory(transBuff);
	if (transBuffS) FreeMemory(transBuffS);
}

static void PsyUnpredictability(PsyInfo *psyInfo)
{
	int i, j;
	double predMagMin, predMagPlus, predMag, mag;
	double predPhMin, predPhPlus, predPh, ph;
	psydata_t *psydata = psyInfo->data;

	for (i = 0; i < psyInfo->size; i++)
	{
		predMagMin = 2.0 * psydata->fftMagMin1[i] - psydata->fftMagMin2[i];
		predMagPlus = 2.0 * psydata->fftMagPlus1[i] - psydata->fftMagPlus2[i];
		predPhMin = 2.0 * psydata->fftPhMin1[i] - psydata->fftPhMin2[i];
		predPhPlus = 2.0 * psydata->fftPhPlus1[i] - psydata->fftPhPlus2[i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psydata->fftMag[i] - predMagMin) < (psydata->fftMag[i] - predMagPlus)) {
				predMag = predMagMin;
				predPh = predPhMin;
			} else {
				predMag = predMagPlus;
				predPh = predPhPlus;
			}
		} else if (predMagMin == 0.0) {
			predMag = predMagPlus;
			predPh = predPhPlus;
		} else { /* predMagPlus == 0.0 */
			predMag = predMagMin;
			predPh = predPhMin;
		}

		mag = psydata->fftMag[i];
		ph = psydata->fftPh[i];

		/* unpredictability */
		psydata->cw[i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}

	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psydata->fftMagMin1S[7][i] - psydata->fftMagMin1S[6][i];
		predMagPlus = 2.0 * psydata->fftMagS[1][i] - psydata->fftMagS[2][i];
		predPhMin = 2.0 * psydata->fftPhMin1S[7][i] - psydata->fftPhMin1S[6][i];
		predPhPlus = 2.0 * psydata->fftPhS[1][i] - psydata->fftPhS[2][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psydata->fftMagS[0][i] - predMagMin) < (psydata->fftMagS[0][i] - predMagPlus)) {
				predMag = predMagMin;
				predPh = predPhMin;
			} else {
				predMag = predMagPlus;
				predPh = predPhPlus;
			}
		} else if (predMagMin == 0.0) {
			predMag = predMagPlus;
			predPh = predPhPlus;
		} else { /* predMagPlus == 0.0 */
			predMag = predMagMin;
			predPh = predPhMin;
		}

		mag = psydata->fftMagS[0][i];
		ph = psydata->fftPhS[0][i];

		/* unpredictability */
		psydata->cwS[0][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}
	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psydata->fftMagS[0][i] - psydata->fftMagMin1S[7][i];
		predMagPlus = 2.0 * psydata->fftMagS[2][i] - psydata->fftMagS[3][i];
		predPhMin = 2.0 * psydata->fftPhS[0][i] - psydata->fftPhMin1S[7][i];
		predPhPlus = 2.0 * psydata->fftPhS[2][i] - psydata->fftPhS[3][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psydata->fftMagS[1][i] - predMagMin) < (psydata->fftMagS[1][i] - predMagPlus)) {
				predMag = predMagMin;
				predPh = predPhMin;
			} else {
				predMag = predMagPlus;
				predPh = predPhPlus;
			}
		} else if (predMagMin == 0.0) {
			predMag = predMagPlus;
			predPh = predPhPlus;
		} else { /* predMagPlus == 0.0 */
			predMag = predMagMin;
			predPh = predPhMin;
		}

		mag = psydata->fftMagS[1][i];
		ph = psydata->fftPhS[1][i];

		/* unpredictability */
		psydata->cwS[1][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}

	for (j = 2; j < 6; j++) {
		for (i = 0; i < psyInfo->sizeS; i++)
		{
			predMagMin = 2.0 * psydata->fftMagS[j-1][i] - psydata->fftMagS[j-2][i];
			predMagPlus = 2.0 * psydata->fftMagS[j+1][i] - psydata->fftMagS[j+2][i];
			predPhMin = 2.0 * psydata->fftPhS[j-1][i] - psydata->fftPhS[j-2][i];
			predPhPlus = 2.0 * psydata->fftPhS[j+1][i] - psydata->fftPhS[j+2][i];
			if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
				if ((psydata->fftMagS[j][i] - predMagMin) < (psydata->fftMagS[j][i] - predMagPlus)) {
					predMag = predMagMin;
					predPh = predPhMin;
				} else {
					predMag = predMagPlus;
					predPh = predPhPlus;
				}
			} else if (predMagMin == 0.0) {
				predMag = predMagPlus;
				predPh = predPhPlus;
			} else { /* predMagPlus == 0.0 */
				predMag = predMagMin;
				predPh = predPhMin;
			}

			mag = psydata->fftMagS[j][i];
			ph = psydata->fftPhS[j][i];

			/* unpredictability */
			psydata->cwS[j][i] =
				sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
		}
	}

	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psydata->fftMagS[5][i] - psydata->fftMagS[4][i];
		predMagPlus = 2.0 * psydata->fftMagS[7][i] - psydata->fftMagPlus1S[0][i];
		predPhMin = 2.0 * psydata->fftPhS[5][i] - psydata->fftPhS[4][i];
		predPhPlus = 2.0 * psydata->fftPhS[7][i] - psydata->fftPhPlus1S[0][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psydata->fftMagS[6][i] - predMagMin) < (psydata->fftMagS[6][i] - predMagPlus)) {
				predMag = predMagMin;
				predPh = predPhMin;
			} else {
				predMag = predMagPlus;
				predPh = predPhPlus;
			}
		} else if (predMagMin == 0.0) {
			predMag = predMagPlus;
			predPh = predPhPlus;
		} else { /* predMagPlus == 0.0 */
			predMag = predMagMin;
			predPh = predPhMin;
		}

		mag = psydata->fftMagS[6][i];
		ph = psydata->fftPhS[6][i];

		/* unpredictability */
		psydata->cwS[6][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}
	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psydata->fftMagS[6][i] - psydata->fftMagMin1S[5][i];
		predMagPlus = 2.0 * psydata->fftMagPlus1S[0][i] - psydata->fftMagPlus1S[1][i];
		predPhMin = 2.0 * psydata->fftPhS[6][i] - psydata->fftPhS[5][i];
		predPhPlus = 2.0 * psydata->fftPhPlus1S[0][i] - psydata->fftPhPlus1S[1][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psydata->fftMagS[7][i] - predMagMin) < (psydata->fftMagS[7][i] - predMagPlus)) {
				predMag = predMagMin;
				predPh = predPhMin;
			} else {
				predMag = predMagPlus;
				predPh = predPhPlus;
			}
		} else if (predMagMin == 0.0) {
			predMag = predMagPlus;
			predPh = predPhPlus;
		} else { /* predMagPlus == 0.0 */
			predMag = predMagMin;
			predPh = predPhMin;
		}

		mag = psydata->fftMagS[7][i];
		ph = psydata->fftPhS[7][i];

		/* unpredictability */
		psydata->cwS[7][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}
}

static void PsyThreshold(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, int *cb_width_long,
						 int num_cb_long, int *cb_width_short, int num_cb_short)
{
	psydata_t *psydata = psyInfo->data;
	gpsydata_t *gpsydata = gpsyInfo->data;

	int b, bb, w, low, high, j;
	double tmp, ct, ecb, cb;
	double tb, snr, bc, en, nb;

	double e[MAX_SCFAC_BANDS];
	double c[MAX_SCFAC_BANDS];

	double tot, mx, estot[8];
	double pe = 0.0;

	/* Energy in each partition and weighted unpredictability */
	high = 0;
	for (b = 0; b < num_cb_long; b++)
	{
		low = high;
		high += cb_width_long[b];

		e[b] = 0.0;
		c[b] = 0.0;

		for (w = low; w < high; w++)
		{
			tmp = psydata->fftMag[w];
			tmp *= tmp;
			e[b] += tmp;
			c[b] += tmp * psydata->cw[w];
		}
	}

	/* Convolve the partitioned energy and unpredictability
	   with the spreading function */
	for (b = 0; b < num_cb_long; b++)
	{
		ecb = 0.0;
		ct = 0.0;

		for (bb = 0; bb < num_cb_long; bb++)
		{
			ecb += e[bb] * gpsydata->spreading[bb][b];
			ct += c[bb] * gpsydata->spreading[bb][b];
		}
		if (ecb != 0.0) cb = ct / ecb;
		else cb = 0.0;
		en = ecb * gpsydata->rnorm[b];
		
		/* Get the tonality index */
		tb = -0.299 - 0.43*log(cb);
		tb = max(min(tb,1),0);

		/* Calculate the required SNR in each partition */
		snr = tb * 18.0 + (1-tb) * 6.0;

		/* Power ratio */
		bc = pow(10.0, 0.1*(-snr));

		/* Actual energy threshold */
		nb = en * bc;
		nb = max(min(nb, psydata->lastNb[b]*2), gpsydata->ath[b]);
		psydata->lastNb[b] = en * bc;

		/* Perceptual entropy */
		tmp = cb_width_long[b]
			* log((nb + 0.0000000001)
			/ (e[b] + 0.0000000001));
		tmp = min(0,tmp);

		pe -= tmp;

		psyInfo->maskThr[b] = psydata->maskThrNext[b];
		psyInfo->maskEn[b] = psydata->maskEnNext[b];
		psydata->maskThrNext[b] = nb;
		psydata->maskEnNext[b] = en;
	}

	/* Short windows */
	for (j = 0; j < 8; j++)
	{
		/* Energy in each partition and weighted unpredictability */
		high = 0;
		for (b = 0; b < num_cb_short; b++)
		{
			low = high;
			high += cb_width_short[b];

			e[b] = 0.0;
			c[b] = 0.0;

			for (w = low; w < high; w++)
			{
				tmp = psydata->fftMagS[j][w];
				tmp *= tmp;
				e[b] += tmp;
				c[b] += tmp * psydata->cwS[j][w];
			}
		}

		estot[j] = 0.0;

		/* Convolve the partitioned energy and unpredictability
		with the spreading function */
		for (b = 0; b < num_cb_short; b++)
		{
			ecb = 0.0;
			ct = 0.0;

			for (bb = 0; bb < num_cb_short; bb++)
			{
				ecb += e[bb] * gpsydata->spreadingS[bb][b];
				ct += c[bb] * gpsydata->spreadingS[bb][b];
			}
			if (ecb != 0.0) cb = ct / ecb;
			else cb = 0.0;
			en = ecb * gpsydata->rnormS[b];
			
			/* Get the tonality index */
			tb = -0.299 - 0.43*log(cb);
			tb = max(min(tb,1),0);

			/* Calculate the required SNR in each partition */
			snr = tb * 18.0 + (1-tb) * 6.0;

			/* Power ratio */
			bc = pow(10.0, 0.1*(-snr));

			/* Actual energy threshold */
			nb = en * bc;
			nb = max(nb, gpsydata->athS[b]);

			estot[j] += e[b];

			psyInfo->maskThrS[j][b] = psydata->maskThrNextS[j][b];
			psyInfo->maskEnS[j][b] = psydata->maskEnNextS[j][b];
			psydata->maskThrNextS[j][b] = nb;
			psydata->maskEnNextS[j][b] = en;
		}

		if (estot[j] != 0.0)
			estot[j] /= num_cb_short;
	}

	tot = mx = estot[0];
	for (j = 1; j < 8; j++) {
		tot += estot[j];
		mx = max(mx, estot[j]);
	}

	tot = max(tot, 1.e-12);
	if (((mx/tot) > 0.25) && (pe > 1100.0) || ((mx/tot) > 0.5)) {
		psyInfo->block_type = ONLY_SHORT_WINDOW;
		psydata->threeInARow++;
	} else if ((psydata->lastEnr > 0.35) && (psydata->lastPe > 1000.0)) {
		psyInfo->block_type = ONLY_SHORT_WINDOW;
		psydata->threeInARow++;
	} else if (psydata->threeInARow >= 3) {
		psyInfo->block_type = ONLY_SHORT_WINDOW;
		psydata->threeInARow = 0;
	} else
		psyInfo->block_type = ONLY_LONG_WINDOW;

 	psydata->lastEnr = mx/tot;
	psydata->lastPe = pe;
}

static void PsyThresholdMS(ChannelInfo *channelInfoL, GlobalPsyInfo *gpsyInfo,
						   PsyInfo *psyInfoL, PsyInfo *psyInfoR,
						   int *cb_width_long, int num_cb_long, int *cb_width_short,
						   int num_cb_short)
{
	psydata_t *psydata_l = psyInfoL->data;
	psydata_t *psydata_r = psyInfoR->data;
	gpsydata_t *gpsydata = gpsyInfo->data;
	int b, bb, w, low, high, j;
	double tmp, ct, ecb, cb;
	double tb, snr, bc, enM, enS, nbM, nbS;

	double eM[MAX_SCFAC_BANDS];
	double eS[MAX_SCFAC_BANDS];
	double cM[MAX_SCFAC_BANDS];
	double cS[MAX_SCFAC_BANDS];

	double x1, x2, db, mld;

#ifdef _DEBUG
	int ms_used = 0;
	int ms_usedS = 0;
#endif

	/* Energy in each partition and weighted unpredictability */
	high = 0;
	for (b = 0; b < num_cb_long; b++)
	{
		low = high;
		high += cb_width_long[b];

		eM[b] = 0.0;
		cM[b] = 0.0;
		eS[b] = 0.0;
		cS[b] = 0.0;

		for (w = low; w < high; w++)
		{
			tmp = (psydata_l->fftMag[w] + psydata_r->fftMag[w]) * 0.5;
			tmp *= tmp;
			eM[b] += tmp;
			cM[b] += tmp * min(psydata_l->cw[w], psydata_r->cw[w]);

			tmp = (psydata_l->fftMag[w] - psydata_r->fftMag[w]) * 0.5;
			tmp *= tmp;
			eS[b] += tmp;
			cS[b] += tmp * min(psydata_l->cw[w], psydata_r->cw[w]);
		}
	}

	/* Convolve the partitioned energy and unpredictability
	   with the spreading function */
	for (b = 0; b < num_cb_long; b++)
	{
		/* Mid channel */
		ecb = 0.0;
		ct = 0.0;

		for (bb = 0; bb < num_cb_long; bb++)
		{
			ecb += eM[bb] * gpsydata->spreading[bb][b];
			ct += cM[bb] * gpsydata->spreading[bb][b];
		}
		if (ecb != 0.0) cb = ct / ecb;
		else cb = 0.0;
		enM = ecb * gpsydata->rnorm[b];
		
		/* Get the tonality index */
		tb = -0.299 - 0.43*log(cb);
		tb = max(min(tb,1),0);

		/* Calculate the required SNR in each partition */
		snr = tb * 18.0 + (1-tb) * 6.0;

		/* Power ratio */
		bc = pow(10.0, 0.1*(-snr));

		/* Actual energy threshold */
		nbM = enM * bc;
		nbM = max(min(nbM, psydata_l->lastNbMS[b]*2), gpsydata->ath[b]);
		psydata_l->lastNbMS[b] = enM * bc;


		/* Side channel */
		ecb = 0.0;
		ct = 0.0;

		for (bb = 0; bb < num_cb_long; bb++)
		{
			ecb += eS[bb] * gpsydata->spreading[bb][b];
			ct += cS[bb] * gpsydata->spreading[bb][b];
		}
		if (ecb != 0.0) cb = ct / ecb;
		else cb = 0.0;
		enS = ecb * gpsydata->rnorm[b];
		
		/* Get the tonality index */
		tb = -0.299 - 0.43*log(cb);
		tb = max(min(tb,1),0);

		/* Calculate the required SNR in each partition */
		snr = tb * 18.0 + (1-tb) * 6.0;

		/* Power ratio */
		bc = pow(10.0, 0.1*(-snr));

		/* Actual energy threshold */
		nbS = enS * bc;
		nbS = max(min(nbS, psydata_r->lastNbMS[b]*2), gpsydata->ath[b]);
		psydata_r->lastNbMS[b] = enS * bc;


		psyInfoL->maskThrMS[b] = psydata_l->maskThrNextMS[b];
		psyInfoR->maskThrMS[b] = psydata_r->maskThrNextMS[b];
		psyInfoL->maskEnMS[b] = psydata_l->maskEnNextMS[b];
		psyInfoR->maskEnMS[b] = psydata_r->maskEnNextMS[b];
		psydata_l->maskThrNextMS[b] = nbM;
		psydata_r->maskThrNextMS[b] = nbS;
		psydata_l->maskEnNextMS[b] = enM;
		psydata_r->maskEnNextMS[b] = enS;

		if (psyInfoL->maskThr[b] <= 1.58*psyInfoR->maskThr[b]
			&& psyInfoR->maskThr[b] <= 1.58*psyInfoL->maskThr[b]) {

			mld = gpsydata->mld[b]*enM;
			psyInfoL->maskThrMS[b] = max(psyInfoL->maskThrMS[b],
				min(psyInfoR->maskThrMS[b],mld));

			mld = gpsydata->mld[b]*enS;
			psyInfoR->maskThrMS[b] = max(psyInfoR->maskThrMS[b],
				min(psyInfoL->maskThrMS[b],mld));
		}

		x1 = min(psyInfoL->maskThr[b], psyInfoR->maskThr[b]);
		x2 = max(psyInfoL->maskThr[b], psyInfoR->maskThr[b]);
		/* thresholds difference in db */
		if (x2 >= 1000*x1) db=3;
		else db = log10(x2/x1);  
		if (db < 0.25) {
#ifdef _DEBUG
			ms_used++;
#endif
			channelInfoL->msInfo.ms_used[b] = 1;
		} else {
			channelInfoL->msInfo.ms_used[b] = 0;
		}
	}

#ifdef _DEBUG
	printf("%d\t", ms_used);
#endif

	/* Short windows */
	for (j = 0; j < 8; j++)
	{
		/* Energy in each partition and weighted unpredictability */
		high = 0;
		for (b = 0; b < num_cb_short; b++)
		{
			low = high;
			high += cb_width_short[b];

			eM[b] = 0.0;
			eS[b] = 0.0;
			cM[b] = 0.0;
			cS[b] = 0.0;

			for (w = low; w < high; w++)
			{
				tmp = (psydata_l->fftMagS[j][w] + psydata_r->fftMagS[j][w]) * 0.5;
				tmp *= tmp;
				eM[b] += tmp;
				cM[b] += tmp * min(psydata_l->cwS[j][w], psydata_r->cwS[j][w]);

				tmp = (psydata_l->fftMagS[j][w] - psydata_r->fftMagS[j][w]) * 0.5;
				tmp *= tmp;
				eS[b] += tmp;
				cS[b] += tmp * min(psydata_l->cwS[j][w], psydata_r->cwS[j][w]);

			}
		}

		/* Convolve the partitioned energy and unpredictability
		with the spreading function */
		for (b = 0; b < num_cb_short; b++)
		{
			/* Mid channel */
			ecb = 0.0;
			ct = 0.0;

			for (bb = 0; bb < num_cb_short; bb++)
			{
				ecb += eM[bb] * gpsydata->spreadingS[bb][b];
				ct += cM[bb] * gpsydata->spreadingS[bb][b];
			}
			if (ecb != 0.0) cb = ct / ecb;
			else cb = 0.0;
			enM = ecb * gpsydata->rnormS[b];
			
			/* Get the tonality index */
			tb = -0.299 - 0.43*log(cb);
			tb = max(min(tb,1),0);

			/* Calculate the required SNR in each partition */
			snr = tb * 18.0 + (1-tb) * 6.0;

			/* Power ratio */
			bc = pow(10.0, 0.1*(-snr));

			/* Actual energy threshold */
			nbM = enM * bc;
			nbM = max(nbM, gpsydata->athS[b]);


			/* Side channel */
			ecb = 0.0;
			ct = 0.0;

			for (bb = 0; bb < num_cb_short; bb++)
			{
				ecb += eS[bb] * gpsydata->spreadingS[bb][b];
				ct += cS[bb] * gpsydata->spreadingS[bb][b];
			}
			if (ecb != 0.0) cb = ct / ecb;
			else cb = 0.0;
			enS = ecb * gpsydata->rnormS[b];
			
			/* Get the tonality index */
			tb = -0.299 - 0.43*log(cb);
			tb = max(min(tb,1),0);

			/* Calculate the required SNR in each partition */
			snr = tb * 18.0 + (1-tb) * 6.0;

			/* Power ratio */
			bc = pow(10.0, 0.1*(-snr));

			/* Actual energy threshold */
			nbS = enS * bc;
			nbS = max(nbS, gpsydata->athS[b]);


			psyInfoL->maskThrSMS[j][b] = psydata_l->maskThrNextSMS[j][b];
			psyInfoR->maskThrSMS[j][b] = psydata_r->maskThrNextSMS[j][b];
			psyInfoL->maskEnSMS[j][b] = psydata_l->maskEnNextSMS[j][b];
			psyInfoR->maskEnSMS[j][b] = psydata_r->maskEnNextSMS[j][b];
			psydata_l->maskThrNextSMS[j][b] = nbM;
			psydata_r->maskThrNextSMS[j][b] = nbS;
			psydata_l->maskEnNextSMS[j][b] = enM;
			psydata_r->maskEnNextSMS[j][b] = enS;

			if (psyInfoL->maskThrS[j][b] <= 1.58*psyInfoR->maskThrS[j][b]
				&& psyInfoR->maskThrS[j][b] <= 1.58*psyInfoL->maskThrS[j][b]) {

				mld = gpsydata->mldS[b]*enM;
				psyInfoL->maskThrSMS[j][b] = max(psyInfoL->maskThrSMS[j][b],
					min(psyInfoR->maskThrSMS[j][b],mld));

				mld = gpsydata->mldS[b]*enS;
				psyInfoR->maskThrSMS[j][b] = max(psyInfoR->maskThrSMS[j][b],
					min(psyInfoL->maskThrSMS[j][b],mld));
			}

			x1 = min(psyInfoL->maskThrS[j][b], psyInfoR->maskThrS[j][b]);
			x2 = max(psyInfoL->maskThrS[j][b], psyInfoR->maskThrS[j][b]);
			/* thresholds difference in db */
			if (x2 >= 1000*x1) db = 3;
			else db = log10(x2/x1);
			if (db < 0.25) {
#ifdef _DEBUG
				ms_usedS++;
#endif
				channelInfoL->msInfo.ms_usedS[j][b] = 1;
			} else {
				channelInfoL->msInfo.ms_usedS[j][b] = 0;
			}
		}
	}

#ifdef _DEBUG
	printf("%d\t", ms_usedS);
#endif
}

static void BlockSwitch(CoderInfo *coderInfo, PsyInfo *psyInfo, unsigned int numChannels)
{
	unsigned int channel;
	int desire = ONLY_LONG_WINDOW;

	/* Use the same block type for all channels
	   If there is 1 channel that wants a short block,
	   use a short block on all channels.
	*/
	for (channel = 0; channel < numChannels; channel++)
	{
		if (psyInfo[channel].block_type == ONLY_SHORT_WINDOW)
			desire = ONLY_SHORT_WINDOW;
	}

	for (channel = 0; channel < numChannels; channel++)
	{
		if ((coderInfo[channel].block_type == ONLY_SHORT_WINDOW) ||
			(coderInfo[channel].block_type == LONG_SHORT_WINDOW) ) {
			if ((coderInfo[channel].desired_block_type==ONLY_LONG_WINDOW) &&
				(desire == ONLY_LONG_WINDOW) ) {
				coderInfo[channel].block_type = SHORT_LONG_WINDOW;
			} else {
				coderInfo[channel].block_type = ONLY_SHORT_WINDOW;
			}
		} else if (desire == ONLY_SHORT_WINDOW) {
			coderInfo[channel].block_type = LONG_SHORT_WINDOW;
		} else {
			coderInfo[channel].block_type = ONLY_LONG_WINDOW;
		}
		coderInfo[channel].desired_block_type = desire;
	}
}

static double freq2bark(double freq)
{
    double bark;

    if(freq > 200.0)
		bark = 26.81 / (1 + (1960 / freq)) - 0.53; 
    else
		bark = freq / 102.9;

    return (bark);
}

static double ATHformula(double f)
{
	double ath;
	f /= 1000;  // convert to khz
	f  = max(0.01, f);
	f  = min(18.0,f);

	/* from Painter & Spanias, 1997 */
	/* minimum: (i=77) 3.3kHz = -5db */
	ath =    3.640 * pow(f,-0.8)
		- 6.500 * exp(-0.6*pow(f-3.3,2.0))
		+ 0.001 * pow(f,4.0);
	return ath;
}

psymodel_t psymodel1 =
{
  PsyInit,
  PsyEnd,
  PsyCalculate,
  PsyBufferUpdate,
  BlockSwitch
};
