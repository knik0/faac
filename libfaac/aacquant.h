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
 * $Id: aacquant.h,v 1.2 2001/03/05 11:33:37 menno Exp $
 */

#ifndef AACQUANT_H
#define AACQUANT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "coder.h"
#include "psych.h"

#define IXMAX_VAL 8191
#define PRECALC_SIZE (IXMAX_VAL+2)
#define LARGE_BITS 100000
#define SF_OFFSET 100

#define POW20(x)  pow(2.0,((double)x)*.25)
#define IPOW20(x)  pow(2.0,-((double)x)*.1875)


typedef struct {
    int     over_count;      /* number of quantization noise > masking */
    int     tot_count;       /* all */
    double  over_noise;      /* sum of quantization noise > masking */
    double  tot_noise;       /* sum of all quantization noise */
    double  max_noise;       /* max quantization noise */
} calcNoiseResult;

void AACQuantizeInit(CoderInfo *coderInfo, unsigned int numChannels);
void AACQuantizeEnd(CoderInfo *coderInfo, unsigned int numChannels);

int AACQuantize(CoderInfo *coderInfo,
				PsyInfo *psyInfo,
				ChannelInfo *channelInfo,
				int *cb_width,
				int num_cb,
				double *xr,
				int desired_rate);

static int SearchStepSize(CoderInfo *coderInfo,
						  const int desired_rate,
						  const double *xr,
						  int *xi);

static void Quantize(const double *xr, int *ix, double istep);

static int SortForGrouping(CoderInfo* coderInfo, PsyInfo *psyInfo,
						   ChannelInfo *channelInfo, int *sfb_width_table,
						   double *xr);

static int CountBitsLong(CoderInfo *coderInfo, int *xi);

static int CountBits(CoderInfo *coderInfo, int *ix, const double *xr);

static int InnerLoop(CoderInfo *coderInfo, double *xr_pow, int *xi, int max_bits);

static void CalcAllowedDist(PsyInfo *psyInfo, int *cb_width, int num_cb,
							double *xr, double *xmin);

static int CalcNoise(CoderInfo *coderInfo, double *xr, int *xi, double *requant_xr,
					 double *error_energy, double *xmin, calcNoiseResult *res);

static int OuterLoop(CoderInfo *coderInfo, double *xr, double *xr_pow, int *xi,
					 double *xmin, int target_bits);

static int QuantCompare(calcNoiseResult *best,
						calcNoiseResult *calc);

static int BalanceNoise(CoderInfo *coderInfo, double *distort, double *xrpow);

static void AmpScalefacBands(CoderInfo *coderInfo, double *distort, double *xr_pow);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AACQUANT_H */
