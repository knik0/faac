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
 * $Id: psych.c,v 1.14 2001/09/28 18:36:06 menno Exp $
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


void PsyInit(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels,
			 unsigned int sampleRate, int *cb_width_long, int num_cb_long,
			 int *cb_width_short, int num_cb_short)
{
	unsigned int channel;
	int i, j, b, bb, high, low, size;
	double tmpx,tmpy,tmp,x,b1,b2;
	double bval[MAX_SCFAC_BANDS];

	gpsyInfo->ath = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->athS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->rnorm = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->rnormS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->mld = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->mldS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->hannWindow = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));
	gpsyInfo->hannWindowS = (double*)AllocMemory(2*BLOCK_LEN_SHORT*sizeof(double));

	for(i = 0; i < BLOCK_LEN_LONG*2; i++)
		gpsyInfo->hannWindow[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_LONG*2)));
	for(i = 0; i < BLOCK_LEN_SHORT*2; i++)
		gpsyInfo->hannWindowS[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_SHORT*2)));
	gpsyInfo->sampleRate = (double)sampleRate;

	size = BLOCK_LEN_LONG;
	for (channel = 0; channel < numChannels; channel++) {
		psyInfo[channel].size = size;

		psyInfo[channel].lastPe = 0.0;
		psyInfo[channel].lastEnr = 0.0;
		psyInfo[channel].threeInARow = 0;
		psyInfo[channel].cw = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].maskThr = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEn = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrNext = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnNext = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrNextMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnNextMS = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].prevSamples = (double*)AllocMemory(size*sizeof(double));
		memset(psyInfo[channel].prevSamples, 0, size*sizeof(double));

		psyInfo[channel].lastNb = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].lastNbMS = (double*)AllocMemory(size*sizeof(double));
		for (j = 0; j < size; j++) {
			psyInfo[channel].lastNb[j] = 2.;
			psyInfo[channel].lastNbMS[j] = 2.;
		}

		psyInfo[channel].fftMagPlus2 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftMagPlus1 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftMag = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftMagMin1 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftMagMin2 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftPhPlus2 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftPhPlus1 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftPh = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftPhMin1 = (double*)AllocMemory(size*sizeof(double));
		psyInfo[channel].fftPhMin2 = (double*)AllocMemory(size*sizeof(double));
	}

	size = BLOCK_LEN_SHORT;
	for (channel = 0; channel < numChannels; channel++) {
		psyInfo[channel].sizeS = size;

		psyInfo[channel].prevSamplesS = (double*)AllocMemory(size*sizeof(double));
		memset(psyInfo[channel].prevSamplesS, 0, size*sizeof(double));

		for (j = 0; j < 8; j++) {
			psyInfo[channel].cwS[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].maskThrS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrNextS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnNextS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrNextSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnNextSMS[j] = (double*)AllocMemory(MAX_SCFAC_BANDS*sizeof(double));

			psyInfo[channel].fftMagPlus2S[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftMagPlus1S[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftMagS[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftMagMin1S[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftPhPlus2S[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftPhPlus1S[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftPhS[j] = (double*)AllocMemory(size*sizeof(double));
			psyInfo[channel].fftPhMin1S[j] = (double*)AllocMemory(size*sizeof(double));
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

			if (tmpy <= -100.0) gpsyInfo->spreading[b][bb] = 0.0;
			else gpsyInfo->spreading[b][bb] = exp((x + tmpy)*0.2302585093);
		}
	}

    for( b = 0; b < num_cb_long; b++){
		tmp = 0.0;
		for( bb = 0; bb < num_cb_long; bb++)
			tmp += gpsyInfo->spreading[bb][b];
		gpsyInfo->rnorm[b] = 1.0/tmp;
    }

	j = 0;
    for( b = 0; b < num_cb_long; b++){
		gpsyInfo->ath[b] = 1.e37;

		for (bb = 0; bb < cb_width_long[b]; bb++, j++) {
			double freq = gpsyInfo->sampleRate*j/(1000.0*2*size);
			double level;
			level = ATHformula(freq*1000) - 20;
			level = pow(10., 0.1*level);
			level *= cb_width_long[b];
			if (level < gpsyInfo->ath[b])
				gpsyInfo->ath[b] = level;
		}
    }

	low = 0;
	for (b = 0; b < num_cb_long; b++) {
		tmp = freq2bark(gpsyInfo->sampleRate*low/(2*size));
		tmp = (min(tmp, 15.5)/15.5);

		gpsyInfo->mld[b] = pow(10.0, 1.25*(1-cos(M_PI*tmp))-2.5);
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

			if (tmpy <= -100.0) gpsyInfo->spreadingS[b][bb] = 0.0;
			else gpsyInfo->spreadingS[b][bb] = exp((x + tmpy)*0.2302585093);
		}
	}

	j = 0;
    for( b = 0; b < num_cb_short; b++){
		gpsyInfo->athS[b] = 1.e37;

		for (bb = 0; bb < cb_width_short[b]; bb++, j++) {
			double freq = gpsyInfo->sampleRate*j/(1000.0*2*size);
			double level;
			level = ATHformula(freq*1000) - 20;
			level = pow(10., 0.1*level);
			level *= cb_width_short[b];
			if (level < gpsyInfo->athS[b])
				gpsyInfo->athS[b] = level;
		}
    }

    for( b = 0; b < num_cb_short; b++){
		tmp = 0.0;
		for( bb = 0; bb < num_cb_short; bb++)
			tmp += gpsyInfo->spreadingS[bb][b];
		gpsyInfo->rnormS[b] = 1.0/tmp;
    }

	low = 0;
	for (b = 0; b < num_cb_short; b++) {
		tmp = freq2bark(gpsyInfo->sampleRate*low/(2*size));
		tmp = (min(tmp, 15.5)/15.5);

		gpsyInfo->mldS[b] = pow(10.0, 1.25*(1-cos(M_PI*tmp))-2.5);
		low += cb_width_short[b];
	}
}

void PsyEnd(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels)
{
	unsigned int channel;
	int j;

	if (gpsyInfo->ath) FreeMemory(gpsyInfo->ath);
	if (gpsyInfo->athS) FreeMemory(gpsyInfo->athS);
	if (gpsyInfo->rnorm) FreeMemory(gpsyInfo->rnorm);
	if (gpsyInfo->rnormS) FreeMemory(gpsyInfo->rnormS);
	if (gpsyInfo->mld) FreeMemory(gpsyInfo->mld);
	if (gpsyInfo->mldS) FreeMemory(gpsyInfo->mldS);
	if (gpsyInfo->hannWindow) FreeMemory(gpsyInfo->hannWindow);
	if (gpsyInfo->hannWindowS) FreeMemory(gpsyInfo->hannWindowS);

	for (channel = 0; channel < numChannels; channel++) {
		if (psyInfo[channel].prevSamples) FreeMemory(psyInfo[channel].prevSamples);
		if (psyInfo[channel].cw) FreeMemory(psyInfo[channel].cw);
		if (psyInfo[channel].maskThr) FreeMemory(psyInfo[channel].maskThr);
		if (psyInfo[channel].maskEn) FreeMemory(psyInfo[channel].maskEn);
		if (psyInfo[channel].maskThrNext) FreeMemory(psyInfo[channel].maskThrNext);
		if (psyInfo[channel].maskEnNext) FreeMemory(psyInfo[channel].maskEnNext);
		if (psyInfo[channel].maskThrMS) FreeMemory(psyInfo[channel].maskThrMS);
		if (psyInfo[channel].maskEnMS) FreeMemory(psyInfo[channel].maskEnMS);
		if (psyInfo[channel].maskThrNextMS) FreeMemory(psyInfo[channel].maskThrNextMS);
		if (psyInfo[channel].maskEnNextMS) FreeMemory(psyInfo[channel].maskEnNextMS);
		
		if (psyInfo[channel].lastNb) FreeMemory(psyInfo[channel].lastNb);
		if (psyInfo[channel].lastNbMS) FreeMemory(psyInfo[channel].lastNbMS);

		if (psyInfo[channel].fftMagPlus2) FreeMemory(psyInfo[channel].fftMagPlus2);
		if (psyInfo[channel].fftMagPlus1) FreeMemory(psyInfo[channel].fftMagPlus1);
		if (psyInfo[channel].fftMag) FreeMemory(psyInfo[channel].fftMag);
		if (psyInfo[channel].fftMagMin1) FreeMemory(psyInfo[channel].fftMagMin1);
		if (psyInfo[channel].fftMagMin2) FreeMemory(psyInfo[channel].fftMagMin2);
		if (psyInfo[channel].fftPhPlus2) FreeMemory(psyInfo[channel].fftPhPlus2);
		if (psyInfo[channel].fftPhPlus1) FreeMemory(psyInfo[channel].fftPhPlus1);
		if (psyInfo[channel].fftPh) FreeMemory(psyInfo[channel].fftPh);
		if (psyInfo[channel].fftPhMin1) FreeMemory(psyInfo[channel].fftPhMin1);
		if (psyInfo[channel].fftPhMin2) FreeMemory(psyInfo[channel].fftPhMin2);
	}

	for (channel = 0; channel < numChannels; channel++) {
		if(psyInfo[channel].prevSamplesS) FreeMemory(psyInfo[channel].prevSamplesS);
		for (j = 0; j < 8; j++) {
			if (psyInfo[channel].cwS[j]) FreeMemory(psyInfo[channel].cwS[j]);
			if (psyInfo[channel].maskThrS[j]) FreeMemory(psyInfo[channel].maskThrS[j]);
			if (psyInfo[channel].maskEnS[j]) FreeMemory(psyInfo[channel].maskEnS[j]);
			if (psyInfo[channel].maskThrNextS[j]) FreeMemory(psyInfo[channel].maskThrNextS[j]);
			if (psyInfo[channel].maskEnNextS[j]) FreeMemory(psyInfo[channel].maskEnNextS[j]);
			if (psyInfo[channel].maskThrSMS[j]) FreeMemory(psyInfo[channel].maskThrSMS[j]);
			if (psyInfo[channel].maskEnSMS[j]) FreeMemory(psyInfo[channel].maskEnSMS[j]);
			if (psyInfo[channel].maskThrNextSMS[j]) FreeMemory(psyInfo[channel].maskThrNextSMS[j]);
			if (psyInfo[channel].maskEnNextSMS[j]) FreeMemory(psyInfo[channel].maskEnNextSMS[j]);

			if (psyInfo[channel].fftMagPlus2S[j]) FreeMemory(psyInfo[channel].fftMagPlus2S[j]);
			if (psyInfo[channel].fftMagPlus1S[j]) FreeMemory(psyInfo[channel].fftMagPlus1S[j]);
			if (psyInfo[channel].fftMagS[j]) FreeMemory(psyInfo[channel].fftMagS[j]);
			if (psyInfo[channel].fftMagMin1S[j]) FreeMemory(psyInfo[channel].fftMagMin1S[j]);
			if (psyInfo[channel].fftPhPlus2S[j]) FreeMemory(psyInfo[channel].fftPhPlus2S[j]);
			if (psyInfo[channel].fftPhPlus1S[j]) FreeMemory(psyInfo[channel].fftPhPlus1S[j]);
			if (psyInfo[channel].fftPhS[j]) FreeMemory(psyInfo[channel].fftPhS[j]);
			if (psyInfo[channel].fftPhMin1S[j]) FreeMemory(psyInfo[channel].fftPhMin1S[j]);
		}
	}
}

/* Do psychoacoustical analysis */
void PsyCalculate(ChannelInfo *channelInfo, GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
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

void PsyBufferUpdate(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, double *newSamples)
{
	int i, j;
	double a, b;
	double *transBuff, *transBuffS, *tmp;

	transBuff = (double*)AllocMemory(2*psyInfo->size*sizeof(double));

	memcpy(transBuff, psyInfo->prevSamples, psyInfo->size*sizeof(double));
	memcpy(transBuff + psyInfo->size, newSamples, psyInfo->size*sizeof(double));


	/* In 2 frames this will be the frequencies where
	   the psychoacoustics are calculated for */
	Hann(gpsyInfo, transBuff, 2*psyInfo->size);
	rsfft(transBuff, 11);


	/* shift all buffers 1 frame ahead */
	tmp = psyInfo->fftMagMin2;
	psyInfo->fftMagMin2 = psyInfo->fftMagMin1;
	psyInfo->fftMagMin1 = psyInfo->fftMag;
	psyInfo->fftMag = psyInfo->fftMagPlus1;
	psyInfo->fftMagPlus1 = psyInfo->fftMagPlus2;
	psyInfo->fftMagPlus2 = tmp;

	tmp = psyInfo->fftPhMin2;
	psyInfo->fftPhMin2 = psyInfo->fftPhMin1;
	psyInfo->fftPhMin1 = psyInfo->fftPh;
	psyInfo->fftPh = psyInfo->fftPhPlus1;
	psyInfo->fftPhPlus1 = psyInfo->fftPhPlus2;
	psyInfo->fftPhPlus2 = tmp;


	/* Calculate magnitude and phase of new data */
	for (i = 0; i < psyInfo->size; i++) {
		a = transBuff[i];
		b = transBuff[i + psyInfo->size];
		psyInfo->fftMagPlus2[i] = sqrt(a*a + b*b);

		if(a > 0.0){
			if(b >= 0.0)
				psyInfo->fftPhPlus2[i] = atan2(b, a);
			else
				psyInfo->fftPhPlus2[i] = atan2(b, a) + M_PI * 2.0;
		} else if(a < 0.0) {
			psyInfo->fftPhPlus2[i] = atan2(b, a) + M_PI;
		} else {
			if(b > 0.0)
				psyInfo->fftPhPlus2[i] = M_PI * 0.5;
			else if( b < 0.0 )
				psyInfo->fftPhPlus2[i] = M_PI * 1.5;
			else
				psyInfo->fftPhPlus2[i] = 0.0;
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
		rsfft(transBuffS, 8);


		/* shift all buffers 1 frame ahead */
		tmp = psyInfo->fftMagMin1S[j];
		psyInfo->fftMagMin1S[j] = psyInfo->fftMagS[j];
		psyInfo->fftMagS[j] = psyInfo->fftMagPlus1S[j];
		psyInfo->fftMagPlus1S[j] = psyInfo->fftMagPlus2S[j];
		psyInfo->fftMagPlus2S[j] = tmp;

		tmp = psyInfo->fftPhMin1S[j];
		psyInfo->fftPhMin1S[j] = psyInfo->fftPhS[j];
		psyInfo->fftPhS[j] = psyInfo->fftPhPlus1S[j];
		psyInfo->fftPhPlus1S[j] = psyInfo->fftPhPlus2S[j];
		psyInfo->fftPhPlus2S[j] = tmp;


		/* Calculate magnitude and phase of new data */
		for (i = 0; i < psyInfo->sizeS; i++) {
			a = transBuffS[i];
			b = transBuffS[i + psyInfo->sizeS];
			psyInfo->fftMagPlus2S[j][i] = sqrt(a*a + b*b);

			if(a > 0.0){
				if(b >= 0.0)
					psyInfo->fftPhPlus2S[j][i] = atan2(b, a);
				else
					psyInfo->fftPhPlus2S[j][i] = atan2(b, a) + M_PI * 2.0;
			} else if(a < 0.0) {
				psyInfo->fftPhPlus2S[j][i] = atan2(b, a) + M_PI;
			} else {
				if(b > 0.0)
					psyInfo->fftPhPlus2S[j][i] = M_PI * 0.5;
				else if( b < 0.0 )
					psyInfo->fftPhPlus2S[j][i] = M_PI * 1.5;
				else
					psyInfo->fftPhPlus2S[j][i] = 0.0;
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

	for (i = 0; i < psyInfo->size; i++)
	{
		predMagMin = 2.0 * psyInfo->fftMagMin1[i] - psyInfo->fftMagMin2[i];
		predMagPlus = 2.0 * psyInfo->fftMagPlus1[i] - psyInfo->fftMagPlus2[i];
		predPhMin = 2.0 * psyInfo->fftPhMin1[i] - psyInfo->fftPhMin2[i];
		predPhPlus = 2.0 * psyInfo->fftPhPlus1[i] - psyInfo->fftPhPlus2[i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psyInfo->fftMag[i] - predMagMin) < (psyInfo->fftMag[i] - predMagPlus)) {
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

		mag = psyInfo->fftMag[i];
		ph = psyInfo->fftPh[i];

		/* unpredictability */
		psyInfo->cw[i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}

	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psyInfo->fftMagMin1S[7][i] - psyInfo->fftMagMin1S[6][i];
		predMagPlus = 2.0 * psyInfo->fftMagS[1][i] - psyInfo->fftMagS[2][i];
		predPhMin = 2.0 * psyInfo->fftPhMin1S[7][i] - psyInfo->fftPhMin1S[6][i];
		predPhPlus = 2.0 * psyInfo->fftPhS[1][i] - psyInfo->fftPhS[2][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psyInfo->fftMagS[0][i] - predMagMin) < (psyInfo->fftMagS[0][i] - predMagPlus)) {
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

		mag = psyInfo->fftMagS[0][i];
		ph = psyInfo->fftPhS[0][i];

		/* unpredictability */
		psyInfo->cwS[0][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}
	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psyInfo->fftMagS[0][i] - psyInfo->fftMagMin1S[7][i];
		predMagPlus = 2.0 * psyInfo->fftMagS[2][i] - psyInfo->fftMagS[3][i];
		predPhMin = 2.0 * psyInfo->fftPhS[0][i] - psyInfo->fftPhMin1S[7][i];
		predPhPlus = 2.0 * psyInfo->fftPhS[2][i] - psyInfo->fftPhS[3][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psyInfo->fftMagS[1][i] - predMagMin) < (psyInfo->fftMagS[1][i] - predMagPlus)) {
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

		mag = psyInfo->fftMagS[1][i];
		ph = psyInfo->fftPhS[1][i];

		/* unpredictability */
		psyInfo->cwS[1][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}

	for (j = 2; j < 6; j++) {
		for (i = 0; i < psyInfo->sizeS; i++)
		{
			predMagMin = 2.0 * psyInfo->fftMagS[j-1][i] - psyInfo->fftMagS[j-2][i];
			predMagPlus = 2.0 * psyInfo->fftMagS[j+1][i] - psyInfo->fftMagS[j+2][i];
			predPhMin = 2.0 * psyInfo->fftPhS[j-1][i] - psyInfo->fftPhS[j-2][i];
			predPhPlus = 2.0 * psyInfo->fftPhS[j+1][i] - psyInfo->fftPhS[j+2][i];
			if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
				if ((psyInfo->fftMagS[j][i] - predMagMin) < (psyInfo->fftMagS[j][i] - predMagPlus)) {
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

			mag = psyInfo->fftMagS[j][i];
			ph = psyInfo->fftPhS[j][i];

			/* unpredictability */
			psyInfo->cwS[j][i] =
				sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
		}
	}

	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psyInfo->fftMagS[5][i] - psyInfo->fftMagS[4][i];
		predMagPlus = 2.0 * psyInfo->fftMagS[7][i] - psyInfo->fftMagPlus1S[0][i];
		predPhMin = 2.0 * psyInfo->fftPhS[5][i] - psyInfo->fftPhS[4][i];
		predPhPlus = 2.0 * psyInfo->fftPhS[7][i] - psyInfo->fftPhPlus1S[0][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psyInfo->fftMagS[6][i] - predMagMin) < (psyInfo->fftMagS[6][i] - predMagPlus)) {
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

		mag = psyInfo->fftMagS[6][i];
		ph = psyInfo->fftPhS[6][i];

		/* unpredictability */
		psyInfo->cwS[6][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}
	for (i = 0; i < psyInfo->sizeS; i++)
	{
		predMagMin = 2.0 * psyInfo->fftMagS[6][i] - psyInfo->fftMagMin1S[5][i];
		predMagPlus = 2.0 * psyInfo->fftMagPlus1S[0][i] - psyInfo->fftMagPlus1S[1][i];
		predPhMin = 2.0 * psyInfo->fftPhS[6][i] - psyInfo->fftPhS[5][i];
		predPhPlus = 2.0 * psyInfo->fftPhPlus1S[0][i] - psyInfo->fftPhPlus1S[1][i];
		if ((predMagMin != 0.0) && (predMagPlus != 0.0)) {
			if ((psyInfo->fftMagS[7][i] - predMagMin) < (psyInfo->fftMagS[7][i] - predMagPlus)) {
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

		mag = psyInfo->fftMagS[7][i];
		ph = psyInfo->fftPhS[7][i];

		/* unpredictability */
		psyInfo->cwS[7][i] =
			sqrt(mag*mag+predMag*predMag-2*mag*predMag*cos(ph+predPh))/(mag+fabs(predMag));
	}
}

static void PsyThreshold(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, int *cb_width_long,
						 int num_cb_long, int *cb_width_short, int num_cb_short)
{
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
			tmp = psyInfo->fftMag[w];
			tmp *= tmp;
			e[b] += tmp;
			c[b] += tmp * psyInfo->cw[w];
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
			ecb += e[bb] * gpsyInfo->spreading[bb][b];
			ct += c[bb] * gpsyInfo->spreading[bb][b];
		}
		if (ecb != 0.0) cb = ct / ecb;
		else cb = 0.0;
		en = ecb * gpsyInfo->rnorm[b];
		
		/* Get the tonality index */
		tb = -0.299 - 0.43*log(cb);
		tb = max(min(tb,1),0);

		/* Calculate the required SNR in each partition */
		snr = tb * 18.0 + (1-tb) * 6.0;

		/* Power ratio */
		bc = pow(10.0, 0.1*(-snr));

		/* Actual energy threshold */
		nb = en * bc;
		nb = max(min(nb, psyInfo->lastNb[b]*2), gpsyInfo->ath[b]);
		psyInfo->lastNb[b] = en * bc;

		/* Perceptual entropy */
		tmp = cb_width_long[b]
			* log((nb + 0.0000000001)
			/ (e[b] + 0.0000000001));
		tmp = min(0,tmp);

		pe -= tmp;

		psyInfo->maskThr[b] = psyInfo->maskThrNext[b];
		psyInfo->maskEn[b] = psyInfo->maskEnNext[b];
		psyInfo->maskThrNext[b] = nb;
		psyInfo->maskEnNext[b] = en;
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
				tmp = psyInfo->fftMagS[j][w];
				tmp *= tmp;
				e[b] += tmp;
				c[b] += tmp * psyInfo->cwS[j][w];
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
				ecb += e[bb] * gpsyInfo->spreadingS[bb][b];
				ct += c[bb] * gpsyInfo->spreadingS[bb][b];
			}
			if (ecb != 0.0) cb = ct / ecb;
			else cb = 0.0;
			en = ecb * gpsyInfo->rnormS[b];
			
			/* Get the tonality index */
			tb = -0.299 - 0.43*log(cb);
			tb = max(min(tb,1),0);

			/* Calculate the required SNR in each partition */
			snr = tb * 18.0 + (1-tb) * 6.0;

			/* Power ratio */
			bc = pow(10.0, 0.1*(-snr));

			/* Actual energy threshold */
			nb = en * bc;
			nb = max(nb, gpsyInfo->athS[b]);

			estot[j] += e[b];

			psyInfo->maskThrS[j][b] = psyInfo->maskThrNextS[j][b];
			psyInfo->maskEnS[j][b] = psyInfo->maskEnNextS[j][b];
			psyInfo->maskThrNextS[j][b] = nb;
			psyInfo->maskEnNextS[j][b] = en;
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
		psyInfo->threeInARow++;
	} else if ((psyInfo->lastEnr > 0.35) && (psyInfo->lastPe > 1000.0)) {
		psyInfo->block_type = ONLY_SHORT_WINDOW;
		psyInfo->threeInARow++;
	} else if (psyInfo->threeInARow >= 3) {
		psyInfo->block_type = ONLY_SHORT_WINDOW;
		psyInfo->threeInARow = 0;
	} else
		psyInfo->block_type = ONLY_LONG_WINDOW;

 	psyInfo->lastEnr = mx/tot;
	psyInfo->lastPe = pe;
}

static void PsyThresholdMS(ChannelInfo *channelInfoL, GlobalPsyInfo *gpsyInfo,
						   PsyInfo *psyInfoL, PsyInfo *psyInfoR,
						   int *cb_width_long, int num_cb_long, int *cb_width_short,
						   int num_cb_short)
{
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
			tmp = (psyInfoL->fftMag[w] + psyInfoR->fftMag[w]) * 0.5;
			tmp *= tmp;
			eM[b] += tmp;
			cM[b] += tmp * min(psyInfoL->cw[w], psyInfoR->cw[w]);

			tmp = (psyInfoL->fftMag[w] - psyInfoR->fftMag[w]) * 0.5;
			tmp *= tmp;
			eS[b] += tmp;
			cS[b] += tmp * min(psyInfoL->cw[w], psyInfoR->cw[w]);
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
			ecb += eM[bb] * gpsyInfo->spreading[bb][b];
			ct += cM[bb] * gpsyInfo->spreading[bb][b];
		}
		if (ecb != 0.0) cb = ct / ecb;
		else cb = 0.0;
		enM = ecb * gpsyInfo->rnorm[b];
		
		/* Get the tonality index */
		tb = -0.299 - 0.43*log(cb);
		tb = max(min(tb,1),0);

		/* Calculate the required SNR in each partition */
		snr = tb * 18.0 + (1-tb) * 6.0;

		/* Power ratio */
		bc = pow(10.0, 0.1*(-snr));

		/* Actual energy threshold */
		nbM = enM * bc;
		nbM = max(min(nbM, psyInfoL->lastNbMS[b]*2), gpsyInfo->ath[b]);
		psyInfoL->lastNbMS[b] = enM * bc;


		/* Side channel */
		ecb = 0.0;
		ct = 0.0;

		for (bb = 0; bb < num_cb_long; bb++)
		{
			ecb += eS[bb] * gpsyInfo->spreading[bb][b];
			ct += cS[bb] * gpsyInfo->spreading[bb][b];
		}
		if (ecb != 0.0) cb = ct / ecb;
		else cb = 0.0;
		enS = ecb * gpsyInfo->rnorm[b];
		
		/* Get the tonality index */
		tb = -0.299 - 0.43*log(cb);
		tb = max(min(tb,1),0);

		/* Calculate the required SNR in each partition */
		snr = tb * 18.0 + (1-tb) * 6.0;

		/* Power ratio */
		bc = pow(10.0, 0.1*(-snr));

		/* Actual energy threshold */
		nbS = enS * bc;
		nbS = max(min(nbS, psyInfoR->lastNbMS[b]*2), gpsyInfo->ath[b]);
		psyInfoR->lastNbMS[b] = enS * bc;


		psyInfoL->maskThrMS[b] = psyInfoL->maskThrNextMS[b];
		psyInfoR->maskThrMS[b] = psyInfoR->maskThrNextMS[b];
		psyInfoL->maskEnMS[b] = psyInfoL->maskEnNextMS[b];
		psyInfoR->maskEnMS[b] = psyInfoR->maskEnNextMS[b];
		psyInfoL->maskThrNextMS[b] = nbM;
		psyInfoR->maskThrNextMS[b] = nbS;
		psyInfoL->maskEnNextMS[b] = enM;
		psyInfoR->maskEnNextMS[b] = enS;

		if (psyInfoL->maskThr[b] <= 1.58*psyInfoR->maskThr[b]
			&& psyInfoR->maskThr[b] <= 1.58*psyInfoL->maskThr[b]) {

			mld = gpsyInfo->mld[b]*enM;
			psyInfoL->maskThrMS[b] = max(psyInfoL->maskThrMS[b],
				min(psyInfoR->maskThrMS[b],mld));

			mld = gpsyInfo->mld[b]*enS;
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
				tmp = (psyInfoL->fftMagS[j][w] + psyInfoR->fftMagS[j][w]) * 0.5;
				tmp *= tmp;
				eM[b] += tmp;
				cM[b] += tmp * min(psyInfoL->cwS[j][w], psyInfoR->cwS[j][w]);

				tmp = (psyInfoL->fftMagS[j][w] - psyInfoR->fftMagS[j][w]) * 0.5;
				tmp *= tmp;
				eS[b] += tmp;
				cS[b] += tmp * min(psyInfoL->cwS[j][w], psyInfoR->cwS[j][w]);

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
				ecb += eM[bb] * gpsyInfo->spreadingS[bb][b];
				ct += cM[bb] * gpsyInfo->spreadingS[bb][b];
			}
			if (ecb != 0.0) cb = ct / ecb;
			else cb = 0.0;
			enM = ecb * gpsyInfo->rnormS[b];
			
			/* Get the tonality index */
			tb = -0.299 - 0.43*log(cb);
			tb = max(min(tb,1),0);

			/* Calculate the required SNR in each partition */
			snr = tb * 18.0 + (1-tb) * 6.0;

			/* Power ratio */
			bc = pow(10.0, 0.1*(-snr));

			/* Actual energy threshold */
			nbM = enM * bc;
			nbM = max(nbM, gpsyInfo->athS[b]);


			/* Side channel */
			ecb = 0.0;
			ct = 0.0;

			for (bb = 0; bb < num_cb_short; bb++)
			{
				ecb += eS[bb] * gpsyInfo->spreadingS[bb][b];
				ct += cS[bb] * gpsyInfo->spreadingS[bb][b];
			}
			if (ecb != 0.0) cb = ct / ecb;
			else cb = 0.0;
			enS = ecb * gpsyInfo->rnormS[b];
			
			/* Get the tonality index */
			tb = -0.299 - 0.43*log(cb);
			tb = max(min(tb,1),0);

			/* Calculate the required SNR in each partition */
			snr = tb * 18.0 + (1-tb) * 6.0;

			/* Power ratio */
			bc = pow(10.0, 0.1*(-snr));

			/* Actual energy threshold */
			nbS = enS * bc;
			nbS = max(nbS, gpsyInfo->athS[b]);


			psyInfoL->maskThrSMS[j][b] = psyInfoL->maskThrNextSMS[j][b];
			psyInfoR->maskThrSMS[j][b] = psyInfoR->maskThrNextSMS[j][b];
			psyInfoL->maskEnSMS[j][b] = psyInfoL->maskEnNextSMS[j][b];
			psyInfoR->maskEnSMS[j][b] = psyInfoR->maskEnNextSMS[j][b];
			psyInfoL->maskThrNextSMS[j][b] = nbM;
			psyInfoR->maskThrNextSMS[j][b] = nbS;
			psyInfoL->maskEnNextSMS[j][b] = enM;
			psyInfoR->maskEnNextSMS[j][b] = enS;

			if (psyInfoL->maskThrS[j][b] <= 1.58*psyInfoR->maskThrS[j][b]
				&& psyInfoR->maskThrS[j][b] <= 1.58*psyInfoL->maskThrS[j][b]) {

				mld = gpsyInfo->mldS[b]*enM;
				psyInfoL->maskThrSMS[j][b] = max(psyInfoL->maskThrSMS[j][b],
					min(psyInfoR->maskThrSMS[j][b],mld));

				mld = gpsyInfo->mldS[b]*enS;
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

void BlockSwitch(CoderInfo *coderInfo, PsyInfo *psyInfo, unsigned int numChannels)
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
