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
 * $Id: psych.h,v 1.3 2001/02/04 17:50:47 oxygene2000 Exp $
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

#define NPART_LONG  72
#define NPART_SHORT 48
#define MAX_NPART   NPART_LONG

typedef struct {
	int sampling_rate;
	int len;
	unsigned char width[MAX_NPART];
} PsyPartTable;

static PsyPartTable psyPartTableLong[12+1];
static PsyPartTable psyPartTableShort[12+1];

typedef struct {
	int size;
	int sizeS;

	/* Previous input samples */
	double *prevSamples;
	double *prevSamplesS;

	/* FFT data */

	/* Magnitude */
	double *energy;
	double *energyS[8];
	double *energyMS;
	double *energySMS[8];
	double *transBuff;
	double *transBuffS[8];

	/* Tonality */
	double *tonality;

	double lastPe;
	double lastEnr;
	int threeInARow;
	int block_type;

	/* Final threshold values */
	double *nb;
	double *nbS[8];
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
	double *window;
	double *windowS;

	/* Stereo demasking thresholds */
	double *mld;
	double *mldS;

	PsyPartTable *psyPart;
	PsyPartTable *psyPartS;

	/* Spreading functions */
	double spreading[NPART_LONG][NPART_LONG];
	double spreadingS[NPART_SHORT][NPART_SHORT];
	int sprInd[NPART_LONG][2];
	int sprIndS[NPART_SHORT][2];

	/* Absolute threshold of hearing */
	double *ath;
	double *athS;
} GlobalPsyInfo;

void PsyInit(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels,
			 unsigned int sampleRate, unsigned int sampleRateIdx);
void PsyEnd(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, unsigned int numChannels);
void PsyCalculate(ChannelInfo *channelInfo, GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
				  int *cb_width_long, int num_cb_long, int *cb_width_short,
				  int num_cb_short, unsigned int numChannels);
void PsyBufferUpdate(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo, double *newSamples);
void PsyBufferUpdateMS(GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfoL, PsyInfo *psyInfoR);
void BlockSwitch(CoderInfo *coderInfo, PsyInfo *psyInfo, unsigned int numChannels);

static void Hann(GlobalPsyInfo *gpsyInfo, double *inSamples, int N);
__inline double mask_add(double m1, double m2, int k, int b, double *ath);
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
