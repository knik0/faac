/*
 * FAAC - Freeware Advanced Audio Coder
 * $Id: fft.c,v 1.12 2005/02/02 07:49:55 sur Exp $
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
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fft.h"
#include "util.h"

#define MAXLOGM 9
#define MAXLOGR 8

void fft_initialize( FFT_Tables *fft_tables )
{
	int i;
	fft_tables->costbl		= AllocMemory( (MAXLOGM+1) * sizeof( fft_tables->costbl[0] ) );
	fft_tables->negsintbl	= AllocMemory( (MAXLOGM+1) * sizeof( fft_tables->negsintbl[0] ) );
	fft_tables->reordertbl	= AllocMemory( (MAXLOGM+1) * sizeof( fft_tables->reordertbl[0] ) );
	
	for( i = 0; i< MAXLOGM+1; i++ )
	{
		fft_tables->costbl[i]		= NULL;
		fft_tables->negsintbl[i]	= NULL;
		fft_tables->reordertbl[i]	= NULL;
	}
}

void fft_terminate( FFT_Tables *fft_tables )
{
	int i;

	for( i = 0; i< MAXLOGM+1; i++ )
	{
		if( fft_tables->costbl[i] != NULL )
			FreeMemory( fft_tables->costbl[i] );
		
		if( fft_tables->negsintbl[i] != NULL )
			FreeMemory( fft_tables->negsintbl[i] );
			
		if( fft_tables->reordertbl[i] != NULL )
			FreeMemory( fft_tables->reordertbl[i] );
	}

	FreeMemory( fft_tables->costbl );
	FreeMemory( fft_tables->negsintbl );
	FreeMemory( fft_tables->reordertbl );

	fft_tables->costbl		= NULL;
	fft_tables->negsintbl	= NULL;
	fft_tables->reordertbl	= NULL;
}

static void reorder2( FFT_Tables *fft_tables, faac_real *xr, faac_real *xi, int logm)
{
	int i;
	int size = 1 << logm;
	const unsigned short *r;


	if ( fft_tables->reordertbl[logm] == NULL ) // create bit reversing table
	{
		fft_tables->reordertbl[logm] = AllocMemory(size * sizeof(*(fft_tables->reordertbl[0])));

		for (i = 0; i < size; i++)
		{
			int reversed = 0;
			int b0;
			int tmp = i;

			for (b0 = 0; b0 < logm; b0++)
			{
				reversed = (reversed << 1) | (tmp & 1);
				tmp >>= 1;
			}
			fft_tables->reordertbl[logm][i] = reversed;
		}
	}

	r = fft_tables->reordertbl[logm];

	for (i = 0; i < size; i++)
	{
		int j = r[i];
		faac_real tmp;

		if (j <= i)
			continue;

		tmp = xr[i];
		xr[i] = xr[j];
		xr[j] = tmp;

		tmp = xi[i];
		xi[i] = xi[j];
		xi[j] = tmp;
	}
}

static void fft_proc(
		faac_real *xr,
		faac_real *xi,
		fftfloat *refac, 
		fftfloat *imfac, 
		int size)	
{
	int step, shift, pos;
	int exp, estep;

	estep = size >> 1;
	/* First stage: step = 1
	   Twiddle factor W_N^0 is always (1, 0).
	   Eliminate all multiplications and table lookups.
	*/
	for (pos = 0; pos < size; pos += 2)
	{
		faac_real v2r, v2i;
		int x1 = pos;
		int x2 = pos + 1;

		v2r = xr[x2];
		v2i = xi[x2];

		xr[x2] = xr[x1] - v2r;
		xr[x1] += v2r;

		xi[x2] = xi[x1] - v2i;
		xi[x1] += v2i;
	}

	/* Second stage: step = 2
	   shift = 0: Twiddle is (1, 0).
	   shift = 1: Twiddle is (0, -1).
	   Eliminate multiplications and avoid trig/table calls entirely.
	*/
	if (size >= 4) {
		for (pos = 0; pos < size; pos += 4)
		{
			faac_real v2r, v2i;
			int x1 = pos;
			int x2 = pos + 2;

			/* shift = 0: Rotation by 0 degrees */
			v2r = xr[x2];
			v2i = xi[x2];

			xr[x2] = xr[x1] - v2r;
			xr[x1] += v2r;

			xi[x2] = xi[x1] - v2i;
			xi[x1] += v2i;

			/* shift = 1: Rotation by -90 degrees */
			x1++;
			x2++;

			v2r = xi[x2];
			v2i = -xr[x2];

			xr[x2] = xr[x1] - v2r;
			xr[x1] += v2r;

			xi[x2] = xi[x1] - v2i;
			xi[x1] += v2i;
		}
	}

	/* Resume standard Radix-2 loop from stage 3 (step = 4) */
	estep = size >> 2;
	for (step = 4; step < size; step *= 2)
	{
		int x1;
		int x2 = 0;
		estep >>= 1;
		for (pos = 0; pos < size; pos += (2 * step))
		{
			x1 = x2;
			x2 += step;
			exp = 0;
			for (shift = 0; shift < step; shift++)
			{
				faac_real v2r, v2i;

				v2r = xr[x2] * refac[exp] - xi[x2] * imfac[exp];
				v2i = xr[x2] * imfac[exp] + xi[x2] * refac[exp];

				xr[x2] = xr[x1] - v2r;
				xr[x1] += v2r;

				xi[x2] = xi[x1] - v2i;

				xi[x1] += v2i;

				exp += estep;

				x1++;
				x2++;
			}
		}
	}
}

static void check_tables( FFT_Tables *fft_tables, int logm)
{
	if( fft_tables->costbl[logm] == NULL )
	{
		int i;
		int size = 1 << logm;

		if( fft_tables->negsintbl[logm] != NULL )
			FreeMemory( fft_tables->negsintbl[logm] );

		fft_tables->costbl[logm]	= AllocMemory((size / 2) * sizeof(*(fft_tables->costbl[0])));
		fft_tables->negsintbl[logm]	= AllocMemory((size / 2) * sizeof(*(fft_tables->negsintbl[0])));

		for (i = 0; i < (size >> 1); i++)
		{
			faac_real theta = 2.0 * M_PI * ((faac_real) i) / (faac_real) size;
			fft_tables->costbl[logm][i]		= FAAC_COS(theta);
			fft_tables->negsintbl[logm][i]	= -FAAC_SIN(theta);
		}
	}
}

void fft( FFT_Tables *fft_tables, faac_real *xr, faac_real *xi, int logm)
{
	if (logm > MAXLOGM)
	{
		fprintf(stderr, "fft size too big\n");
		exit(1);
	}

	if (logm < 1)
	{
		//printf("logm < 1\n");
		return;
	}

	check_tables( fft_tables, logm);

	reorder2( fft_tables, xr, xi, logm);

	fft_proc( xr, xi, fft_tables->costbl[logm], fft_tables->negsintbl[logm], 1 << logm );
}

void rfft( FFT_Tables *fft_tables, faac_real *x, int logm)
{
	faac_real xi[1 << MAXLOGR];

	if (logm > MAXLOGR)
	{
		fprintf(stderr, "rfft size too big\n");
		exit(1);
	}

	memset(xi, 0, (1 << logm) * sizeof(xi[0]));

	fft( fft_tables, x, xi, logm);

	memcpy(x + (1 << (logm - 1)), xi, (1 << (logm - 1)) * sizeof(*x));
}
