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
 */
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
   energies rather than separate prev/curr/next arrays, so PsyCheckShort's +-2
   sub-block lookahead is a single sliding index instead of three-way stitching.
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
}
psydata_t;

/* The high-pass first difference (d[n]=x[n]-x[n-1]) de-weights bass, whose
 * broadband energy would otherwise mask HF attacks and false-trigger short
 * blocks on stationary music; what's left tracks the band where pre-echo is
 * audible. A relative energy jump between sub-blocks past this threshold is a
 * transient. */
#define PSY_TD_THRESH ((faac_real)0.5)

static void PsyCheckShort(PsyInfo * psyInfo)
{
  enum {PREVS = 2, NEXTS = 2};
  psydata_t *psydata = (psydata_t *)psyInfo->data;
  int win;
  faac_real lasteng = (faac_real)psydata->eng[ENG_WIN_CUR - PREVS]; /* start at PREVS before current */

  psyInfo->block_type = ONLY_LONG_WINDOW;

  /* Search for transients across the current frame and its immediate temporal context.
     The search range is [curr-2, curr+9]. */
  for (win = 1; win < PREVS + SUBBLOCKS_PER_FRAME + NEXTS; win++)
  {
      faac_real eng = (faac_real)psydata->eng[ENG_WIN_CUR - PREVS + win];

      faac_real toteng = (eng < lasteng) ? eng : lasteng;
      faac_real volchg = FAAC_FABS(eng - lasteng);

      /* Relative energy jump indicates a transient. IEEE divide handles silence cases. */
      if (volchg / toteng > PSY_TD_THRESH)
      {
          psyInfo->block_type = ONLY_SHORT_WINDOW;
          break;
      }
      lasteng = eng;
  }
}

void PsyInit(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo, unsigned int numChannels,
		    unsigned int sampleRate)
{
  unsigned int channel;
  int size;

  gpsyInfo->sampleRate = (faac_real) sampleRate;

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
void PsyCalculate(ChannelInfo * channelInfo, PsyInfo * psyInfo,
			 unsigned int numChannels
			)
{
  unsigned int channel;

  for (channel = 0; channel < numChannels; channel++)
  {
    if (channelInfo[channel].present)
    {

      if (channelInfo[channel].type == ELEMENT_CPE &&
	  channelInfo[channel].ch_is_left)
      {				/* CPE */

	int leftChan = channel;
	int rightChan = channelInfo[channel].paired_ch;

	PsyCheckShort(&psyInfo[leftChan]);
	PsyCheckShort(&psyInfo[rightChan]);
      }
      else if (channelInfo[channel].type == ELEMENT_LFE)
      {				/* LFE */
        // Only set block type and it should be OK
	psyInfo[channel].block_type = ONLY_LONG_WINDOW;
      }
      else if (channelInfo[channel].type == ELEMENT_SCE)
      {				/* SCE */
	PsyCheckShort(&psyInfo[channel]);
      }
    }
  }
}

void PsyBufferUpdate(GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo,
                            faac_real * restrict p_lookahead1,
                            faac_real * restrict p_lookahead2)
{
  int win;
  faac_real * restrict transBuff = gpsyInfo->sharedWorkBuffLong;
  psydata_t *psydata = (psydata_t *)psyInfo->data;

  /* Shift the energy windows down by one frame: PREV<-CUR, CUR<-NEXT, freeing
     the NEXT region for the freshly-computed lookahead window below. */
  memmove(psydata->eng, psydata->eng + SUBBLOCKS_PER_FRAME,
          2 * SUBBLOCKS_PER_FRAME * sizeof(psyfloat));

  /* Assembly of the newest 2048-sample window for energy analysis */
  memcpy(transBuff, p_lookahead1, BLOCK_LEN_LONG * sizeof(faac_real));
  memcpy(transBuff + BLOCK_LEN_LONG, p_lookahead2, BLOCK_LEN_LONG * sizeof(faac_real));

  for (win = 0; win < SUBBLOCKS_PER_FRAME; win++)
  {
    /* seg[-1] is in bounds (seg starts >= 448 samples in), so the first
     * difference carries across the sub-block boundary instead of resetting. */
    faac_real *seg = transBuff + (win * BLOCK_LEN_SHORT) + (BLOCK_LEN_LONG - BLOCK_LEN_SHORT) / 2;
    faac_real e = 0.0;
    int l, n = 2 * psyInfo->sizeS;

    for (l = 0; l < n; l++)
    {
      faac_real d = seg[l] - seg[l - 1];
      e += d * d;
    }
    psydata->eng[ENG_WIN_NEXT + win] = (psyfloat)e;
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
