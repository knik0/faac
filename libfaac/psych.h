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
 * $Id: psych.h,v 1.1 2001/01/17 11:21:40 menno Exp $
 */

#ifndef PSYCH_H
#define PSYCH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif

#include "coder.h"
#include "channels.h"

typedef struct {
	int size;
	int sizeS;

	/* Previous input samples */
	double *prevSamples;
	double *prevSamplesS;

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
	int block_type;

	/* Final threshold values */
	double *maskThr;
	double *maskEn;
	double *maskThrS[8];
	double *maskEnS[8];
	double *maskThrNext;
	double *maskEnNext;
	double *maskThrNextS[8];
	double *maskEnNextS[8];

	double *lastNb;
	double *lastNbMS;

	double *maskThrMS;
	double *maskEnMS;
	double *maskThrSMS[8];
	double *maskEnSMS[8];
	double *maskThrNextMS;
	double *maskEnNextMS;
	double *maskThrNextSMS[8];
	double *maskEnNextSMS[8];
} PsyInfo;

typedef struct {
	double sampleRate;

	/* Hann window */
	double *hannWindow;
	double *hannWindowS;

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
} GlobalPsyInfo;

void PsyInit(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels,
			 unsigned int sampleRate, int *cb_width_long, int num_cb_long,
			 int *cb_width_short, int num_cb_short);
void PsyEnd(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels);
void PsyCalculate(ChannelInfo *channelInfo, GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
				  int *cb_width_long, int num_cb_long, int *cb_width_short,
				  int num_cb_short, unsigned int numChannels);
void PsyBufferUpdate(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, double *newSamples);
void BlockSwitch(CoderInfo *coderInfo, PsyInfo *psyInfo, unsigned int numChannels);

static void Hann(GlobalPsyInfo *gpsyInfo, double *inSamples, int N);
static void PsyUnpredictability(PsyInfo *psyInfo);
static void PsyThreshold(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, int *cb_width_long,
						 int num_cb_long, int *cb_width_short, int num_cb_short);
static void PsyThresholdMS(ChannelInfo *channelInfoL, GlobalPsyInfo *gpsyInfo,
						   PsyInfo *psyInfoL, PsyInfo *psyInfoR, int *cb_width_long,
						   int num_cb_long, int *cb_width_short, int num_cb_short);
static double freq2bark(double freq);
static double ATHformula(double f);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PSYCH_H */