/*
 *	Function prototypes for fast FFT routines
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
  $Revision: 1.4 $
  $Date: 2000/10/05 08:39:02 $ (check in)
  $Author: menno $
  *************************************************************************/

#include "transfo.h"

#define PFFTW(name)  CONCAT(pfftw_, name)
#define PFFTWI(name)  CONCAT(pfftwi_, name)
#define CONCAT_AUX(a, b) a ## b
#define CONCAT(a, b) CONCAT_AUX(a,b)
#define FFTW_KONST(x) ((fftw_real) x)

void PFFTW(twiddle_4)(fftw_complex *A, const fftw_complex *W, int iostride);
void PFFTWI(twiddle_4)(fftw_complex *A, const fftw_complex *W, int iostride);

