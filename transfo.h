/*
 *	Function prototypes for MDCT transform
 *
 *	Copyright (c) 1999 M. Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**************************************************************************
  Version Control Information			Method: CVS
  Identifiers:
  $Revision: 1.10 $
  $Date: 2000/10/05 08:39:03 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef TRANSFORM_H
#define TRANSFORM_H

// Use this for decoder - single precision
//typedef float fftw_real;

// Use this for encoder - double precision
typedef double fftw_real;

typedef struct {
     fftw_real re, im;
} fftw_complex;

#define c_re(c)  ((c).re)
#define c_im(c)  ((c).im)

#define DEFINE_PFFTW(size)			\
 void pfftwi_##size(fftw_complex *input);	\
 void pfftw_##size(fftw_complex *input);	\
 int  pfftw_permutation_##size(int i);

DEFINE_PFFTW(16)
DEFINE_PFFTW(32)
DEFINE_PFFTW(64)
DEFINE_PFFTW(128)
DEFINE_PFFTW(256)
DEFINE_PFFTW(512)
DEFINE_PFFTW(1024)

void make_FFT_order(void);
void make_MDCT_windows(void);
void IMDCT(fftw_real *data, int N);
void MDCT(fftw_real *data, int N);
void realft2048(double *data);
void realft256(double *data);
void initrft(void);

extern int unscambled64[64];      /* the permutation array for FFT64*/
extern int unscambled128[128];    /* the permutation array for FFT128*/
extern int unscambled512[512];    /* the permutation array for FFT512*/
extern int unscambled1024[1024];  /* the permutation array for FFT1024*/
extern fftw_complex FFTarray[1024];    /* the array for in-place FFT */

#endif	  /*	TRANSFORM_H		*/
