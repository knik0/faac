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
 * $Id: filtbank.h,v 1.6 2001/03/05 11:33:37 menno Exp $
 */

#ifndef FILTBANK_H
#define FILTBANK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "frame.h"

#define NFLAT_LS 448


#define MOVERLAPPED     0
#define MNON_OVERLAPPED 1


#define SINE_WINDOW 0
#define KBD_WINDOW  1

void FilterBankInit(faacEncHandle hEncoder);

void FilterBankEnd(faacEncHandle hEncoder);

void FilterBank(faacEncHandle hEncoder,
				CoderInfo *coderInfo,
				double *p_in_data,
				double *p_out_mdct,
				double *p_overlap,
				int overlap_select);

void IFilterBank(faacEncHandle hEncoder,
				 CoderInfo *coderInfo,
				 double *p_in_data,
				 double *p_out_mdct,
				 double *p_overlap,
				 int overlap_select);

void specFilter(double *freqBuff,
				int sampleRate,
				int lowpassFreq,
				int specLen
				);

static void CalculateKBDWindow(double* win, double alpha, int length);
static double Izero(double x);
static void MDCT(double *data, int N);
static void IMDCT(double *data, int N);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILTBANK_H */
