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
 * $Id: psych.c,v 1.4 2001/02/01 20:22:47 menno Exp $
 */

#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "psych.h"
#include "coder.h"
#include "fft.h"

#define NS_INTERP(x,y,r) (pow((x),(r))*pow((y),1-(r)))
#define SQRT2 1.41421356237309504880

void PsyInit(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels,
			 unsigned int sampleRate, unsigned int sampleRateIdx)
{
	unsigned int channel;
	int i, j, b, bb, high, low, size;
	double tmpx,tmpy,tmp,x;
	double bval[MAX_NPART], SNR;

	gpsyInfo->ath = (double*)malloc(NPART_LONG*sizeof(double));
	gpsyInfo->athS = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->mld = (double*)malloc(NPART_LONG*sizeof(double));
	gpsyInfo->mldS = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
	gpsyInfo->window = (double*)malloc(2*BLOCK_LEN_LONG*sizeof(double));
	gpsyInfo->windowS = (double*)malloc(2*BLOCK_LEN_SHORT*sizeof(double));

	for(i = 0; i < BLOCK_LEN_LONG*2; i++)
		gpsyInfo->window[i] = 0.42-0.5*cos(2*M_PI*(i+.5)/(BLOCK_LEN_LONG*2))+
			0.08*cos(4*M_PI*(i+.5)/(BLOCK_LEN_LONG*2));
	for(i = 0; i < BLOCK_LEN_SHORT*2; i++)
		gpsyInfo->windowS[i] = 0.5 * (1-cos(2.0*M_PI*(i+0.5)/(BLOCK_LEN_SHORT*2)));
	gpsyInfo->sampleRate = (double)sampleRate;

	size = BLOCK_LEN_LONG;
	for (channel = 0; channel < numChannels; channel++) {
		psyInfo[channel].size = size;

		psyInfo[channel].lastPe = 0.0;
		psyInfo[channel].lastEnr = 0.0;
		psyInfo[channel].threeInARow = 0;
		psyInfo[channel].tonality = (double*)malloc(NPART_LONG*sizeof(double));
		psyInfo[channel].nb = (double*)malloc(NPART_LONG*sizeof(double));
		psyInfo[channel].maskThr = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEn = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrNext = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnNext = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrMS = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnMS = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskThrNextMS = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].maskEnNextMS = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
		psyInfo[channel].prevSamples = (double*)malloc(size*sizeof(double));
		memset(psyInfo[channel].prevSamples, 0, size*sizeof(double));

		psyInfo[channel].lastNb = (double*)malloc(NPART_LONG*sizeof(double));
		psyInfo[channel].lastNbMS = (double*)malloc(NPART_LONG*sizeof(double));
		for (j = 0; j < NPART_LONG; j++) {
			psyInfo[channel].lastNb[j] = 2.;
			psyInfo[channel].lastNbMS[j] = 2.;
		}

		psyInfo[channel].energy = (double*)malloc(size*sizeof(double));
		psyInfo[channel].energyMS = (double*)malloc(size*sizeof(double));
		psyInfo[channel].transBuff = (double*)malloc(2*size*sizeof(double));
	}

	gpsyInfo->psyPart = &psyPartTableLong[sampleRateIdx];
	gpsyInfo->psyPartS = &psyPartTableShort[sampleRateIdx];

	size = BLOCK_LEN_SHORT;
	for (channel = 0; channel < numChannels; channel++) {
		psyInfo[channel].sizeS = size;

		psyInfo[channel].prevSamplesS = (double*)malloc(size*sizeof(double));
		memset(psyInfo[channel].prevSamplesS, 0, size*sizeof(double));

		for (j = 0; j < 8; j++) {
			psyInfo[channel].nbS[j] = (double*)malloc(NPART_SHORT*sizeof(double));
			psyInfo[channel].maskThrS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrNextS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnNextS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrSMS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnSMS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskThrNextSMS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));
			psyInfo[channel].maskEnNextSMS[j] = (double*)malloc(MAX_SCFAC_BANDS*sizeof(double));

			psyInfo[channel].energyS[j] = (double*)malloc(size*sizeof(double));
			psyInfo[channel].energySMS[j] = (double*)malloc(size*sizeof(double));
			psyInfo[channel].transBuffS[j] = (double*)malloc(2*size*sizeof(double));
		}
	}

	size = BLOCK_LEN_LONG;
	high = 0;
	for(b = 0; b < gpsyInfo->psyPart->len; b++) {
		low = high;
		high += gpsyInfo->psyPart->width[b];

		bval[b] = 0.5 * (freq2bark(gpsyInfo->sampleRate*low/(2*size)) + 
			freq2bark(gpsyInfo->sampleRate*(high-1)/(2*size)));
	}

	for(b = 0; b < gpsyInfo->psyPart->len; b++) {
		for(bb = 0; bb < gpsyInfo->psyPart->len; bb++) {
			if (bval[b] >= bval[bb]) tmpx = (bval[b] - bval[bb])*3.0;
			else tmpx = (bval[b] - bval[bb])*1.5;

			if(tmpx >= 0.5 && tmpx <= 2.5)
			{
				tmp = tmpx - 0.5;
				x = 8.0 * (tmp*tmp - 2.0 * tmp);
			} else
				x = 0.0;

			tmpx += 0.474;
			tmpy = 15.811389 + 7.5*tmpx - 17.5*sqrt(1.0+tmpx*tmpx);

			if (tmpy < -100.0) gpsyInfo->spreading[b][bb] = 0.0;
			else gpsyInfo->spreading[b][bb] = exp((x + tmpy)*0.2302585093);
		}
	}
	for(b = 0; b < gpsyInfo->psyPart->len; b++) {
		for(bb = 0; bb < gpsyInfo->psyPart->len; bb++) {
			if (gpsyInfo->spreading[b][bb] != 0.0)
				break;
		}
		gpsyInfo->sprInd[b][0] = bb;
		for(bb = gpsyInfo->psyPart->len-1; bb > 0; bb--) {
			if (gpsyInfo->spreading[b][bb] != 0.0)
				break;
		}
		gpsyInfo->sprInd[b][1] = bb;
	}

    for( b = 0; b < gpsyInfo->psyPart->len; b++){
		tmp = 0.0;
		for( bb = gpsyInfo->sprInd[b][0]; bb < gpsyInfo->sprInd[b][1]; bb++)
			tmp += gpsyInfo->spreading[b][bb];
		for( bb = gpsyInfo->sprInd[b][0]; bb < gpsyInfo->sprInd[b][1]; bb++)
			gpsyInfo->spreading[b][bb] /= tmp;
    }

	j = 0;
    for( b = 0; b < gpsyInfo->psyPart->len; b++){
		gpsyInfo->ath[b] = 1.e37;

		for (bb = 0; bb < gpsyInfo->psyPart->width[b]; bb++, j++) {
			double freq = gpsyInfo->sampleRate*j/(1000.0*2*size);
			double level;
			level = ATHformula(freq*1000) - 20;
			level = pow(10., 0.1*level);
			level *= gpsyInfo->psyPart->width[b];
			if (level < gpsyInfo->ath[b])
				gpsyInfo->ath[b] = level;
		}
    }

	low = 0;
	for (b = 0; b < gpsyInfo->psyPart->len; b++) {
		tmp = freq2bark(gpsyInfo->sampleRate*low/(2*size));
		tmp = (min(tmp, 15.5)/15.5);

		gpsyInfo->mld[b] = pow(10.0, 1.25*(1-cos(M_PI*tmp))-2.5);
		low += gpsyInfo->psyPart->width[b];
	}


	size = BLOCK_LEN_SHORT;
	high = 0;
	for(b = 0; b < gpsyInfo->psyPartS->len; b++) {
		low = high;
		high += gpsyInfo->psyPartS->width[b];

		bval[b] = 0.5 * (freq2bark(gpsyInfo->sampleRate*low/(2*size)) + 
			freq2bark(gpsyInfo->sampleRate*(high-1)/(2*size)));
	}

	for(b = 0; b < gpsyInfo->psyPartS->len; b++) {
		for(bb = 0; bb < gpsyInfo->psyPartS->len; bb++) {
			if (bval[b] >= bval[bb]) tmpx = (bval[b] - bval[bb])*3.0;
			else tmpx = (bval[b] - bval[bb])*1.5;

			if(tmpx >= 0.5 && tmpx <= 2.5)
			{
				tmp = tmpx - 0.5;
				x = 8.0 * (tmp*tmp - 2.0 * tmp);
			} else
				x = 0.0;

			tmpx += 0.474;
			tmpy = 15.811389 + 7.5*tmpx - 17.5*sqrt(1.0+tmpx*tmpx);

			if (tmpy < -100.0) gpsyInfo->spreadingS[b][bb] = 0.0;
			else gpsyInfo->spreadingS[b][bb] = exp((x + tmpy)*0.2302585093);
		}
	}
	for(b = 0; b < gpsyInfo->psyPartS->len; b++) {
		for(bb = 0; bb < gpsyInfo->psyPartS->len; bb++) {
			if (gpsyInfo->spreadingS[b][bb] != 0.0)
				break;
		}
		gpsyInfo->sprIndS[b][0] = bb;
		for(bb = gpsyInfo->psyPartS->len-1; bb > 0; bb--) {
			if (gpsyInfo->spreadingS[b][bb] != 0.0)
				break;
		}
		gpsyInfo->sprIndS[b][1] = bb;
	}

	j = 0;
    for( b = 0; b < gpsyInfo->psyPartS->len; b++){
		gpsyInfo->athS[b] = 1.e37;

		for (bb = 0; bb < gpsyInfo->psyPartS->width[b]; bb++, j++) {
			double freq = gpsyInfo->sampleRate*j/(1000.0*2*size);
			double level;
			level = ATHformula(freq*1000) - 20;
			level = pow(10., 0.1*level);
			level *= gpsyInfo->psyPartS->width[b];
			if (level < gpsyInfo->athS[b])
				gpsyInfo->athS[b] = level;
		}
    }

    for( b = 0; b < gpsyInfo->psyPartS->len; b++){
		tmp = 0.0;
		for( bb = gpsyInfo->sprIndS[b][0]; bb < gpsyInfo->sprIndS[b][1]; bb++)
			tmp += gpsyInfo->spreadingS[b][bb];

		/* SNR formula */
		if (bval[b] < 13) SNR = -8.25;
		else SNR = -4.5 * (bval[b]-13)/(24.0-13.0) +
			-8.25*(bval[b]-24)/(13.0-24.0);
		SNR = pow(10.0, SNR/10.0);

		for( bb = gpsyInfo->sprIndS[b][0]; bb < gpsyInfo->sprIndS[b][1]; bb++)
			gpsyInfo->spreadingS[b][bb] *= SNR / tmp;
    }

	low = 0;
	for (b = 0; b < gpsyInfo->psyPartS->len; b++) {
		tmp = freq2bark(gpsyInfo->sampleRate*low/(2*size));
		tmp = (min(tmp, 15.5)/15.5);

		gpsyInfo->mldS[b] = pow(10.0, 1.25*(1-cos(M_PI*tmp))-2.5);
		low += gpsyInfo->psyPartS->width[b];
	}
}

void PsyEnd(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels)
{
	unsigned int channel;
	int j;

	if (gpsyInfo->ath) free(gpsyInfo->ath);
	if (gpsyInfo->athS) free(gpsyInfo->athS);
	if (gpsyInfo->mld) free(gpsyInfo->mld);
	if (gpsyInfo->mldS) free(gpsyInfo->mldS);
	if (gpsyInfo->window) free(gpsyInfo->window);
	if (gpsyInfo->windowS) free(gpsyInfo->windowS);

	for (channel = 0; channel < numChannels; channel++) {
		if (psyInfo[channel].nb) free(psyInfo[channel].nb);
		if (psyInfo[channel].tonality) free(psyInfo[channel].tonality);
		if (psyInfo[channel].prevSamples) free(psyInfo[channel].prevSamples);
		if (psyInfo[channel].maskThr) free(psyInfo[channel].maskThr);
		if (psyInfo[channel].maskEn) free(psyInfo[channel].maskEn);
		if (psyInfo[channel].maskThrNext) free(psyInfo[channel].maskThrNext);
		if (psyInfo[channel].maskEnNext) free(psyInfo[channel].maskEnNext);
		if (psyInfo[channel].maskThrMS) free(psyInfo[channel].maskThrMS);
		if (psyInfo[channel].maskEnMS) free(psyInfo[channel].maskEnMS);
		if (psyInfo[channel].maskThrNextMS) free(psyInfo[channel].maskThrNextMS);
		if (psyInfo[channel].maskEnNextMS) free(psyInfo[channel].maskEnNextMS);
		
		if (psyInfo[channel].lastNb) free(psyInfo[channel].lastNb);
		if (psyInfo[channel].lastNbMS) free(psyInfo[channel].lastNbMS);

		if (psyInfo[channel].energy) free(psyInfo[channel].energy);
		if (psyInfo[channel].energyMS) free(psyInfo[channel].energyMS);
		if (psyInfo[channel].transBuff) free(psyInfo[channel].transBuff);
	}

	for (channel = 0; channel < numChannels; channel++) {
		if(psyInfo[channel].prevSamplesS) free(psyInfo[channel].prevSamplesS);
		for (j = 0; j < 8; j++) {
			if (psyInfo[channel].nbS[j]) free(psyInfo[channel].nbS[j]);
			if (psyInfo[channel].maskThrS[j]) free(psyInfo[channel].maskThrS[j]);
			if (psyInfo[channel].maskEnS[j]) free(psyInfo[channel].maskEnS[j]);
			if (psyInfo[channel].maskThrNextS[j]) free(psyInfo[channel].maskThrNextS[j]);
			if (psyInfo[channel].maskEnNextS[j]) free(psyInfo[channel].maskEnNextS[j]);
			if (psyInfo[channel].maskThrSMS[j]) free(psyInfo[channel].maskThrSMS[j]);
			if (psyInfo[channel].maskEnSMS[j]) free(psyInfo[channel].maskEnSMS[j]);
			if (psyInfo[channel].maskThrNextSMS[j]) free(psyInfo[channel].maskThrNextSMS[j]);
			if (psyInfo[channel].maskEnNextSMS[j]) free(psyInfo[channel].maskEnNextSMS[j]);

			if (psyInfo[channel].energyS[j]) free(psyInfo[channel].energyS[j]);
			if (psyInfo[channel].energySMS[j]) free(psyInfo[channel].energySMS[j]);
			if (psyInfo[channel].transBuffS[j]) free(psyInfo[channel].transBuffS[j]);
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

				PsyBufferUpdateMS(gpsyInfo, &psyInfo[leftChan], &psyInfo[rightChan]);

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
			inSamples[i] *= gpsyInfo->window[i];
	} else {
		for(i = 0; i < size; i++)
			inSamples[i] *= gpsyInfo->windowS[i];
	}
}

void PsyBufferUpdate(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, double *newSamples)
{
	int i, j;
	double a, b;
	double temp[2048];

	memcpy(psyInfo->transBuff, psyInfo->prevSamples, psyInfo->size*sizeof(double));
	memcpy(psyInfo->transBuff + psyInfo->size, newSamples, psyInfo->size*sizeof(double));


	Hann(gpsyInfo, psyInfo->transBuff, 2*psyInfo->size);
	rsfft(psyInfo->transBuff, 11);

	/* Calculate magnitude of new data */
	for (i = 0; i < psyInfo->size; i++) {
		a = psyInfo->transBuff[i];
		b = psyInfo->transBuff[i+psyInfo->size];
		psyInfo->energy[i] = 0.5 * (a*a + b*b);
	}

	memcpy(temp, psyInfo->prevSamples, psyInfo->size*sizeof(double));
	memcpy(temp + psyInfo->size, newSamples, psyInfo->size*sizeof(double));

	for (j = 0; j < 8; j++) {

		memcpy(psyInfo->transBuffS[j], temp+(j*128)+(1024-128), 2*psyInfo->sizeS*sizeof(double));

		Hann(gpsyInfo, psyInfo->transBuffS[j], 2*psyInfo->sizeS);
		rsfft(psyInfo->transBuffS[j], 8);

		/* Calculate magnitude of new data */
		for(i = 0; i < psyInfo->sizeS; i++){
			a = psyInfo->transBuffS[j][i];
			b = psyInfo->transBuffS[j][i+psyInfo->sizeS];
			psyInfo->energyS[j][i] = 0.5 * (a*a + b*b);
		}
	}

	memcpy(psyInfo->prevSamples, newSamples, psyInfo->size*sizeof(double));
}

void PsyBufferUpdateMS(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfoL, PsyInfo *psyInfoR)
{
	int i, j;
	double a, b;
	double dataL[2048], dataR[2048];

	for (i = 0; i < psyInfoL->size*2; i++) {
		a = psyInfoL->transBuff[i];
		b = psyInfoR->transBuff[i];
		dataL[i] = (a+b)*SQRT2*0.5;
		dataR[i] = (a-b)*SQRT2*0.5;
	}

	/* Calculate magnitude of new data */
	for (i = 0; i < psyInfoL->size; i++) {
		a = dataL[i];
		b = dataL[i+psyInfoL->size];
		psyInfoL->energyMS[i] = 0.5 * (a*a + b*b);

		a = dataR[i];
		b = dataR[i+psyInfoL->size];
		psyInfoR->energyMS[i] = 0.5 * (a*a + b*b);
	}

	for (j = 0; j < 8; j++) {

		for (i = 0; i < psyInfoL->sizeS*2; i++) {
			a = psyInfoL->transBuffS[j][i];
			b = psyInfoR->transBuffS[j][i];
			dataL[i] = (a+b)*SQRT2*0.5;
			dataR[i] = (a-b)*SQRT2*0.5;
		}

		/* Calculate magnitude of new data */
		for (i = 0; i < psyInfoL->sizeS; i++) {
			a = dataL[i];
			b = dataL[i+psyInfoL->sizeS];
			psyInfoL->energySMS[j][i] = 0.5 * (a*a + b*b);

			a = dataR[i];
			b = dataR[i+psyInfoL->sizeS];
			psyInfoR->energySMS[j][i] = 0.5 * (a*a + b*b);
		}
	}
}

/* addition of simultaneous masking */
__inline double mask_add(double m1, double m2, int k, int b, double *ath)
{
	static const double table1[] = {
		3.3246 *3.3246 ,3.23837*3.23837,3.15437*3.15437,3.00412*3.00412,2.86103*2.86103,2.65407*2.65407,2.46209*2.46209,2.284  *2.284  ,
		2.11879*2.11879,1.96552*1.96552,1.82335*1.82335,1.69146*1.69146,1.56911*1.56911,1.46658*1.46658,1.37074*1.37074,1.31036*1.31036,
		1.25264*1.25264,1.20648*1.20648,1.16203*1.16203,1.12765*1.12765,1.09428*1.09428,1.0659 *1.0659 ,1.03826*1.03826,1.01895*1.01895,
		1
	};

	static const double table2[] = {
		1.33352*1.33352,1.35879*1.35879,1.38454*1.38454,1.39497*1.39497,1.40548*1.40548,1.3537 *1.3537 ,1.30382*1.30382,1.22321*1.22321,
		1.14758*1.14758
	};

	static const double table3[] = {
		2.35364*2.35364,2.29259*2.29259,2.23313*2.23313,2.12675*2.12675,2.02545*2.02545,1.87894*1.87894,1.74303*1.74303,1.61695*1.61695,
		1.49999*1.49999,1.39148*1.39148,1.29083*1.29083,1.19746*1.19746,1.11084*1.11084,1.03826*1.03826
	};


	int i;
	double m;

	if (m1 == 0) return m2;

	if (b < 0) b = -b;

	i = (int)(10*log10(m2 / m1)/10*16);
	m = 10*log10((m1+m2)/ath[k]);

	if (i < 0) i = -i;

	if (b <= 3) { /* approximately, 1 bark = 3 partitions */
		if (i > 8) return m1+m2;
		return (m1+m2)*table2[i];
	}

	if (m<15) {
		if (m > 0) {
			double f=1.0,r;
			if (i > 24) return m1+m2;
			if (i > 13) f = 1; else f = table3[i];
			r = (m-0)/15;
			return (m1+m2)*(table1[i]*r+f*(1-r));
		}
		if (i > 13) return m1+m2;
		return (m1+m2)*table3[i];
	}

	if (i > 24) return m1+m2;
	return (m1+m2)*table1[i];
}

static void PsyThreshold(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, int *cb_width_long,
						 int num_cb_long, int *cb_width_short, int num_cb_short)
{
	int b, bb, w, low, high, j;
	double tmp, ecb;

	double e[MAX_NPART];
	double c[MAX_NPART];
	double maxi[MAX_NPART];
	double avg[MAX_NPART];
	double eb;

	double nb_tmp[1024], epart, npart;

	double tot, mx, estot[8];
	double pe = 0.0;

	/* Energy in each partition and weighted unpredictability */
	high = 0;
	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		double m, a;
		low = high;
		high += gpsyInfo->psyPart->width[b];

		eb = psyInfo->energy[low];
		m = a = eb;

		for (w = low+1; w < high; w++)
		{
			double el = psyInfo->energy[w];
			eb += el;
			a += el;
			m = m < el ? el : m;
		}
		e[b] = eb;
		maxi[b] = m;
		avg[b] = a / gpsyInfo->psyPart->width[b];
	}

	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		static double tab[20] = {
			1,0.79433,0.63096,0.63096,0.63096,0.63096,0.63096,0.25119,0.11749,0.11749,
			0.11749,0.11749,0.11749,0.11749,0.11749,0.11749,0.11749,0.11749,0.11749,0.11749
		};
		int c1,c2,t;
		double m, a, tonality;

		c1 = c2 = 0;
		m = a = 0;
		for(w = b-1; w <= b+1; w++)
		{
			if (w >= 0 && w < gpsyInfo->psyPart->len) {
				c1++;
				c2 += gpsyInfo->psyPart->width[w];
				a += avg[w];
				m = m < maxi[w] ? maxi[w] : m;
			}
		}

		a /= c1;
		tonality = (a == 0) ? 0 : (m / a - 1)/(c2-1);

		t = (int)(20*tonality);
		if (t > 19) t = 19;
		psyInfo->tonality[b] = tab[t];
		c[b] = e[b] * tab[t];
	}

	/* Convolve the partitioned energy and unpredictability
	   with the spreading function */
	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		ecb = 0;
		for (bb = gpsyInfo->sprInd[b][0]; bb < gpsyInfo->sprInd[b][1]; bb++)
		{
			ecb = mask_add(ecb, gpsyInfo->spreading[b][bb] * c[bb], bb, bb-b, gpsyInfo->ath);
		}
		ecb *= 0.158489319246111;

		/* Actual energy threshold */
		psyInfo->nb[b] = NS_INTERP(min(ecb, 2*psyInfo->lastNb[b]), ecb, 1/*pcfact*/);
//		psyInfo->nb[b] = max(psyInfo->nb[b], gpsyInfo->ath[b]);
		psyInfo->lastNb[b] = ecb;

		/* Perceptual entropy */
		tmp = gpsyInfo->psyPart->width[b]
			* log((psyInfo->nb[b] + 0.0000000001)
			/ (e[b] + 0.0000000001));
		tmp = min(0,tmp);

		pe -= tmp;
	}

	high = 0;
	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		low = high;
		high += gpsyInfo->psyPart->width[b];

		for (w = low; w < high; w++)
		{
			nb_tmp[w] = psyInfo->nb[b] / gpsyInfo->psyPart->width[b];
		}
	}

	high = 0;
	for (b = 0; b < num_cb_long; b++)
	{
		low = high;
		high += cb_width_long[b];

		epart = psyInfo->energy[low];
		npart = nb_tmp[low];
		for (w = low+1; w < high; w++)
		{
			epart += psyInfo->energy[w];

			if (nb_tmp[w] < npart)
				npart = nb_tmp[w];
		}
		npart *= cb_width_long[b];

		psyInfo->maskThr[b] = psyInfo->maskThrNext[b];
		psyInfo->maskEn[b] = psyInfo->maskEnNext[b];
		tmp = npart / epart;
		psyInfo->maskThrNext[b] = npart;
		psyInfo->maskEnNext[b] = epart;
	}

	/* Short windows */
	for (j = 0; j < 8; j++)
	{
		/* Energy in each partition and weighted unpredictability */
		high = 0;
		for (b = 0; b < gpsyInfo->psyPartS->len; b++)
		{
			low = high;
			high += gpsyInfo->psyPartS->width[b];

			eb = psyInfo->energyS[j][low];

			for (w = low+1; w < high; w++)
			{
				double el = psyInfo->energyS[j][w];
				eb += el;
			}
			e[b] = eb;
		}

		estot[j] = 0.0;

		/* Convolve the partitioned energy and unpredictability
		with the spreading function */
		for (b = 0; b < gpsyInfo->psyPartS->len; b++)
		{
			ecb = 0;
			for (bb = gpsyInfo->sprIndS[b][0]; bb <= gpsyInfo->sprIndS[b][1]; bb++)
			{
				ecb += gpsyInfo->spreadingS[b][bb] * e[bb];
			}

			/* Actual energy threshold */
			psyInfo->nbS[j][b] = max(1e-6, ecb);
//			psyInfo->nbS[j][b] = max(psyInfo->nbS[j][b], gpsyInfo->athS[b]);

			estot[j] += e[b];
		}

		if (estot[j] != 0.0)
			estot[j] /= gpsyInfo->psyPartS->len;

		high = 0;
		for (b = 0; b < gpsyInfo->psyPartS->len; b++)
		{
			low = high;
			high += gpsyInfo->psyPartS->width[b];

			for (w = low; w < high; w++)
			{
				nb_tmp[w] = psyInfo->nbS[j][b] / gpsyInfo->psyPartS->width[b];
			}
		}

		high = 0;
		for (b = 0; b < num_cb_short; b++)
		{
			low = high;
			high += cb_width_short[b];

			epart = psyInfo->energyS[j][low];
			npart = nb_tmp[low];
			for (w = low+1; w < high; w++)
			{
				epart += psyInfo->energyS[j][w];

				if (nb_tmp[w] < npart)
					npart = nb_tmp[w];
			}
			npart *= cb_width_short[b];

			psyInfo->maskThrS[j][b] = psyInfo->maskThrNextS[j][b];
			psyInfo->maskEnS[j][b] = psyInfo->maskEnNextS[j][b];
			psyInfo->maskThrNextS[j][b] = npart;
			psyInfo->maskEnNextS[j][b] = epart;
		}
	}

	tot = mx = estot[0];
	for (j = 1; j < 8; j++) {
		tot += estot[j];
		mx = max(mx, estot[j]);
	}

#ifdef _DEBUG
	printf("%4f %2.2f ", pe, mx/tot);
#endif

	tot = max(tot, 1.e-12);
	if (((mx/tot) > 0.35) && (pe > 1800.0) || ((mx/tot) > 0.5) || (pe > 3000.0)) {
		psyInfo->block_type = ONLY_SHORT_WINDOW;
		psyInfo->threeInARow++;
	} else if ((psyInfo->lastEnr > 0.5) || (psyInfo->lastPe > 3000.0)) {
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
	double ecb, tmp1, tmp2;

	double nb_tmpM[1024];
	double nb_tmpS[1024];
	double epartM, epartS, npartM, npartS;

	double nbM[MAX_NPART];
	double nbS[MAX_NPART];
	double eM[MAX_NPART];
	double eS[MAX_NPART];
	double cM[MAX_NPART];
	double cS[MAX_NPART];

	double x1, x2, db, mld;

#ifdef _DEBUG
	int ms_used = 0;
	int ms_usedS = 0;
#endif

	/* Energy in each partition and weighted unpredictability */
	high = 0;
	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		double mid, side, ebM, ebS;
		low = high;
		high += gpsyInfo->psyPart->width[b];

		mid  = psyInfoL->energyMS[low];
		side = psyInfoR->energyMS[low];

		ebM = mid;
		ebS = side;

		for (w = low+1; w < high; w++)
		{
			mid  = psyInfoL->energyMS[w];
			side = psyInfoR->energyMS[w];

			ebM += mid;
			ebS += side;
		}
		eM[b] = ebM;
		eS[b] = ebS;
		cM[b] = ebM * min(psyInfoL->tonality[b], psyInfoR->tonality[b]);
		cS[b] = ebS * min(psyInfoL->tonality[b], psyInfoR->tonality[b]);
	}

	/* Convolve the partitioned energy and unpredictability
	   with the spreading function */
	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		/* Mid channel */

		ecb = 0;
		for (bb = gpsyInfo->sprInd[b][0]; bb <= gpsyInfo->sprInd[b][1]; bb++)
		{
			ecb = mask_add(ecb, gpsyInfo->spreading[bb][b] * cM[bb], bb, bb-b, gpsyInfo->ath);
		}
		ecb *= 0.158489319246111;

		/* Actual energy threshold */
		nbM[b] = NS_INTERP(min(ecb, 2*psyInfoL->lastNbMS[b]), ecb, 1/*pcfact*/);
//		nbM[b] = max(nbM[b], gpsyInfo->ath[b]);
		psyInfoL->lastNbMS[b] = ecb;


		/* Side channel */

		ecb = 0;
		for (bb = gpsyInfo->sprInd[b][0]; bb <= gpsyInfo->sprInd[b][1]; bb++)
		{
			ecb = mask_add(ecb, gpsyInfo->spreading[bb][b] * cS[bb], bb, bb-b, gpsyInfo->ath);
		}
		ecb *= 0.158489319246111;

		/* Actual energy threshold */
		nbS[b] = NS_INTERP(min(ecb, 2*psyInfoR->lastNbMS[b]), ecb, 1/*pcfact*/);
//		nbS[b] = max(nbS[b], gpsyInfo->ath[b]);
		psyInfoR->lastNbMS[b] = ecb;

		if (psyInfoL->nb[b] <= 1.58*psyInfoR->nb[b]
			&& psyInfoR->nb[b] <= 1.58*psyInfoL->nb[b]) {

			mld = gpsyInfo->mld[b]*eM[b];
			tmp1 = max(nbM[b], min(nbS[b],mld));

			mld = gpsyInfo->mld[b]*eS[b];
			tmp2 = max(nbS[b], min(nbM[b],mld));

			nbM[b] = tmp1;
			nbS[b] = tmp2;
		}
	}

	high = 0;
	for (b = 0; b < gpsyInfo->psyPart->len; b++)
	{
		low = high;
		high += gpsyInfo->psyPart->width[b];

		for (w = low; w < high; w++)
		{
			nb_tmpM[w] = nbM[b] / gpsyInfo->psyPart->width[b];
			nb_tmpS[w] = nbS[b] / gpsyInfo->psyPart->width[b];
		}
	}

	high = 0;
	for (b = 0; b < num_cb_long; b++)
	{
		low = high;
		high += cb_width_long[b];

		epartM = psyInfoL->energyMS[low];
		npartM = nb_tmpM[low];
		epartS = psyInfoR->energyMS[low];
		npartS = nb_tmpS[low];

		for (w = low+1; w < high; w++)
		{
			epartM += psyInfoL->energyMS[w];
			epartS += psyInfoR->energyMS[w];

			if (nb_tmpM[w] < npartM)
				npartM = nb_tmpM[w];
			if (nb_tmpS[w] < npartS)
				npartS = nb_tmpS[w];
		}
		npartM *= cb_width_long[b];
		npartS *= cb_width_long[b];

		psyInfoL->maskThrMS[b] = psyInfoL->maskThrNextMS[b];
		psyInfoR->maskThrMS[b] = psyInfoR->maskThrNextMS[b];
		psyInfoL->maskEnMS[b] = psyInfoL->maskEnNextMS[b];
		psyInfoR->maskEnMS[b] = psyInfoR->maskEnNextMS[b];
		psyInfoL->maskThrNextMS[b] = npartM;
		psyInfoR->maskThrNextMS[b] = npartS;
		psyInfoL->maskEnNextMS[b] = epartM;
		psyInfoR->maskEnNextMS[b] = epartS;

		{
			double thmL = psyInfoL->maskThr[b];
			double thmR = psyInfoR->maskThr[b];
			double thmM = psyInfoL->maskThrMS[b];
			double thmS = psyInfoR->maskThrMS[b];
			double msfix = 3.5;

			if (thmL*msfix < (thmM+thmS)/2) {
				double f = thmL*msfix / ((thmM+thmS)/2);
				thmM *= f;
				thmS *= f;
			}
			if (thmR*msfix < (thmM+thmS)/2) {
				double f = thmR*msfix / ((thmM+thmS)/2);
				thmM *= f;
				thmS *= f;
			}

			psyInfoL->maskThrMS[b] = min(thmM,psyInfoL->maskThrMS[b]);
			psyInfoR->maskThrMS[b] = min(thmS,psyInfoR->maskThrMS[b]);
			channelInfoL->msInfo.ms_used[b] = 1;
		}

#if 0
		x1 = min(npartM, npartS);
		x2 = max(npartM, npartS);
		/* thresholds difference in db */
		if (x2 >= 1000*x1) db=3;
		else db = log10(x2/x1);  
		if (db < 0.05) {
#ifdef _DEBUG
			ms_used++;
#endif
			channelInfoL->msInfo.ms_used[b] = 1;
		} else {
			channelInfoL->msInfo.ms_used[b] = 0;
		}
#endif
	}


#ifdef _DEBUG
	printf("MSL:%3d ", ms_used);
#endif

	/* Short windows */
	for (j = 0; j < 8; j++)
	{
		/* Energy in each partition and weighted unpredictability */
		high = 0;
		for (b = 0; b < gpsyInfo->psyPartS->len; b++)
		{
			double ebM, ebS;
			low = high;
			high += gpsyInfo->psyPartS->width[b];

			ebM = psyInfoL->energySMS[j][low];
			ebS = psyInfoR->energySMS[j][low];

			for (w = low+1; w < high; w++)
			{
				ebM += psyInfoL->energySMS[j][w];
				ebS += psyInfoR->energySMS[j][w];
			}
			eM[b] = ebM;
			eS[b] = ebS;
		}

		/* Convolve the partitioned energy and unpredictability
		with the spreading function */
		for (b = 0; b < gpsyInfo->psyPartS->len; b++)
		{
			/* Mid channel */

			/* Get power ratio */
			ecb = 0;
			for (bb = gpsyInfo->sprIndS[b][0]; bb <= gpsyInfo->sprIndS[b][1]; bb++)
			{
				ecb += gpsyInfo->spreadingS[b][bb] * eM[bb];
			}

			/* Actual energy threshold */
			nbM[b] = max(1e-6, ecb);
//			nbM[b] = max(nbM[b], gpsyInfo->athS[b]);


			/* Side channel */

			/* Get power ratio */
			ecb = 0;
			for (bb = gpsyInfo->sprIndS[b][0]; bb <= gpsyInfo->sprIndS[b][1]; bb++)
			{
				ecb += gpsyInfo->spreadingS[b][bb] * eS[bb];
			}

			/* Actual energy threshold */
			nbS[b] = max(1e-6, ecb);
//			nbS[b] = max(nbS[b], gpsyInfo->athS[b]);

			if (psyInfoL->nbS[j][b] <= 1.58*psyInfoR->nbS[j][b]
				&& psyInfoR->nbS[j][b] <= 1.58*psyInfoL->nbS[j][b]) {

				mld = gpsyInfo->mldS[b]*eM[b];
				tmp1 = max(nbM[b], min(nbS[b],mld));

				mld = gpsyInfo->mldS[b]*eS[b];
				tmp2 = max(nbS[b], min(nbM[b],mld));

				nbM[b] = tmp1;
				nbS[b] = tmp2;
			}
		}

		high = 0;
		for (b = 0; b < gpsyInfo->psyPartS->len; b++)
		{
			low = high;
			high += gpsyInfo->psyPartS->width[b];

			for (w = low; w < high; w++)
			{
				nb_tmpM[w] = nbM[b] / gpsyInfo->psyPartS->width[b];
				nb_tmpS[w] = nbS[b] / gpsyInfo->psyPartS->width[b];
			}
		}

		high = 0;
		for (b = 0; b < num_cb_short; b++)
		{
			low = high;
			high += cb_width_short[b];

			epartM = psyInfoL->energySMS[j][low];
			epartS = psyInfoR->energySMS[j][low];
			npartM = nb_tmpM[low];
			npartS = nb_tmpS[low];

			for (w = low+1; w < high; w++)
			{
				epartM += psyInfoL->energySMS[j][w];
				epartS += psyInfoR->energySMS[j][w];

				if (nb_tmpM[w] < npartM)
					npartM = nb_tmpM[w];
				if (nb_tmpS[w] < npartS)
					npartS = nb_tmpS[w];
			}
			npartM *= cb_width_short[b];
			npartS *= cb_width_short[b];

			psyInfoL->maskThrSMS[j][b] = psyInfoL->maskThrNextSMS[j][b];
			psyInfoR->maskThrSMS[j][b] = psyInfoR->maskThrNextSMS[j][b];
			psyInfoL->maskEnSMS[j][b] = psyInfoL->maskEnNextSMS[j][b];
			psyInfoR->maskEnSMS[j][b] = psyInfoR->maskEnNextSMS[j][b];
			psyInfoL->maskThrNextSMS[j][b] = npartM;
			psyInfoR->maskThrNextSMS[j][b] = npartS;
			psyInfoL->maskEnNextSMS[j][b] = epartM;
			psyInfoR->maskEnNextSMS[j][b] = epartS;

			{
				double thmL = psyInfoL->maskThrS[j][b];
				double thmR = psyInfoR->maskThrS[j][b];
				double thmM = psyInfoL->maskThrSMS[j][b];
				double thmS = psyInfoR->maskThrSMS[j][b];
				double msfix = 3.5;

				if (thmL*msfix < (thmM+thmS)/2) {
					double f = thmL*msfix / ((thmM+thmS)/2);
					thmM *= f;
					thmS *= f;
				}
				if (thmR*msfix < (thmM+thmS)/2) {
					double f = thmR*msfix / ((thmM+thmS)/2);
					thmM *= f;
					thmS *= f;
				}

				psyInfoL->maskThrSMS[j][b] = min(thmM,psyInfoL->maskThrSMS[j][b]);
				psyInfoR->maskThrSMS[j][b] = min(thmS,psyInfoR->maskThrSMS[j][b]);
				channelInfoL->msInfo.ms_usedS[j][b] = 1;
			}

#if 0
			x1 = min(npartM, npartS);
			x2 = max(npartM, npartS);
			/* thresholds difference in db */
			if (x2 >= 1000*x1) db = 3;
			else db = log10(x2/x1);
			if (db < 0.05) {
#ifdef _DEBUG
				ms_usedS++;
#endif
				channelInfoL->msInfo.ms_usedS[j][b] = 1;
			} else {
				channelInfoL->msInfo.ms_usedS[j][b] = 0;
			}
#endif
		}
	}

#ifdef _DEBUG
	printf("MSS:%3d ", ms_usedS);
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

#ifdef _DEBUG
	printf("%s ", (coderInfo[0].block_type == ONLY_SHORT_WINDOW) ? "SHORT" : "LONG ");
#endif
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
	f /= 1000;  /* convert to khz */
	f  = max(0.01, f);
	f  = min(18.0,f);

	/* from Painter & Spanias, 1997 */
	/* modified by Gabriel Bouvigne to better fit to the reality */
	ath =    3.640 * pow(f,-0.8)
		- 6.800 * exp(-0.6*pow(f-3.4,2.0))
		+ 6.000 * exp(-0.15*pow(f-8.7,2.0))
		+ 0.6* 0.001 * pow(f,4.0);
	return ath;
}

static PsyPartTable psyPartTableLong[12+1] =
{
  { 96000, 71,
     { /* width */
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,
      3,3,3,3,3,4,4,4,5,5,5,6,6,7,7,8,8,9,10,10,11,12,13,14,15,16,
      18,19,21,24,26,30,34,39,45,53,64,78,98,127,113
     }
  },
  { 88200, 72,
     { /* width */
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,
      3,3,3,3,3,4,4,4,4,5,5,5,6,6,7,7,8,8,9,10,10,11,12,13,14,15,
      16,18,19,21,23,26,29,32,37,42,49,58,69,85,106,137,35
     }
  },
  { 64000, 67,
     { /* width */
      2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,
      4,4,4,4,5,5,5,6,6,7,7,8,8,9,10,10,11,12,13,14,15,16,17,
      18,20,21,23,25,28,30,34,37,42,47,54,63,73,87,105,57
     }
  },
  { 48000, 69,
     { /* width */
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
      3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 10, 11, 12,
      13, 14, 15, 16, 17, 18, 20, 21, 23, 24, 26, 28, 31, 34, 37, 40, 45, 50,
      56, 63, 72, 84, 86
     }
  },
  { 44100, 70,
     { /* width */
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 
      3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 11,
      12, 13, 14, 15, 16, 17, 18, 20, 21, 23, 24, 26, 28, 30, 33, 36, 39,
      43, 47, 53, 59, 67, 76, 88, 27
     }
  },
  { 32000, 66,
     { /* width */
       3,3,3,3,3,3,3,3,3,3,3,
       3,3,3,3,3,3,3,3,4,4,4,
       4,4,4,4,5,5,5,5,6,6,6,
       7,7,8,8,9,10,10,11,12,13,14,
       15,16,17,19,20,22,23,25,27,29,31,
       33,35,38,41,45,48,53,58,64,71,62
     }
  },
  { 24000, 66,
     { /* width */
       3,3,3,3,3,3,3,3,3,3,3,
       4,4,4,4,4,4,4,4,4,4,4,
       5,5,5,5,5,6,6,6,6,7,7,
       7,8,8,9,9,10,11,12,12,13,14,
       15,17,18,19,21,22,24,26,28,30,32,
       34,37,39,42,45,49,53,57,62,67,34
     }
  },
  { 22050, 63,
     { /* width */
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5,
      6, 6, 6, 6, 7, 7, 7, 8, 8, 9, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17,
      19, 20, 22, 23, 25, 27, 29, 31, 33, 36, 38, 41, 44, 47, 51, 55, 59,
      64, 61
     }
  },
  { 16000, 60,
     { /* width */
       5,5,5,5,5,5,5,5,5,5,
       5,5,5,5,5,6,6,6,6,6,
       6,6,7,7,7,7,8,8,8,9,
       9,10,10,11,11,12,13,14,15,16,
       17,18,19,21,22,24,26,28,30,33,
       35,38,41,44,47,50,54,58,62,58
     }
  },
  { 12000, 57,
     { /* width */
       6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,
       8,8,8,8,8,9,9,9,10,10,11,11,12,12,13,13,
       14,15,16,17,18,19,20,22,23,25,27,29,31,
       34,36,39,42,45,49,53,57,61,58 
    }
  },
  { 11025, 56,
     { /* width */
       7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,
       9,9,9,9,10,10,10,11,11,12,12,13,13,14,15,16,17,18,19,20,
       21,23,24,26,28,30,33,35,38,41,44,48,51,55,59,64,9
     }
  },
  { 8000, 52,
     { /* width */
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11,
      12, 12, 12, 13, 13, 14, 14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 23, 24,
      26, 27, 29, 31, 33, 36, 38, 41, 44, 48, 52, 56, 60, 14
     }
  },
  { -1 }
};

static PsyPartTable psyPartTableShort[12+1] =
{
  { 96000, 36,
     { /* width */
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,4,4,5,5,
      6,7,9,11,14,18,7
     }
   },
  { 88200, 37,
    { /* width */
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,4,4,
      5,5,6,7,8,10,12,16,1
     }
  },
  { 64000, 39,
     { /* width */
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,4,4,4,
      5,5,6,7,8,9,11,13,10
     }
  },
  { 48000, 42,
    { /* width */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
      2, 3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10, 12, 1
     }
  },
  { 44100, 42, 
    { /* width */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
      2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10, 12
     }
  },
  { 32000, 44,
     { /* width */
       1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
       2,2,2,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,6,6,7,8,8,9,8
     }
  },
  { 24000, 46,
     { /* width */
       1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
       2,2,2,2,2,2,2,3,3,3,3,3,4,4,4,5,5,5,6,6,7,7,8,8,9,1
     }
  },
  { 22050, 46,
     { /* width */
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
      2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 7
     }
  },
  { 16000, 47,
     { /* width */
       1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
       2,2,2,2,2,2,2,2,3,3,3,3,3,4,4,4,5,5,5,6,6,7,7,8,8,7
     }
  },
  { 12000, 48,
     { /* width */
       1,1,1,1,1,1,1,1,1,1,1,1,
       1,1,1,1,1,1,1,2,2,2,2,2,
       2,2,2,2,2,2,3,3,3,3,3,4,
       4,4,5,5,5,6,6,7,7,8,8,3
     }
  },
  { 11025, 47,
     { /* width */
       1,1,1,1,1,1,1,1,1,1,
       1,1,1,1,1,1,1,1,2,2,
       2,2,2,2,2,2,2,2,2,3,
       3,3,3,3,4,4,4,4,5,5,
       5,6,6,7,7,8,8
     }
  },
  { 8000, 40,
    { /* width */
     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
     3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 3
    }
  },
  { -1 }
};
