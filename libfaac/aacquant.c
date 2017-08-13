/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 * Copyright (C) 2002, 2003 Krzysztof Nikiel
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
 */

#include <math.h>
#include <stdlib.h>

#include <faac.h>
#include "aacquant.h"
#include "coder.h"
#include "huffman.h"
#include "util.h"

#include "quantize.c"


void AACQuantizeInit(CoderInfo *coderInfo, unsigned int numChannels,
                     AACQuantCfg *aacquantCfg)
{
    unsigned int channel, i;


    aacquantCfg->pow43 = (double*)AllocMemory(PRECALC_SIZE*sizeof(double));
    aacquantCfg->pow43[0] = 0.0;
    for(i=1;i<PRECALC_SIZE;i++)
        aacquantCfg->pow43[i] = pow((double)i, 4.0/3.0);

    for (channel = 0; channel < numChannels; channel++) {
        coderInfo[channel].requantFreq = (double*)AllocMemory(BLOCK_LEN_LONG*sizeof(double));
    }
}

void AACQuantizeEnd(CoderInfo *coderInfo, unsigned int numChannels,
                    AACQuantCfg *aacquantCfg)
{
    unsigned int channel;


    if (aacquantCfg->pow43)
    {
        FreeMemory(aacquantCfg->pow43);
        aacquantCfg->pow43 = NULL;
    }

    for (channel = 0; channel < numChannels; channel++) {
        if (coderInfo[channel].requantFreq) FreeMemory(coderInfo[channel].requantFreq);
    }
}

static void UpdateRequant(CoderInfo *coderInfo, int *xi,
                          double *pow43)
{
  double *requant_xr = coderInfo->requantFreq;
  int sb;
  int i;

  for (sb = 0; sb < coderInfo->nr_of_sfb; sb++)
  {
    double invQuantFac =
      pow(2.0, -0.25*(coderInfo->scale_factor[sb] - coderInfo->global_gain));
    int start = coderInfo->sfb_offset[sb];
    int end = coderInfo->sfb_offset[sb + 1];

    for (i = start; i < end; i++)
      requant_xr[i] = pow43[xi[i]] * invQuantFac;
  }
}

int AACQuantize(CoderInfo *coderInfo,
                int *cb_width,
                int num_cb,
                double *xr,
                AACQuantCfg *aacquantCfg)
{
    int sb, i;
    int bits = 0, sign;
    int xi[FRAME_LEN];

    /* Use local copy's */
    int *scale_factor = coderInfo->scale_factor;

    /* Set all scalefactors to 0 */
    coderInfo->global_gain = 0;
    for (sb = 0; sb < coderInfo->nr_of_sfb; sb++)
        scale_factor[sb] = 0;

    if (BlocQuant(coderInfo, xr, xi, (double)aacquantCfg->quality/100))
    {
        UpdateRequant(coderInfo, xi, aacquantCfg->pow43);

        for ( i = 0; i < FRAME_LEN; i++ )  {
            sign = (xr[i] < 0) ? -1 : 1;
            xi[i] *= sign;
            coderInfo->requantFreq[i] *= sign;
        }
    }


    BitSearch(coderInfo, xi);

    /* offset the difference of common_scalefac and scalefactors by SF_OFFSET  */
    for (i = 0; i < coderInfo->nr_of_sfb; i++) {
        if ((coderInfo->book_vector[i]!=INTENSITY_HCB)&&(coderInfo->book_vector[i]!=INTENSITY_HCB2)) {
            scale_factor[i] = coderInfo->global_gain - scale_factor[i] + SF_OFFSET;
        }
    }
    coderInfo->global_gain = scale_factor[0];
    // clamp to valid diff range
    {
      int previous_scale_factor = coderInfo->global_gain;
      int previous_is_factor = 0;
      for (i = 0; i < coderInfo->nr_of_sfb; i++) {
        if ((coderInfo->book_vector[i]==INTENSITY_HCB) ||
            (coderInfo->book_vector[i]==INTENSITY_HCB2)) {
            const int diff = scale_factor[i] - previous_is_factor;
            if (diff < -60) scale_factor[i] = previous_is_factor - 60;
            else if (diff > 60) scale_factor[i] = previous_is_factor + 60;
            previous_is_factor = scale_factor[i];
//            printf("sf %d: %d diff=%d **\n", i, coderInfo->scale_factor[i], diff);
        } else if (coderInfo->book_vector[i]) {
            const int diff = scale_factor[i] - previous_scale_factor;
            if (diff < -60) scale_factor[i] = previous_scale_factor - 60;
            else if (diff > 60) scale_factor[i] = previous_scale_factor + 60;
            previous_scale_factor = scale_factor[i];
//            printf("sf %d: %d diff=%d\n", i, coderInfo->scale_factor[i], diff);
        }
      }
    }

    /* place the codewords and their respective lengths in arrays data[] and len[] respectively */
    /* there are 'counter' elements in each array, and these are variable length arrays depending on the input */
#ifdef DRM
    coderInfo->iLenReordSpData = 0; /* init length of reordered spectral data */
    coderInfo->iLenLongestCW = 0; /* init length of longest codeword */
    coderInfo->cur_cw = 0; /* init codeword counter */
#endif
    coderInfo->spectral_count = 0;
    sb = 0;
    for(i = 0; i < coderInfo->nr_of_sfb; i++) {
        OutputBits(
            coderInfo,
#ifdef DRM
            &coderInfo->book_vector[i], /* needed for VCB11 */
#else
            coderInfo->book_vector[i],
#endif
            xi,
            coderInfo->sfb_offset[i],
            coderInfo->sfb_offset[i+1]-coderInfo->sfb_offset[i]);

        if (coderInfo->book_vector[i])
              sb = i;
    }

    // FIXME: Check those max_sfb/nr_of_sfb. Isn't it the same?
    coderInfo->max_sfb = coderInfo->nr_of_sfb = sb + 1;

    return bits;
}

int SortForGrouping(CoderInfo* coderInfo,
                    int *sfb_width_table,
                    double *xr)
{
    int i,j,ii;
    int index = 0;
    double xr_tmp[FRAME_LEN];
    int group_offset=0;
    int k=0;
    int windowOffset = 0;


    /* set up local variables for used quantInfo elements */
    int* sfb_offset = coderInfo->sfb_offset;
    int* nr_of_sfb = &(coderInfo->nr_of_sfb);
    int* window_group_length;
    int num_window_groups;
    *nr_of_sfb = coderInfo->max_sfb;              /* Init to max_sfb */
    window_group_length = coderInfo->window_group_length;
    num_window_groups = coderInfo->num_window_groups;

    /* calc org sfb_offset just for shortblock */
    sfb_offset[k]=0;
    for (k=1 ; k <*nr_of_sfb+1; k++) {
        sfb_offset[k] = sfb_offset[k-1] + sfb_width_table[k-1];
    }

    /* sort the input spectral coefficients */
    index = 0;
    group_offset=0;
    for (i=0; i< num_window_groups; i++) {
        for (k=0; k<*nr_of_sfb; k++) {
            for (j=0; j < window_group_length[i]; j++) {
                for (ii=0;ii< sfb_width_table[k];ii++)
                    xr_tmp[index++] = xr[ii+ sfb_offset[k] + BLOCK_LEN_SHORT*j +group_offset];
            }
        }
        group_offset +=  BLOCK_LEN_SHORT*window_group_length[i];
    }

    for (k=0; k<FRAME_LEN; k++){
        xr[k] = xr_tmp[k];
    }


    /* now calc the new sfb_offset table for the whole p_spectrum vector*/
    index = 0;
    sfb_offset[index++] = 0;
    windowOffset = 0;
    for (i=0; i < num_window_groups; i++) {
        for (k=0 ; k <*nr_of_sfb; k++) {
            sfb_offset[index] = sfb_offset[index-1] + sfb_width_table[k]*window_group_length[i] ;
            index++;
        }
        windowOffset += window_group_length[i];
    }

    *nr_of_sfb = *nr_of_sfb * num_window_groups;  /* Number interleaved bands. */

    return 0;
}

void CalcAvgEnrg(CoderInfo *coderInfo,
                 const double *xr)
{
  int end, l;
  int last = 0;
  double totenrg = 0.0;

  end = coderInfo->sfb_offset[coderInfo->nr_of_sfb];
  for (l = 0; l < end; l++)
  {
    if (xr[l])
    {
      last = l;
      totenrg += xr[l] * xr[l];
    }
    }
  last++;

  coderInfo->lastx = last;
  coderInfo->avgenrg = totenrg / last;
}
