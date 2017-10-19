/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2002 Krzysztof Nikiel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: psychkni.c,v 1.19 2012/03/01 18:34:17 knik Exp $
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "blockswitch.h"
#include "coder.h"
#include "fft.h"
#include "util.h"
#include <faac.h>

typedef float psyfloat;

typedef struct
{
  /* bandwidth */
  int bandS;
  int lastband;

  /* band volumes */
  psyfloat *engPrev[8];
  psyfloat *eng[8];
  psyfloat *engNext[8];
  psyfloat *engNext2[8];
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

#define PRINTSTAT 0
#if PRINTSTAT
static struct {
    int tot;
    int s;
} frames;
#endif

static void PsyCheckShort(PsyInfo * psyInfo, double quality)
{
  enum {PREVS = 2, NEXTS = 2};
  psydata_t *psydata = psyInfo->data;
  int lastband = psydata->lastband;
  int firstband = 2;
  int sfb, win;
  psyfloat *lasteng;

  psyInfo->block_type = ONLY_LONG_WINDOW;

  lasteng = NULL;
  for (win = 0; win < PREVS + 8 + NEXTS; win++)
  {
      psyfloat *eng;

      if (win < PREVS)
          eng = psydata->engPrev[win + 8 - PREVS];
      else if (win < (PREVS + 8))
          eng = psydata->eng[win - PREVS];
      else
          eng = psydata->engNext[win - PREVS - 8];

      if (lasteng)
      {
          double toteng = 0.0;
          double volchg = 0.0;

          for (sfb = firstband; sfb < lastband; sfb++)
          {
              toteng += (eng[sfb] < lasteng[sfb]) ? eng[sfb] : lasteng[sfb];
              volchg += fabs(eng[sfb] - lasteng[sfb]);
          }

          if ((volchg / toteng * quality) > 3.0)
          {
              psyInfo->block_type = ONLY_SHORT_WINDOW;
              break;
          }
      }
      lasteng = eng;
  }

#if PRINTSTAT
  frames.tot++;
  if (psyInfo->block_type == ONLY_SHORT_WINDOW)
      frames.s++;
#endif
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
    psyInfo[channel].size = size;

    psyInfo[channel].prevSamples =
      (double *) AllocMemory(size * sizeof(double));
    memset(psyInfo[channel].prevSamples, 0, size * sizeof(double));
  }

  size = BLOCK_LEN_SHORT;
  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = psyInfo[channel].data;

    psyInfo[channel].sizeS = size;

    for (j = 0; j < 8; j++)
    {
      psydata->engPrev[j] =
            (psyfloat *) AllocMemory(NSFB_SHORT * sizeof(psyfloat));
      memset(psydata->engPrev[j], 0, NSFB_SHORT * sizeof(psyfloat));
      psydata->eng[j] =
          (psyfloat *) AllocMemory(NSFB_SHORT * sizeof(psyfloat));
      memset(psydata->eng[j], 0, NSFB_SHORT * sizeof(psyfloat));
      psydata->engNext[j] =
          (psyfloat *) AllocMemory(NSFB_SHORT * sizeof(psyfloat));
      memset(psydata->engNext[j], 0, NSFB_SHORT * sizeof(psyfloat));
      psydata->engNext2[j] =
          (psyfloat *) AllocMemory(NSFB_SHORT * sizeof(psyfloat));
      memset(psydata->engNext2[j], 0, NSFB_SHORT * sizeof(psyfloat));
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
    if (psyInfo[channel].prevSamples)
      FreeMemory(psyInfo[channel].prevSamples);
  }

  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = psyInfo[channel].data;

    for (j = 0; j < 8; j++)
    {
        if (psydata->engPrev[j])
            FreeMemory(psydata->engPrev[j]);
        if (psydata->eng[j])
            FreeMemory(psydata->eng[j]);
        if (psydata->engNext[j])
            FreeMemory(psydata->engNext[j]);
        if (psydata->engNext2[j])
            FreeMemory(psydata->engNext2[j]);
    }
  }

  for (channel = 0; channel < numChannels; channel++)
  {
    if (psyInfo[channel].data)
      FreeMemory(psyInfo[channel].data);
  }

#if PRINTSTAT
  printf("short frames: %d/%d (%.2f %%)\n", frames.s, frames.tot, 100.0*frames.s/frames.tot);
#endif
}

/* Do psychoacoustical analysis */
static void PsyCalculate(ChannelInfo * channelInfo, GlobalPsyInfo * gpsyInfo,
			 PsyInfo * psyInfo, int *cb_width_long, int
			 num_cb_long, int *cb_width_short,
			 int num_cb_short, unsigned int numChannels,
			 double quality
			)
{
  unsigned int channel;

  // limit switching threshold
  if (quality < 0.4)
      quality = 0.4;

  for (channel = 0; channel < numChannels; channel++)
  {
    if (channelInfo[channel].present)
    {

      if (channelInfo[channel].cpe &&
	  channelInfo[channel].ch_is_left)
      {				/* CPE */

	int leftChan = channel;
	int rightChan = channelInfo[channel].paired_ch;

	PsyCheckShort(&psyInfo[leftChan], quality);
	PsyCheckShort(&psyInfo[rightChan], quality);
      }
      else if (!channelInfo[channel].cpe &&
	       channelInfo[channel].lfe)
      {				/* LFE */
        // Only set block type and it should be OK
	psyInfo[channel].block_type = ONLY_LONG_WINDOW;
      }
      else if (!channelInfo[channel].cpe)
      {				/* SCE */
	PsyCheckShort(&psyInfo[channel], quality);
      }
    }
  }
}

// imported from filtbank.c
static void mdct( FFT_Tables *fft_tables, double *data, int N )
{
    double *xi, *xr;
    double tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    double freq = 2.0 * M_PI / N;
    double cosfreq8, sinfreq8;
    int i, n;

    xi = (double*)AllocMemory((N >> 2)*sizeof(double));
    xr = (double*)AllocMemory((N >> 2)*sizeof(double));

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    cosfreq8 = cos (freq * 0.125);
    sinfreq8 = sin (freq * 0.125);
    c = cosfreq8;
    s = sinfreq8;

    for (i = 0; i < (N >> 2); i++) {
        /* calculate real and imaginary parts of g(n) or G(p) */
        n = 2 * i;

        if (n < (N >> 2))
            tempr = data [(N>>2) + (N>>1) - 1 - n] + data [N - (N>>2) + n];
        else
            tempr = data [(N>>2) + (N>>1) - 1 - n] - data [-(N>>2) + n];

        if (n < (N >> 2))
            tempi = data [(N>>2) + n] - data [(N>>2) - 1 - n];
        else
            tempi = data [(N>>2) + n] + data [N + (N>>2) - 1 - n];

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
    case BLOCK_LEN_SHORT * 2:
        fft( fft_tables, xr, xi, 6);
        break;
    case BLOCK_LEN_LONG * 2:
        fft( fft_tables, xr, xi, 9);
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

    if (xr) FreeMemory(xr);
    if (xi) FreeMemory(xi);
}


static void PsyBufferUpdate( FFT_Tables *fft_tables, GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo,
			    double *newSamples, unsigned int bandwidth,
			    int *cb_width_short, int num_cb_short)
{
  int win;
  double transBuff[2 * BLOCK_LEN_LONG];
  double transBuffS[2 * BLOCK_LEN_SHORT];
  psydata_t *psydata = psyInfo->data;
  psyfloat *tmp;
  int sfb;

  psydata->bandS = psyInfo->sizeS * bandwidth * 2 / gpsyInfo->sampleRate;

  memcpy(transBuff, psyInfo->prevSamples, psyInfo->size * sizeof(double));
  memcpy(transBuff + psyInfo->size, newSamples, psyInfo->size * sizeof(double));

  for (win = 0; win < 8; win++)
  {
    int first = 0;
    int last = 0;

    memcpy(transBuffS, transBuff + (win * BLOCK_LEN_SHORT) + (BLOCK_LEN_LONG - BLOCK_LEN_SHORT) / 2,
	   2 * psyInfo->sizeS * sizeof(double));

    Hann(gpsyInfo, transBuffS, 2 * psyInfo->sizeS);
    mdct( fft_tables, transBuffS, 2 * psyInfo->sizeS);

    // shift bufs
    tmp = psydata->engPrev[win];
    psydata->engPrev[win] = psydata->eng[win];
    psydata->eng[win] = psydata->engNext[win];
    psydata->engNext[win] = psydata->engNext2[win];
    psydata->engNext2[win] = tmp;

    for (sfb = 0; sfb < num_cb_short; sfb++)
    {
      double e;
      int l;

      first = last;
      last = first + cb_width_short[sfb];

      if (first < 1)
          first = 1;

      if (first >= psydata->bandS) // band out of range
          break;

      e = 0.0;
      for (l = first; l < last; l++)
          e += transBuffS[l] * transBuffS[l];

      psydata->engNext2[win][sfb] = e;
    }
    psydata->lastband = sfb;
    for (; sfb < num_cb_short; sfb++)
    {
        psydata->engNext2[win][sfb] = 0;
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
