/*
 * FAAC - Freeware Advanced Audio Coder
 * $Id: fft.c,v 1.7 2002/08/21 16:52:25 knik Exp $
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

#include "fft.h"
#include "util.h"

#define MAXLOGM 11

typedef float fftfloat;

static fftfloat *costbl[MAXLOGM + 1] = {NULL};	// size/2
static fftfloat *negsintbl[MAXLOGM + 1] = {NULL};	// size/2
static unsigned short *reordertbl[MAXLOGM + 1] = {NULL};	//size

static void reorder(double *x, int logm)
{
  int i;
  int size = 1 << logm;
  unsigned short *r;	//size


  if (!reordertbl[logm])
    // create bit reversing table
  {
    reordertbl[logm] = AllocMemory(size * sizeof(*(reordertbl[0])));

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
      reordertbl[logm][i] = reversed;
    }
  }

  r = reordertbl[logm];

  for (i = 0; i < size; i++)
  {
    int j = r[i];
    double tmp;

    if (j <= i)
      continue;

    tmp = x[i];
    x[i] = x[j];
    x[j] = tmp;
  }
}

static void fft_proc(double *xr, double *xi,
		     fftfloat *refac, fftfloat *imfac, int size)
{
  int step, shift, pos;
  int exp, estep;

  estep = size;
  for (step = 1; step < size; step *= 2)
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
	double v2r, v2i;

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

static void check_tables(int logm)
{

  if (!(costbl[logm]))
  {
    int i;
    int size = 1 << logm;

    if (negsintbl[logm])
      FreeMemory(negsintbl[logm]);

    costbl[logm] = AllocMemory((size / 2) * sizeof(*(costbl[0])));
    negsintbl[logm] = AllocMemory((size / 2) * sizeof(*(negsintbl[0])));

    for (i = 0; i < (size >> 1); i++)
    {
      double theta = 2.0 * M_PI * ((double) i) / (double) size;
      costbl[logm][i] = cos(theta);
      negsintbl[logm][i] = -sin(theta);
    }
  }
}

void fft(double *xr, double *xi, int logm)
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

  check_tables(logm);

  reorder(xr, logm);
  reorder(xi, logm);

  fft_proc(xr, xi, costbl[logm], negsintbl[logm], 1 << logm);
}

void rfft(double *x, int logm)
{
   static double xi[1 << MAXLOGM];

   if (logm > MAXLOGM)
   {
     fprintf(stderr, "fft size too big\n");
     exit(1);
   }

   memset(xi, 0, (1 << logm) * sizeof(xi[0]));

   fft(x, xi, logm);

   memcpy(x + (1 << (logm - 1)), xi, (1 << (logm - 1)) * sizeof(*x));
}

void ffti(double *xr, double *xi, int logm)
{
  int i, size;
  double fac;
  double *xrp, *xip;

  fft(xi, xr, logm);

  size = 1 << logm;
  fac = 1.0 / size;
  xrp = xr;
  xip = xi;
  for (i = 0; i < size; i++)
  {
    *xrp++ *= fac;
    *xip++ *= fac;
  }
}

/*
$Log: fft.c,v $
Revision 1.7  2002/08/21 16:52:25  knik
new simplier and faster fft routine and correct real fft
new real fft is just a complex fft wrapper so it is slower than optimal but
by surprise it seems to be at least as fast as the old buggy function

*/
