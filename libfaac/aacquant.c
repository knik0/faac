/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
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
 * $Id: aacquant.c,v 1.15 2002/12/28 09:22:18 knik Exp $
 */

#include <math.h>
#include <stdlib.h>

#include "aacquant.h"
#include "coder.h"
#include "huffman.h"
#include "psych.h"
#include "util.h"


#define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
#define QUANTFAC(rx)  adj43[rx]
#define ROUNDFAC 0.4054

static int FixNoise(CoderInfo *coderInfo,
		    const double *xr,
		    double *xr_pow,
		    int *xi,
		    double *xmin);

static int SortForGrouping(CoderInfo* coderInfo,
                           PsyInfo *psyInfo,
                           ChannelInfo *channelInfo,
                           int *sfb_width_table,
                           double *xr);

static int SearchStepSize(CoderInfo *coderInfo,
                          const int desired_rate,
                          const double *xr,
                          int *xi);

static void CalcAllowedDist(PsyInfo *psyInfo, int *cb_offset, int num_cb,
			    double *xr, double *xmin, int bits);

static int CountBits(CoderInfo *coderInfo, int *ix, const double *xr);

static int CountBitsLong(CoderInfo *coderInfo, int *xi);


double *pow43;
double *adj43;
double *adj43asm;


void AACQuantizeInit(CoderInfo *coderInfo, unsigned int numChannels)
{
    unsigned int channel, i;

    pow43 = (double*)AllocMemory(PRECALC_SIZE*sizeof(double));
    adj43 = (double*)AllocMemory(PRECALC_SIZE*sizeof(double));
    adj43asm = (double*)AllocMemory(PRECALC_SIZE*sizeof(double));

    pow43[0] = 0.0;
    for(i=1;i<PRECALC_SIZE;i++)
        pow43[i] = pow((double)i, 4.0/3.0);

    adj43asm[0] = 0.0;
    for (i = 1; i < PRECALC_SIZE; i++)
        adj43asm[i] = i - 0.5 - pow(0.5 * (pow43[i - 1] + pow43[i]),0.75);
    for (i = 0; i < PRECALC_SIZE-1; i++)
        adj43[i] = (i + 1) - pow(0.5 * (pow43[i] + pow43[i + 1]), 0.75);
    adj43[i] = 0.5;

    for (channel = 0; channel < numChannels; channel++) {
        coderInfo[channel].old_value = 0;
        coderInfo[channel].CurrentStep = 4;

        coderInfo[channel].requantFreq = (double*)AllocMemory(BLOCK_LEN_LONG*sizeof(double));
    }
}

void AACQuantizeEnd(CoderInfo *coderInfo, unsigned int numChannels)
{
    unsigned int channel;

    if (pow43) FreeMemory(pow43);
    if (adj43) FreeMemory(adj43);
    if (adj43asm) FreeMemory(adj43asm);

    for (channel = 0; channel < numChannels; channel++) {
        if (coderInfo[channel].requantFreq) FreeMemory(coderInfo[channel].requantFreq);
    }
}

static void BalanceEnergy(CoderInfo *coderInfo,
			  const double *xr, const int *xi)
{
  const double ifqstep = pow(2.0, 0.25);
  const double logstep_1 = 1.0 / log(ifqstep);
  const int sfcmax = 40;
  const int sfcmin = -10;
  int sb;
  int nsfb = coderInfo->nr_of_sfb;
  int start, end;
  int l;
  double en0, enq;
  int shift;

  for (sb = 0; sb < nsfb; sb++)
  {
    double qfac_1 = pow(2.0, -0.25*(coderInfo->scale_factor[sb] - coderInfo->global_gain));

    start = coderInfo->sfb_offset[sb];
    end   = coderInfo->sfb_offset[sb+1];

    en0 = 0.0;
    enq = 0.0;
    for (l = start; l < end; l++)
    {
      double xq = pow43[xi[l]];

      en0 += xr[l] * xr[l];
      enq += xq * xq;
    }

    if (enq == 0.0)
      continue;

    enq *= qfac_1 * qfac_1;

    shift = log(sqrt(enq / en0)) * logstep_1 + 1000.5;
    shift -= 1000;

    shift += coderInfo->scale_factor[sb];

    if (shift < sfcmin)
      shift = sfcmin;
    if (shift > sfcmax)
      shift = sfcmax;
    coderInfo->scale_factor[sb] = shift;
  }
}

static void UpdateRequant(CoderInfo *coderInfo, int *xi)
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
                PsyInfo *psyInfo,
                ChannelInfo *channelInfo,
                int *cb_width,
                int num_cb,
                double *xr,
                int desired_rate)
{
    int sb, i, do_q = 0;
    int bits, sign;
    double xr_pow[FRAME_LEN];
    double xmin[MAX_SCFAC_BANDS];
    int xi[FRAME_LEN];

    /* Use local copy's */
    int *scale_factor = coderInfo->scale_factor;


    if (coderInfo->block_type == ONLY_SHORT_WINDOW) {
        SortForGrouping(coderInfo, psyInfo, channelInfo, cb_width, xr);
    } else {
        for (sb = 0; sb < coderInfo->nr_of_sfb; sb++) {
            if (channelInfo->msInfo.is_present && channelInfo->msInfo.ms_used[sb]) {
                psyInfo->maskThr[sb] = psyInfo->maskThrMS[sb];
                psyInfo->maskEn[sb] = psyInfo->maskEnMS[sb];
            }
        }
    }


    /* Set all scalefactors to 0 */
    coderInfo->global_gain = 0;
    for (sb = 0; sb < coderInfo->nr_of_sfb; sb++)
        scale_factor[sb] = 0;

    /* Compute xr_pow */
    for (i = 0; i < FRAME_LEN; i++) {
        double temp = fabs(xr[i]);
        xr_pow[i] = sqrt(temp * sqrt(temp));
        do_q += (temp > 1E-20);
    }

    if (do_q) {
        CalcAllowedDist(psyInfo, coderInfo->sfb_offset,
			coderInfo->nr_of_sfb, xr, xmin, desired_rate);
	bits = SearchStepSize(coderInfo, 0.8 * desired_rate, xr_pow, xi);
	FixNoise(coderInfo, xr, xr_pow, xi, xmin);
	BalanceEnergy(coderInfo, xr, xi);
	UpdateRequant(coderInfo, xi);

#if 0
	printf("global gain: %d\n", coderInfo->global_gain);
	for (i = 0; i < coderInfo->nr_of_sfb; i++)
	  printf("sf %d: %d\n", i, coderInfo->scale_factor[i]);
#endif

        for ( i = 0; i < FRAME_LEN; i++ )  {
            sign = (xr[i] < 0) ? -1 : 1;
            xi[i] *= sign;
            coderInfo->requantFreq[i] *= sign;
        }
    } else {
        coderInfo->global_gain = 0;
        SetMemory(xi, 0, FRAME_LEN*sizeof(int));
    }

    CountBitsLong(coderInfo, xi);

    /* offset the difference of common_scalefac and scalefactors by SF_OFFSET  */
    for (i = 0; i < coderInfo->nr_of_sfb; i++) {
        if ((coderInfo->book_vector[i]!=INTENSITY_HCB)&&(coderInfo->book_vector[i]!=INTENSITY_HCB2)) {
            scale_factor[i] = coderInfo->global_gain - scale_factor[i] + SF_OFFSET;
        }
    }
    coderInfo->global_gain = scale_factor[0];

    /* place the codewords and their respective lengths in arrays data[] and len[] respectively */
    /* there are 'counter' elements in each array, and these are variable length arrays depending on the input */
    coderInfo->spectral_count = 0;
    for(i = 0; i < coderInfo->nr_of_sfb; i++) {
        OutputBits(
            coderInfo,
            coderInfo->book_vector[i],
            xi,
            coderInfo->sfb_offset[i],
            coderInfo->sfb_offset[i+1]-coderInfo->sfb_offset[i]);
    }

    return bits;
}

static int SearchStepSize(CoderInfo *coderInfo,
                          const int desired_rate,
                          const double *xr,
                          int *xi)
{
    int flag_GoneOver = 0;
    int CurrentStep = coderInfo->CurrentStep & 0xf;
    int lastshort = coderInfo->CurrentStep & 0x10;
    int thisshort = (coderInfo->block_type == ONLY_SHORT_WINDOW) ? 0x10 : 0;
    int nBits;
    int StepSize = coderInfo->old_value;
    int Direction = 0;
    int blockshift = 0;

    if (thisshort > lastshort)
      blockshift = -7;
    if (thisshort < lastshort)
      blockshift = +7;

    if (blockshift && (StepSize + blockshift) >= 0)
      StepSize += blockshift;

    do
    {
        coderInfo->global_gain = StepSize;
        nBits = CountBits(coderInfo, xi, xr);

        if (CurrentStep == 1 ) {
            break; /* nothing to adjust anymore */
        }
        if (flag_GoneOver) {
            CurrentStep /= 2;
        }
        if (nBits > desired_rate) { /* increase Quantize_StepSize */
            if (Direction == -1 && !flag_GoneOver) {
                flag_GoneOver = 1;
                CurrentStep /= 2; /* late adjust */
            }
            Direction = 1;
            StepSize += CurrentStep;
        } else if (nBits < desired_rate) {
            if (Direction == 1 && !flag_GoneOver) {
                flag_GoneOver = 1;
                CurrentStep /= 2; /* late adjust */
            }
            Direction = -1;
            StepSize -= CurrentStep;
        } else break;
    } while (1);

    while (nBits > desired_rate)
    {
      StepSize++;
      coderInfo->global_gain = StepSize;
      nBits = CountBits(coderInfo, xi, xr);
    }

    CurrentStep = coderInfo->old_value - StepSize;
    CurrentStep += blockshift;

    coderInfo->CurrentStep = CurrentStep/4 != 0 ? 4 : 2;
    coderInfo->old_value = coderInfo->global_gain;

    coderInfo->CurrentStep |= thisshort;

    return nBits;
}

#if 1 /* TAKEHIRO_IEEE754_HACK */

#pragma warning( disable : 4244 4307 )

typedef union {
    float f;
    int i;
} fi_union;

#define MAGIC_FLOAT (65536*(128))
#define MAGIC_INT 0x4b000000

static void Quantize(const double *xp, int *pi, double istep)
{
    int j;
    fi_union *fi;

    fi = (fi_union *)pi;
    for (j = FRAME_LEN/4 - 1; j >= 0; --j) {
        double x0 = istep * xp[0];
        double x1 = istep * xp[1];
        double x2 = istep * xp[2];
        double x3 = istep * xp[3];

        x0 += MAGIC_FLOAT; fi[0].f = x0;
        x1 += MAGIC_FLOAT; fi[1].f = x1;
        x2 += MAGIC_FLOAT; fi[2].f = x2;
        x3 += MAGIC_FLOAT; fi[3].f = x3;

        fi[0].f = x0 + (adj43asm - MAGIC_INT)[fi[0].i];
        fi[1].f = x1 + (adj43asm - MAGIC_INT)[fi[1].i];
        fi[2].f = x2 + (adj43asm - MAGIC_INT)[fi[2].i];
        fi[3].f = x3 + (adj43asm - MAGIC_INT)[fi[3].i];

        fi[0].i -= MAGIC_INT;
        fi[1].i -= MAGIC_INT;
        fi[2].i -= MAGIC_INT;
        fi[3].i -= MAGIC_INT;
        fi += 4;
        xp += 4;
    }
}

static void QuantizeBand(const double *xp, int *pi, double istep,
			   int offset, int end)
{
  int j;
  fi_union *fi;

  fi = (fi_union *)pi;
  for (j = offset; j < end; j++)
  {
    double x0 = istep * xp[j];

    x0 += MAGIC_FLOAT; fi[j].f = x0;
    fi[j].f = x0 + (adj43asm - MAGIC_INT)[fi[j].i];
    fi[j].i -= MAGIC_INT;
  }
}
#else
static void Quantize(const double *xr, int *ix, double istep)
{
    int j;

    for (j = FRAME_LEN/8; j > 0; --j) {
        double x1, x2, x3, x4, x5, x6, x7, x8;
        int rx1, rx2, rx3, rx4, rx5, rx6, rx7, rx8;

        x1 = *xr++ * istep;
        x2 = *xr++ * istep;
        XRPOW_FTOI(x1, rx1);
        x3 = *xr++ * istep;
        XRPOW_FTOI(x2, rx2);
        x4 = *xr++ * istep;
        XRPOW_FTOI(x3, rx3);
        x5 = *xr++ * istep;
        XRPOW_FTOI(x4, rx4);
        x6 = *xr++ * istep;
        XRPOW_FTOI(x5, rx5);
        x7 = *xr++ * istep;
        XRPOW_FTOI(x6, rx6);
        x8 = *xr++ * istep;
        XRPOW_FTOI(x7, rx7);
        x1 += QUANTFAC(rx1);
        XRPOW_FTOI(x8, rx8);
        x2 += QUANTFAC(rx2);
        XRPOW_FTOI(x1,*ix++);
        x3 += QUANTFAC(rx3);
        XRPOW_FTOI(x2,*ix++);
        x4 += QUANTFAC(rx4);
        XRPOW_FTOI(x3,*ix++);
        x5 += QUANTFAC(rx5);
        XRPOW_FTOI(x4,*ix++);
        x6 += QUANTFAC(rx6);
        XRPOW_FTOI(x5,*ix++);
        x7 += QUANTFAC(rx7);
        XRPOW_FTOI(x6,*ix++);
        x8 += QUANTFAC(rx8);
        XRPOW_FTOI(x7,*ix++);
        XRPOW_FTOI(x8,*ix++);
    }
}
#endif

static int CountBitsLong(CoderInfo *coderInfo, int *xi)
{
    int i, bits = 0;

    /* find a good method to section the scalefactor bands into huffman codebook sections */
    BitSearch(coderInfo, xi);

    /* calculate the amount of bits needed for encoding the huffman codebook numbers */
    bits += SortBookNumbers(coderInfo, NULL, 0);

    /* calculate the amount of bits needed for the spectral values */
    coderInfo->spectral_count = 0;
    for(i = 0; i < coderInfo->nr_of_sfb; i++) {
        bits += CalcBits(coderInfo,
            coderInfo->book_vector[i],
            xi,
            coderInfo->sfb_offset[i],
            coderInfo->sfb_offset[i+1] - coderInfo->sfb_offset[i]);
    }

    /* the number of bits for the scalefactors */
    bits += WriteScalefactors(coderInfo, NULL, 0);

    /* the total amount of bits required */
    return bits;
}

static int CountBits(CoderInfo *coderInfo, int *ix, const double *xr)
{
    int bits = 0, i;

    /* since quantize uses table lookup, we need to check this first: */
    double w = (IXMAX_VAL) / IPOW20(coderInfo->global_gain);
    for ( i = 0; i < FRAME_LEN; i++ )  {
        if (xr[i] > w)
            return LARGE_BITS;
    }

    Quantize(xr, ix, IPOW20(coderInfo->global_gain));

    bits = CountBitsLong(coderInfo, ix);

    return bits;
}

static void CalcAllowedDist(PsyInfo *psyInfo, int *cb_offset, int num_cb,
                            double *xr, double *xmin, int bits)
{
    int sfb, start, end;
    double xmin0;
    static const double globalthr = 23e-3;

    for (sfb = 0; sfb < num_cb; sfb++)
    {
        start = cb_offset[sfb];
        end = cb_offset[sfb + 1];

        xmin0 = psyInfo->maskEn[sfb];
        if (xmin0 > 0.0)
	  xmin0 = psyInfo->maskThr[sfb] * (double)end * globalthr / xmin0;

	xmin[sfb] = xmin0;
    }
}

static int FixNoise(CoderInfo *coderInfo,
		    const double *xr,
		    double *xr_pow,
		    int *xi,
		    double *xmin)
{
    int i, sb;
    int start, end;
    double quantvol, diffvol;
    double quantfac;
    double tmp;
    int done = 0;
    const double ifqstep = pow(2.0, 0.1875);
    const int sfacmax = 30;

    for (sb = 0; sb < coderInfo->nr_of_sfb; sb++)
    {
      if (!xmin[sb])
        continue;

      start = coderInfo->sfb_offset[sb];
      end = coderInfo->sfb_offset[sb+1];

    calcdist:
      quantfac = pow(2.0, 0.25*(coderInfo->scale_factor[sb] - coderInfo->global_gain));

      quantvol = 0.0;
      diffvol = 0.0;
      for (i = start; i < end; i++)
      {
	tmp = pow43[xi[i]];
	quantvol += tmp;

	diffvol += fabs(fabs(xr[i]) * quantfac - tmp);
    }

      if (diffvol / quantvol > xmin[sb]) // distorion to high
      {
	if (coderInfo->scale_factor[sb] < sfacmax)
	{
	  for (i = start; i < end; i++)
	    xr_pow[i] *= ifqstep;
	  coderInfo->scale_factor[sb]++;
	  QuantizeBand(xr_pow, xi, IPOW20(coderInfo->global_gain), start, end);
	  goto calcdist;
	}
      }
    else
	done++;
      //printf("%d: %g\t%g - %g(%d)\n", sb, quantvol, diffvol/quantvol, xmin[sb], coderInfo->scale_factor[sb]);
            }

    return done;
}

static int SortForGrouping(CoderInfo* coderInfo,
                           PsyInfo *psyInfo,
                           ChannelInfo *channelInfo,
                           int *sfb_width_table,
                           double *xr)
{
    int i,j,ii;
    int index = 0;
    double xr_tmp[1024];
    double thr_tmp[150];
    double en_tmp[150];
    int book=1;
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
                    xr_tmp[index++] = xr[ii+ sfb_offset[k] + 128*j +group_offset];
            }
        }
        group_offset +=  128*window_group_length[i];
    }

    for (k=0; k<1024; k++){
        xr[k] = xr_tmp[k];
    }


    /* now calc the new sfb_offset table for the whole p_spectrum vector*/
    index = 0;
    sfb_offset[index++] = 0;
    windowOffset = 0;
    for (i=0; i < num_window_groups; i++) {
        for (k=0 ; k <*nr_of_sfb; k++) {
            int w;
            double worstTHR;
            double worstEN;

            /* for this window group and this band, find worst case inverse sig-mask-ratio */
            if (channelInfo->msInfo.is_present && channelInfo->msInfo.ms_usedS[windowOffset][k]) {
                worstTHR = psyInfo->maskThrSMS[windowOffset][k];
                worstEN = psyInfo->maskEnSMS[windowOffset][k];
            } else {
                worstTHR = psyInfo->maskThrS[windowOffset][k];
                worstEN = psyInfo->maskEnS[windowOffset][k];
            }

            for (w=1;w<window_group_length[i];w++) {
                if (channelInfo->msInfo.is_present && channelInfo->msInfo.ms_usedS[w+windowOffset][k]) {
                    if (psyInfo->maskThrSMS[w+windowOffset][k] < worstTHR) {
                        worstTHR = psyInfo->maskThrSMS[w+windowOffset][k];
                        worstEN = psyInfo->maskEnSMS[w+windowOffset][k];
                    }
                } else {
                    if (psyInfo->maskThrS[w+windowOffset][k] < worstTHR) {
                        worstTHR = psyInfo->maskThrS[w+windowOffset][k];
                        worstEN = psyInfo->maskEnS[w+windowOffset][k];
                    }
                }
            }
            thr_tmp[k+ i* *nr_of_sfb] = worstTHR;
            en_tmp[k+ i* *nr_of_sfb] = worstEN;
            sfb_offset[index] = sfb_offset[index-1] + sfb_width_table[k]*window_group_length[i] ;
            index++;
        }
        windowOffset += window_group_length[i];
    }

    *nr_of_sfb = *nr_of_sfb * num_window_groups;  /* Number interleaved bands. */

    for (k = 0; k < *nr_of_sfb; k++){
        psyInfo->maskThr[k] = thr_tmp[k];
        psyInfo->maskEn[k] = en_tmp[k];
    }

    return 0;
}
