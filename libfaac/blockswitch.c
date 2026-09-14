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
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blockswitch.h"
#include "coder.h"
#include "util.h"
#include "faac_internal.h"
#include "frame.h"

typedef float psyfloat;

/* The high-pass energy timeline is held as one contiguous array of per-sub-block
   energies rather than separate prev/curr/next arrays, so the +-2 sub-block
   lookahead around the current frame is a single sliding index instead of
   three-way stitching.
   It holds three 2-frame energy windows back to back: PREV, CUR and the one
   lookahead window NEXT. (Energy windows are 2 frames wide, which is why a single
   "next" window consumes the two-frames-ahead sample slot in the input FIFO.) */
#define SUBBLOCKS_PER_FRAME 8
#define ENG_WIN_PREV (0 * SUBBLOCKS_PER_FRAME)
#define ENG_WIN_CUR  (1 * SUBBLOCKS_PER_FRAME)
#define ENG_WIN_NEXT (2 * SUBBLOCKS_PER_FRAME)

typedef struct
{
  psyfloat eng[3 * SUBBLOCKS_PER_FRAME];
  /* Bit i set: the energy step into sub-block i is a transient. Judged once,
     when the sub-block's energy is produced, so the per-frame decision is a
     mask test rather than a re-walk of the timeline. */
  unsigned attack;
}
psydata_t;

/* The high-pass first difference (d[n]=x[n]-x[n-1]) de-weights bass, whose
 * broadband energy would otherwise mask HF attacks and false-trigger short
 * blocks on stationary music; what's left tracks the band where pre-echo is
 * audible. A relative energy jump between sub-blocks past this threshold is a
 * transient. */
#define PSY_TD_THRESH (0.5f)

static int PsyIsAttack(float lasteng, float eng)
{
  float toteng = (eng < lasteng) ? eng : lasteng;
  float volchg = fabsf(eng - lasteng);

  /* IEEE divide handles silence: 0/0 is NaN (no attack), x/0 is inf (attack). */
  return volchg / toteng > PSY_TD_THRESH;
}

/* Attack anywhere in the frame or its immediate temporal context, sub-blocks
   [cur-2, cur+9], wants a short block. */
static void PsyCheckShort(PsyInfo * psyInfo)
{
  enum {PREVS = 2, NEXTS = 2};
  const psydata_t *psydata = (const psydata_t *)psyInfo->data;
  unsigned span = (1u << (PREVS + SUBBLOCKS_PER_FRAME + NEXTS - 1)) - 1;

  psyInfo->block_type = (psydata->attack >> (ENG_WIN_CUR - PREVS + 1)) & span
                        ? ONLY_SHORT_WINDOW : ONLY_LONG_WINDOW;
}

void PsyInit(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo, unsigned int numChannels,
		    unsigned int sampleRate)
{
  unsigned int channel;
  int size;

  gpsyInfo->sampleRate = (float) sampleRate;

  for (channel = 0; channel < numChannels; channel++)
  {
    psydata_t *psydata = (psydata_t *)AllocMemory(sizeof(psydata_t));
    if (!psydata) return;
    memset(psydata, 0, sizeof(psydata_t));
    psyInfo[channel].data = psydata;
  }

  size = BLOCK_LEN_LONG;
  for (channel = 0; channel < numChannels; channel++)
  {
    psyInfo[channel].size = size;
  }

  size = BLOCK_LEN_SHORT;
  for (channel = 0; channel < numChannels; channel++)
    psyInfo[channel].sizeS = size;
}

/* Strongest relative energy jump across the sub-blocks of the window the MDCT
   is about to transform. ENG_WIN_PREV is exactly that window -- (FIFO_PAST,
   FIFO_CURR) -- because PsyBufferUpdate has already shifted by the time TNS
   runs.

   Exposed so TNS can gate on the temporal envelope already sitting in
   psydata instead of recomputing it. Returns 0 if PsyBufferUpdate hasn't
   populated the energy windows for this channel yet -- callers must treat
   that as "no basis to judge", not "flat". */
float PsyGetAttack(PsyInfo * psyInfo)
{
  psydata_t *psydata = (psydata_t *)psyInfo->data;
  float strength = 0.0f, total = 0.0f;
  int win;

  if (!psydata)
    return 0.0f;

  for (win = 0; win < SUBBLOCKS_PER_FRAME; win++)
  {
    float e = (float)psydata->eng[ENG_WIN_PREV + win];

    total += e;
    if (win)
    {
      float p = (float)psydata->eng[ENG_WIN_PREV + win - 1];
      float lo = (e < p) ? e : p;
      float s = fabsf(e - p) / lo;      /* IEEE divide covers silence */

      if (s > strength) strength = s;
    }
  }

  return total > 0.0f ? strength : 0.0f;
}

void PsyEnd(PsyInfo * psyInfo, unsigned int numChannels)
{
  unsigned int channel;

  for (channel = 0; channel < numChannels; channel++)
  {
    if (psyInfo[channel].data)
      FreeMemory(psyInfo[channel].data);
  }
}

/* Do psychoacoustical analysis */
/* Fast energy-based Perceptual Entropy approximation: sum subblock high-pass energies
   pre-computed in PsyBufferUpdate(), scaling by PE_ENERGY_SCALE to match PE complexity threshold. */
static void PsyCalcPE(PsyInfo * psyInfo)
{
  psydata_t *psydata = (psydata_t *)psyInfo->data;
  if (!psydata) { psyInfo->pe = 0.0f; return; }
  float pe = (float)psydata->eng[ENG_WIN_CUR + 0] + (float)psydata->eng[ENG_WIN_CUR + 1] +
             (float)psydata->eng[ENG_WIN_CUR + 2] + (float)psydata->eng[ENG_WIN_CUR + 3] +
             (float)psydata->eng[ENG_WIN_CUR + 4] + (float)psydata->eng[ENG_WIN_CUR + 5] +
             (float)psydata->eng[ENG_WIN_CUR + 6] + (float)psydata->eng[ENG_WIN_CUR + 7];
  psyInfo->pe = pe * PE_ENERGY_SCALE;
}

static void PsyAnalyzeChannel(PsyInfo * psyInfo)
{
  PsyCheckShort(psyInfo);
  PsyCalcPE(psyInfo);
}

/* Do psychoacoustical analysis */
void PsyCalculate(PsyInfo * psyInfo, const bool * isLfeChannel,
			 unsigned int numChannels)
{
  for (unsigned int channel = 0; channel < numChannels; channel++)
  {
      if (isLfeChannel[channel])
      {
          psyInfo[channel].block_type = ONLY_LONG_WINDOW;
          psyInfo[channel].pe = 0.0f;
      }
      else
          PsyAnalyzeChannel(&psyInfo[channel]);
  }
}

void PsyBufferUpdate(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo,
                            float * restrict p_lookahead1,
                            float * restrict p_lookahead2)
{
  int win;
  float * restrict transBuff = gpsyInfo->sharedWorkBuffLong;
  psydata_t *psydata = (psydata_t *)psyInfo->data;

  /* Shift the energy windows down by one frame: PREV<-CUR, CUR<-NEXT, freeing
     the NEXT region for the freshly-computed lookahead window below. */
  memmove(psydata->eng, psydata->eng + SUBBLOCKS_PER_FRAME,
          2 * SUBBLOCKS_PER_FRAME * sizeof(psyfloat));
  psydata->attack >>= SUBBLOCKS_PER_FRAME;

  /* Assembly of the newest 2048-sample window for energy analysis */
  memcpy(transBuff, p_lookahead1, BLOCK_LEN_LONG * sizeof(float));
  memcpy(transBuff + BLOCK_LEN_LONG, p_lookahead2, BLOCK_LEN_LONG * sizeof(float));

  for (win = 0; win < SUBBLOCKS_PER_FRAME; win++)
  {
    /* seg[-1] is in bounds (seg starts >= 448 samples in), so the first
     * difference carries across the sub-block boundary instead of resetting. */
    float *seg = transBuff + (win * BLOCK_LEN_SHORT) + (BLOCK_LEN_LONG - BLOCK_LEN_SHORT) / 2;
    float e = 0.0f;
    int l, n = 2 * psyInfo->sizeS;

    for (l = 0; l < n; l++)
    {
      float d = seg[l] - seg[l - 1];
      e += d * d;
    }
    psydata->eng[ENG_WIN_NEXT + win] = (psyfloat)e;
    if (PsyIsAttack((float)psydata->eng[ENG_WIN_NEXT + win - 1], e))
      psydata->attack |= 1u << (ENG_WIN_NEXT + win);
  }
}

void BlockSwitch(struct faacEncStruct *hEncoder, CoderInfo * coderInfo, PsyInfo * psyInfo, unsigned int numChannels)
{
  unsigned int channel;
  int desire = ONLY_LONG_WINDOW;

  /* Shared transient override for HE-AAC path.
   * Core delay alignment: SbrAnalyze runs on frame N full-rate; core
   * block-switch for frame N audio is emitted at a delay. Alignment logic
   * uses the FIFO. */
  if (hEncoder->config.aacObjectType == HE_V1 && SbrContextIsAnalysisValid(hEncoder->sbrContext))
  {
      for (channel = 0; channel < numChannels; channel++)
      {
          /* Alignment: the core frame being coded now lags the freshest SBR
           * analysis by LOOKAHEAD_DEPTH frames; FIFO index 0 holds that frame's
           * decision (FIFO sized SBR_DETECT_FIFO so [0] is LOOKAHEAD_DEPTH back). */
          int wantShort = SbrContextGetWantShort(hEncoder->sbrContext, (int)channel, 0);

          if (wantShort)
              psyInfo[channel].block_type = ONLY_SHORT_WINDOW;
          else
              psyInfo[channel].block_type = ONLY_LONG_WINDOW;
      }
  }

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
