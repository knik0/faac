/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2002 Krzysztof Nikiel
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
 * $Id: psychkni.c,v 1.6 2003/02/09 20:44:16 menno Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "psych.h"
#include "coder.h"
#include "fft.h"
#include "util.h"
#include "frame.h"

#define PREPARELONGFFT 0

typedef double psyfloat;

typedef struct
{
  // bandwidth
  int band;
  int bandS;

  /* FFT data */

  /* energy */
#if PREPARELONGFFT
  psyfloat *fftEnrg;
  psyfloat *fftEnrgNext;
  psyfloat *fftEnrgNext2;
#endif
  psyfloat *fftEnrgS[8];
  psyfloat *fftEnrgNextS[8];
  psyfloat *fftEnrgNext2S[8];
}
psydata_t;


static void Hann(GlobalPsyInfo * gpsyInfo, double *inSamples, int size)
{
  int i;

  /* Applying Hann window */
  if (size == BLOCK_LEN_LONG * 2)
  {
    for (i = 0; i < size; i++)
      inSamples[i] *= gpsyInfo->hannWindow[i];
  }
  else
  {
    for (i = 0; i < size; i++)
      inSamples[i] *= gpsyInfo->hannWindowS[i];
  }
}

static void PsyThreshold(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo, int *cb_width_long,
			 int num_cb_long, int *cb_width_short, int num_cb_short)
{
  int b, j;
  double volb[8][NSFB_SHORT];	// band volume in each window
  double totchg;
  int lastband;
  psydata_t *psydata = psyInfo->data;
  static const double longthr = 0.32;
//  static const double shortthr = longthr * 0.20;
  static const double shortthr = 0.32 * 0.20;
  static const int firstband = 1;


  /* Long window */
  for (b = 0; b < num_cb_long; b++)
  {
    psyInfo->maskEn[b] = 1.0;
    psyInfo->maskThr[b] = longthr;
  }

  /* Short windows */
  for (j = 0; j < 8; j++)
  {
    for (b = 0; b < num_cb_short; b++)
    {
      psyInfo->maskEnS[j][b] = 1.0;
      psyInfo->maskThrS[j][b] = shortthr;
    }
  }

  /* long/short block switch */
  // compute band volume
  for (j = 0; j < 8; j++)
  {
    int l = 0;
    for (b = firstband; b < num_cb_short; b++)
    {
      int last = l + cb_width_short[b];
      double e = 0;

      if (last > psydata->bandS) // band out of range
      {
	volb[j][b] = 0;
	l = last;
	continue;
      }

      for (; l < last; l++)
	e += psydata->fftEnrgS[j][l];
      volb[j][b] = sqrt(e);
    }
  }
  lastband = b;

  // compare volume levels in each band of short widows
  totchg = 0.0;
  for (b = firstband; b < lastband; b++)
  {
    double maxdif = 0;

    for (j = 1; j < 7; j++)
    {
      double slowvol = (1.0 / 7.0) * ((7 - j) * volb[0][b] + j * volb[7][b]);
      double voldif = volb[j][b] / slowvol;

      if (voldif < 1.0)
	voldif = 1.0 / voldif;
      voldif -= 1.0;

      if (voldif > maxdif)
	maxdif = voldif;
    }
    totchg += maxdif;
  }
  totchg = totchg / lastband;

  psyInfo->block_type = (totchg > 0.8) ? ONLY_SHORT_WINDOW : ONLY_LONG_WINDOW;

#if 0
  {
    static int total = 0, shorts = 0;
    char *flash = "    ";

    total++;
    if (psyInfo->block_type == ONLY_SHORT_WINDOW)
    {
      flash = "****";
      shorts++;
    }

    printf("totchg: %s %g\t%g\n", flash, totchg, (double)shorts/total);
  }
#endif
}

static void PsyThresholdMS(ChannelInfo * channelInfoL, GlobalPsyInfo * gpsyInfo,
			   PsyInfo * psyInfoL, PsyInfo * psyInfoR, int
			   *cb_width_long, int num_cb_long, int *cb_width_short,
			   int num_cb_short)
{
  int b, j;

  // do nothing

  // long
  for (b = 0; b < num_cb_long; b++)
  {
    psyInfoL->maskThrMS[b] = 0.7;
    psyInfoR->maskThrMS[b] = 0.7;
    psyInfoL->maskEnMS[b] = 1.0;
    psyInfoR->maskEnMS[b] = 1.0;

    //channelInfoL->msInfo.ms_used[b] = 1;
    channelInfoL->msInfo.ms_used[b] = 0;
  }

  /* Short windows */
  for (j = 0; j < 8; j++)
  {
    for (b = 0; b < num_cb_short; b++)
    {

      psyInfoL->maskThrSMS[j][b] = 0.7;
      psyInfoR->maskThrSMS[j][b] = 0.7;
      psyInfoL->maskEnSMS[j][b] = 1.0;
      psyInfoR->maskEnSMS[j][b] = 1.0;
    }

    //channelInfoL->msInfo.ms_usedS[j][b] = 1;
    channelInfoL->msInfo.ms_usedS[j][b] = 0;
  }
}

static void PsyInit(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo, unsigned int numChannels,
		    unsigned int sampleRate, int *cb_width_long, int num_cb_long,
		    int *cb_width_short, int num_cb_short)
{
  unsigned int channel;
  int i, j, size;

  gpsyInfo->hannWindow =
    (double *) AllocMemory(2 * BLOCK_LEN_LONG * sizeof(double));
  gpsyInfo->hannWindowS =
    (double *) AllocMemory(2 * BLOCK_LEN_SHORT * sizeof(double));

  for (i = 0; i < BLOCK_LEN_LONG * 2; i++)
    gpsyInfo->hannWindow[i] = 0.5 * (1 - cos(2.0 * M_PI * (i + 0.5) /
					     (BLOCK_LEN_LONG * 2)));
  for (i = 0; i < BLOCK_LEN_SHORT * 2; i++)
    gpsyInfo->hannWindowS[i] = 0.5 * (1 - cos(2.0 * M_PI * (i + 0.5) /
					      (BLOCK_LEN_SHORT * 2)));
  gpsyInfo->sampleRate = (double) sampleRate;

  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = AllocMemory(sizeof(psydata_t));
    psyInfo[channel].data = psydata;
  }

  size = BLOCK_LEN_LONG;
  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = psyInfo[channel].data;

    psyInfo[channel].size = size;

    psyInfo[channel].maskThr =
      (double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
    psyInfo[channel].maskEn =
      (double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
    psyInfo[channel].maskThrMS =
      (double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
    psyInfo[channel].maskEnMS =
      (double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
    psyInfo[channel].prevSamples =
      (double *) AllocMemory(size * sizeof(double));
    memset(psyInfo[channel].prevSamples, 0, size * sizeof(double));

#if PREPARELONGFFT
    psydata->fftEnrg = (psyfloat *) AllocMemory(size * sizeof(psyfloat));
    memset(psydata->fftEnrg, 0, size * sizeof(psyfloat));
    psydata->fftEnrgNext = (psyfloat *) AllocMemory(size * sizeof(psyfloat));
    memset(psydata->fftEnrgNext, 0, size * sizeof(psyfloat));
    psydata->fftEnrgNext2 = (psyfloat *) AllocMemory(size * sizeof(psyfloat));
    memset(psydata->fftEnrgNext2, 0, size * sizeof(psyfloat));
#endif
  }

  size = BLOCK_LEN_SHORT;
  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = psyInfo[channel].data;

    psyInfo[channel].sizeS = size;

    psyInfo[channel].prevSamplesS =
      (double *) AllocMemory(size * sizeof(double));
    memset(psyInfo[channel].prevSamplesS, 0, size * sizeof(double));

    for (j = 0; j < 8; j++)
    {
      psyInfo[channel].maskThrS[j] =
	(double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
      psyInfo[channel].maskEnS[j] =
	(double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
      psyInfo[channel].maskThrSMS[j] =
	(double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));
      psyInfo[channel].maskEnSMS[j] =
	(double *) AllocMemory(MAX_SCFAC_BANDS * sizeof(double));

      psydata->fftEnrgS[j] =
	(psyfloat *) AllocMemory(size * sizeof(psyfloat));
      memset(psydata->fftEnrgS[j], 0, size * sizeof(psyfloat));
      psydata->fftEnrgNextS[j] =
	(psyfloat *) AllocMemory(size * sizeof(psyfloat));
      memset(psydata->fftEnrgNextS[j], 0, size * sizeof(psyfloat));
      psydata->fftEnrgNext2S[j] =
	(psyfloat *) AllocMemory(size * sizeof(psyfloat));
      memset(psydata->fftEnrgNext2S[j], 0, size * sizeof(psyfloat));
    }
  }
}

static void PsyEnd(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo, unsigned int numChannels)
{
  unsigned int channel;
  int j;

  if (gpsyInfo->hannWindow)
    FreeMemory(gpsyInfo->hannWindow);
  if (gpsyInfo->hannWindowS)
    FreeMemory(gpsyInfo->hannWindowS);

  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = psyInfo[channel].data;

    if (psyInfo[channel].prevSamples)
      FreeMemory(psyInfo[channel].prevSamples);
    if (psyInfo[channel].maskThr)
      FreeMemory(psyInfo[channel].maskThr);
    if (psyInfo[channel].maskEn)
      FreeMemory(psyInfo[channel].maskEn);
    if (psyInfo[channel].maskThrMS)
      FreeMemory(psyInfo[channel].maskThrMS);
    if (psyInfo[channel].maskEnMS)
      FreeMemory(psyInfo[channel].maskEnMS);

#if PREPARELONGFFT
    if (psydata->fftEnrg)
      FreeMemory(psydata->fftEnrg);
    if (psydata->fftEnrgNext)
      FreeMemory(psydata->fftEnrgNext);
    if (psydata->fftEnrgNext2)
      FreeMemory(psydata->fftEnrgNext2);
#endif
  }

  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = psyInfo[channel].data;

    if (psyInfo[channel].prevSamplesS)
      FreeMemory(psyInfo[channel].prevSamplesS);
    for (j = 0; j < 8; j++)
    {
      if (psyInfo[channel].maskThrS[j])
	FreeMemory(psyInfo[channel].maskThrS[j]);
      if (psyInfo[channel].maskEnS[j])
	FreeMemory(psyInfo[channel].maskEnS[j]);
      if (psyInfo[channel].maskThrSMS[j])
	FreeMemory(psyInfo[channel].maskThrSMS[j]);
      if (psyInfo[channel].maskEnSMS[j])
	FreeMemory(psyInfo[channel].maskEnSMS[j]);

      if (psydata->fftEnrgS[j])
	FreeMemory(psydata->fftEnrgS[j]);
      if (psydata->fftEnrgNextS[j])
	FreeMemory(psydata->fftEnrgNextS[j]);
      if (psydata->fftEnrgNext2S[j])
	FreeMemory(psydata->fftEnrgNext2S[j]);
    }
  }

  for (channel = 0; channel < numChannels; channel++)
  {
    if (psyInfo[channel].data)
      FreeMemory(psyInfo[channel].data);
  }
}

/* Do psychoacoustical analysis */
static void PsyCalculate(ChannelInfo * channelInfo, GlobalPsyInfo * gpsyInfo,
			 PsyInfo * psyInfo, int *cb_width_long, int
			 num_cb_long, int *cb_width_short,
			 int num_cb_short, unsigned int numChannels)
{
  unsigned int channel;

  for (channel = 0; channel < numChannels; channel++)
  {
    if (channelInfo[channel].present)
    {

      if (channelInfo[channel].cpe &&
	  channelInfo[channel].ch_is_left)
      {				/* CPE */

	int leftChan = channel;
	int rightChan = channelInfo[channel].paired_ch;

	/* Calculate the threshold */
	PsyThreshold(gpsyInfo, &psyInfo[leftChan], cb_width_long, num_cb_long,
		     cb_width_short, num_cb_short);
	PsyThreshold(gpsyInfo, &psyInfo[rightChan], cb_width_long, num_cb_long,
		     cb_width_short, num_cb_short);

	/* And for MS */
	PsyThresholdMS(&channelInfo[leftChan], gpsyInfo, &psyInfo[leftChan],
		       &psyInfo[rightChan], cb_width_long, num_cb_long, cb_width_short,
		       num_cb_short);

      }
      else if (!channelInfo[channel].cpe &&
	       channelInfo[channel].lfe)
      {				/* LFE */

	/* NOT FINISHED */

      }
      else if (!channelInfo[channel].cpe)
      {				/* SCE */
	/* Calculate the threshold */
	PsyThreshold(gpsyInfo, &psyInfo[channel], cb_width_long, num_cb_long,
		     cb_width_short, num_cb_short);
      }
    }
  }
}

static void PsyBufferUpdate(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo,
			    double *newSamples, unsigned int bandwidth)
{
  int i, j;
  double a, b;
  double transBuff[2 * BLOCK_LEN_LONG];
  double transBuffS[2 * BLOCK_LEN_SHORT];
  psydata_t *psydata = psyInfo->data;
  psyfloat *tmp;

  psydata->band = psyInfo->size * bandwidth * 2 / gpsyInfo->sampleRate;
  psydata->bandS = psyInfo->sizeS * bandwidth * 2 / gpsyInfo->sampleRate;

#if PREPARELONGFFT
  memcpy(transBuff, psyInfo->prevSamples, psyInfo->size * sizeof(double));
  memcpy(transBuff + psyInfo->size, newSamples, psyInfo->size * sizeof(double));

  Hann(gpsyInfo, transBuff, 2 * psyInfo->size);
  rfft(transBuff, 11);

  // shift bufs
  tmp = psydata->fftEnrg;
  psydata->fftEnrg = psydata->fftEnrgNext;
  psydata->fftEnrgNext = psydata->fftEnrgNext2;
  psydata->fftEnrgNext2 = tmp;

  for (i = 0; i < psydata->band; i++)
  {
    a = transBuff[i];
    b = transBuff[i + psyInfo->size];
    // spectral line energy
    psydata->fftEnrgNext2[i] = a * a + b * b;
    //printf("psyInfo->fftEnrg[%d]: %g\n", i, psyInfo->fftEnrg[i]);
  }
  for (; i < psyInfo->size; i++)
  {
    psydata->fftEnrgNext2[i] = 0;
    //printf("psyInfo->fftEnrg[%d]: %g\n", i, psyInfo->fftEnrg[i]);
  }
#endif

  memcpy(transBuff, psyInfo->prevSamples, psyInfo->size * sizeof(double));
  memcpy(transBuff + psyInfo->size, newSamples, psyInfo->size * sizeof(double));

  for (j = 0; j < 8; j++)
  {
    memcpy(transBuffS, transBuff + (j * 128) + (1024 - 128),
	   2 * psyInfo->sizeS * sizeof(double));

    Hann(gpsyInfo, transBuffS, 2 * psyInfo->sizeS);
    rfft(transBuffS, 8);

    // shift bufs
    tmp = psydata->fftEnrgS[j];
    psydata->fftEnrgS[j] = psydata->fftEnrgNextS[j];
    psydata->fftEnrgNextS[j] = psydata->fftEnrgNext2S[j];
    psydata->fftEnrgNext2S[j] = tmp;

    for (i = 0; i < psydata->bandS; i++)
    {
      a = transBuffS[i];
      b = transBuffS[i + psyInfo->sizeS];
      // spectral line energy
      psydata->fftEnrgNext2S[j][i] = a * a + b * b;
      //printf("psyInfo->fftEnrgS[%d][%d]: %g\n", j, i, psyInfo->fftEnrgS[j][i]);
    }
    for (; i < psyInfo->sizeS; i++)
    {
      psydata->fftEnrgNext2S[j][i] = 0;
      //printf("psyInfo->fftEnrgS[%d][%d]: %g\n", j, i, psyInfo->fftEnrgS[j][i]);
    }
  }

  memcpy(psyInfo->prevSamples, newSamples, psyInfo->size * sizeof(double));
}

static void BlockSwitch(CoderInfo * coderInfo, PsyInfo * psyInfo, unsigned int numChannels)
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
    int lasttype = coderInfo[channel].block_type;

    if (desire == ONLY_SHORT_WINDOW
	|| coderInfo[channel].desired_block_type == ONLY_SHORT_WINDOW)
    {
      if (lasttype == ONLY_LONG_WINDOW || lasttype == SHORT_LONG_WINDOW)
	coderInfo[channel].block_type = LONG_SHORT_WINDOW;
      else
	coderInfo[channel].block_type = ONLY_SHORT_WINDOW;
    }
    else
    {
      if (lasttype == ONLY_SHORT_WINDOW || lasttype == LONG_SHORT_WINDOW)
	coderInfo[channel].block_type = SHORT_LONG_WINDOW;
      else
	coderInfo[channel].block_type = ONLY_LONG_WINDOW;
    }
    coderInfo[channel].desired_block_type = desire;
  }
}

psymodel_t psymodel2 =
{
  PsyInit,
  PsyEnd,
  PsyCalculate,
  PsyBufferUpdate,
  BlockSwitch
};
