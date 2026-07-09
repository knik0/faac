/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
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

#ifndef FILTBANK_H
#define FILTBANK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "frame.h"

#define NFLAT_LS 448



#define SINE_WINDOW 0
#define KBD_WINDOW  1

void			FilterBankInit		( faacEncStruct* hEncoder );

void			FilterBankEnd		( faacEncStruct* hEncoder );

void			MDCT				( FFT_Tables *fft_tables, float * restrict data, int N, float * restrict work );

void			FilterBank( faacEncStruct* hEncoder,
						CoderInfo *coderInfo,
						float * restrict p_prev_data,
						float * restrict p_in_data,
						float * restrict p_out_mdct);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILTBANK_H */
