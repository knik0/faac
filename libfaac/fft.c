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
 * $Id: fft.c,v 1.2 2001/02/04 17:50:47 oxygene2000 Exp $
 */

#include <math.h>
#include <stdlib.h>

#include "fft.h"


#define  MAXLOGM     11    /* max FFT length = 2^MAXLOGM */
#define  TWOPI       6.28318530717958647692
#define  SQHALF      0.707106781186547524401

static   double    *tabr[MAXLOGM]; /* tables of butterfly angles */

static void build_table(int logm)
{
   static   int      m, m2, m4, m8, nel, n;
   static   double    *cn, *spcn, *smcn;
   static   double    ang, c, s;
 
   /* Compute a few constants */
   m = 1 << logm; m2 = m / 2; m4 = m2 / 2; m8 = m4 /2;
 
   /* Allocate memory for tables */
   nel = m4 - 2;
   tabr[logm-4] = (double *) calloc(3 * nel, sizeof(double));
/*
   if ((tab[logm-4] = (double *) calloc(3 * nel, sizeof(double))) == NULL) {
      printf("Error : RSFFT : not enough memory for cosine tables.\n");
      error_exit();
   }
*/
 
   /* Initialize pointers */
   cn  = tabr[logm-4]; spcn  = cn + nel;  smcn  = spcn + nel;
 
   /* Compute tables */
   for (n = 1; n < m4; n++) {
      if (n == m8) continue;
      ang = n * TWOPI / m;
      c = cos(ang); s = sin(ang);
      *cn++ = c; *spcn++ = - (s + c); *smcn++ = s - c;
   }
}
 
/*--------------------------------------------------------------------*
 *    Recursive part of the RSFFT algorithm.  Not externally          *
 *    callable.                                                       *
 *--------------------------------------------------------------------*/
 
static void rsrec(double *x, int logm)
{
   static   int      m, m2, m4, m8, nel, n;
   static   double    *xr1, *xr2, *xi1;
   static   double    *cn, *spcn, *smcn;
   static   double    tmp1, tmp2;
 
   /* Compute trivial cases */
   if (logm < 2) {
      if (logm == 1) {   /* length m = 2 */
         xr2  = x + 1;
         tmp1 = *x + *xr2;
         *xr2 = *x - *xr2;
         *x   = tmp1;
         return;
      }
      else if (logm == 0) return;   /* length m = 1 */
   }
 
   /* Compute a few constants */
   m = 1 << logm; m2 = m / 2; m4 = m2 / 2; m8 = m4 /2;
 
   /* Build tables of butterfly coefficients, if necessary */
   if ((logm >= 4) && (tabr[logm-4] == NULL))
      build_table(logm);
 
   /* Step 1 */
   xr1 = x; xr2 = xr1 + m2;
   for (n = 0; n < m2; n++) {
      tmp1 = *xr1 + *xr2;
      *xr2 = *xr1 - *xr2;
      *xr1 = tmp1;
      xr1++; xr2++;
   }
 
   /* Step 2 */
   xr1 = x + m2 + m4;
   for (n = 0; n < m4; n++) {
      *xr1 = - *xr1;
      xr1++;
   }
 
   /* Step 3: multiplication by W_M^n */
   xr1 = x + m2; xi1 = xr1 + m4;
   if (logm >= 4) {
      nel = m4 - 2;
      cn  = tabr[logm-4]; spcn  = cn + nel;  smcn  = spcn + nel;
   }
   xr1++; xi1++;
   for (n = 1; n < m4; n++) {
      if (n == m8) {
         tmp1 =  SQHALF * (*xr1 + *xi1);
         *xi1 =  SQHALF * (*xi1 - *xr1);
         *xr1 =  tmp1;
      } else {
         tmp2 = *cn++ * (*xr1 + *xi1);
         tmp1 = *spcn++ * *xr1 + tmp2;
         *xr1 = *smcn++ * *xi1 + tmp2;
         *xi1 = tmp1;
      }
      xr1++; xi1++;
   }
 
   /* Call rsrec again with half DFT length */
   rsrec(x, logm-1);
 
   /* Step 4: Call complex DFT routine, with quarter DFT length.
      Constants have to be recomputed, because they are static! */
   m = 1 << logm; m2 = m / 2; m4 = 3 * (m / 4);
   srrec(x + m2, x + m4, logm-2);
 
   /* Step 5: sign change & data reordering */
   m = 1 << logm; m2 = m / 2; m4 = m2 / 2; m8 = m4 / 2;
   xr1 = x + m2 + m4;
   xr2 = x + m - 1;
   for (n = 0; n < m8; n++) {
      tmp1   =    *xr1;
      *xr1++ =  - *xr2;
      *xr2-- =  - tmp1;
   }
   xr1 = x + m2 + 1;
   xr2 = x + m - 2;
   for (n = 0; n < m8; n++) {
      tmp1   =    *xr1;
      *xr1++ =  - *xr2;
      *xr2-- =    tmp1;
      xr1++;
      xr2--;
   }
   if (logm == 2) x[3] = -x[3];
}
 
/*--------------------------------------------------------------------*
 *    Direct transform for real inputs                                *
 *--------------------------------------------------------------------*/
 
/*--------------------------------------------------------------------*
 *    Data unshuffling according to bit-reversed indexing.            *
 *                                                                    *
 *    Bit reversal is done using Evan's algorithm (Ref: D. M. W.      *
 *    Evans, "An improved digit-reversal permutation algorithm ...",  *
 *    IEEE Trans. ASSP, Aug. 1987, pp. 1120-1125).                    *
 *--------------------------------------------------------------------*/
static   int   brseed[256];   /* Evans' seed table */
static   int   brsflg;        /* flag for table building */
 
static void BR_permute(double *x, int logm)
{
   int      i, j, imax, lg2, n;
   int      off, fj, gno, *brp;
   double    tmp, *xp, *xq;
 
   lg2 = logm >> 1;
   n = 1 << lg2;
   if (logm & 1) lg2++;
 
   /* Create seed table if not yet built */
   if (brsflg != logm) {
      brsflg = logm;
      brseed[0] = 0;
      brseed[1] = 1;
      for (j = 2; j <= lg2; j++) {
         imax = 1 << (j - 1);
         for (i = 0; i < imax; i++) {
            brseed[i] <<= 1;
            brseed[i + imax] = brseed[i] + 1;
         }
      }
   }
 
   /* Unshuffling loop */
   for (off = 1; off < n; off++) {
      fj = n * brseed[off]; i = off; j = fj;
      tmp = x[i]; x[i] = x[j]; x[j] = tmp;
      xp = &x[i];
      brp = &brseed[1];
      for (gno = 1; gno < brseed[off]; gno++) {
         xp += n;
         j = fj + *brp++;
         xq = x + j;
         tmp = *xp; *xp = *xq; *xq = tmp;
      }
   }
}
 
/*--------------------------------------------------------------------*
 *    Recursive part of the SRFFT algorithm.                          *
 *--------------------------------------------------------------------*/
 
static void srrec(double *xr, double *xi, int logm)
{
   static   int      m, m2, m4, m8, nel, n;
   static   double    *xr1, *xr2, *xi1, *xi2;
   static   double    *cn, *spcn, *smcn, *c3n, *spc3n, *smc3n;
   static   double    tmp1, tmp2, ang, c, s;
   static   double    *tab[MAXLOGM];
 
   /* Compute trivial cases */
   if (logm < 3) {
      if (logm == 2) {  /* length m = 4 */
         xr2  = xr + 2;
         xi2  = xi + 2;
         tmp1 = *xr + *xr2;
         *xr2 = *xr - *xr2;
         *xr  = tmp1;
         tmp1 = *xi + *xi2;
         *xi2 = *xi - *xi2;
         *xi  = tmp1;
         xr1  = xr + 1;
         xi1  = xi + 1;
         xr2++;
         xi2++;
         tmp1 = *xr1 + *xr2;
         *xr2 = *xr1 - *xr2;
         *xr1 = tmp1;
         tmp1 = *xi1 + *xi2;
         *xi2 = *xi1 - *xi2;
         *xi1 = tmp1;
         xr2  = xr + 1;
         xi2  = xi + 1;
         tmp1 = *xr + *xr2;
         *xr2 = *xr - *xr2;
         *xr  = tmp1;
         tmp1 = *xi + *xi2;
         *xi2 = *xi - *xi2;
         *xi  = tmp1;
         xr1  = xr + 2;
         xi1  = xi + 2;
         xr2  = xr + 3;
         xi2  = xi + 3;
         tmp1 = *xr1 + *xi2;
         tmp2 = *xi1 + *xr2;
         *xi1 = *xi1 - *xr2;
         *xr2 = *xr1 - *xi2;
         *xr1 = tmp1;
         *xi2 = tmp2;
         return;
      }
      else if (logm == 1) {   /* length m = 2 */
         xr2  = xr + 1;
         xi2  = xi + 1;
         tmp1 = *xr + *xr2;
         *xr2 = *xr - *xr2;
         *xr  = tmp1;
         tmp1 = *xi + *xi2;
         *xi2 = *xi - *xi2;
         *xi  = tmp1;
         return;
      }
      else if (logm == 0) return;   /* length m = 1 */
   }
 
   /* Compute a few constants */
   m = 1 << logm; m2 = m / 2; m4 = m2 / 2; m8 = m4 /2;
 
   /* Build tables of butterfly coefficients, if necessary */
   if ((logm >= 4) && (tab[logm-4] == NULL)) {
 
      /* Allocate memory for tables */
      nel = m4 - 2;
	  tab[logm-4] = (double *) calloc(6 * nel, sizeof(double));
/*
      if ((tab[logm-4] = (double *) calloc(6 * nel, sizeof(double))) == NULL) {
         error_exit();
      }
*/
 
      /* Initialize pointers */
      cn  = tab[logm-4]; spcn  = cn + nel;  smcn  = spcn + nel;
      c3n = smcn + nel;  spc3n = c3n + nel; smc3n = spc3n + nel;
 
      /* Compute tables */
      for (n = 1; n < m4; n++) {
         if (n == m8) continue;
         ang = n * TWOPI / m;
         c = cos(ang); s = sin(ang);
         *cn++ = c; *spcn++ = - (s + c); *smcn++ = s - c;
         ang = 3 * n * TWOPI / m;
         c = cos(ang); s = sin(ang);
         *c3n++ = c; *spc3n++ = - (s + c); *smc3n++ = s - c;
      }
   }
 
   /* Step 1 */
   xr1 = xr; xr2 = xr1 + m2;
   xi1 = xi; xi2 = xi1 + m2;
   for (n = 0; n < m2; n++) {
      tmp1 = *xr1 + *xr2;
      *xr2 = *xr1 - *xr2;
      *xr1 = tmp1;
      tmp2 = *xi1 + *xi2;
      *xi2 = *xi1 - *xi2;
      *xi1 = tmp2;
      xr1++; xr2++; xi1++; xi2++;
   }
 
   /* Step 2 */
   xr1 = xr + m2; xr2 = xr1 + m4;
   xi1 = xi + m2; xi2 = xi1 + m4;
   for (n = 0; n < m4; n++) {
      tmp1 = *xr1 + *xi2;
      tmp2 = *xi1 + *xr2;
      *xi1 = *xi1 - *xr2;
      *xr2 = *xr1 - *xi2;
      *xr1 = tmp1;
      *xi2 = tmp2;
      xr1++; xr2++; xi1++; xi2++;
   }
 
   /* Steps 3 & 4 */
   xr1 = xr + m2; xr2 = xr1 + m4;
   xi1 = xi + m2; xi2 = xi1 + m4;
   if (logm >= 4) {
      nel = m4 - 2;
      cn  = tab[logm-4]; spcn  = cn + nel;  smcn  = spcn + nel;
      c3n = smcn + nel;  spc3n = c3n + nel; smc3n = spc3n + nel;
   }
   xr1++; xr2++; xi1++; xi2++;
   for (n = 1; n < m4; n++) {
      if (n == m8) {
         tmp1 =  SQHALF * (*xr1 + *xi1);
         *xi1 =  SQHALF * (*xi1 - *xr1);
         *xr1 =  tmp1;
         tmp2 =  SQHALF * (*xi2 - *xr2);
         *xi2 = -SQHALF * (*xr2 + *xi2);
         *xr2 =  tmp2;
      } else {
         tmp2 = *cn++ * (*xr1 + *xi1);
         tmp1 = *spcn++ * *xr1 + tmp2;
         *xr1 = *smcn++ * *xi1 + tmp2;
         *xi1 = tmp1;
         tmp2 = *c3n++ * (*xr2 + *xi2);
         tmp1 = *spc3n++ * *xr2 + tmp2;
         *xr2 = *smc3n++ * *xi2 + tmp2;
         *xi2 = tmp1;
      }
      xr1++; xr2++; xi1++; xi2++;
   }
 
   /* Call ssrec again with half DFT length */
   srrec(xr, xi, logm-1);
 
   /* Call ssrec again twice with one quarter DFT length.
      Constants have to be recomputed, because they are static! */
   m = 1 << logm; m2 = m / 2;
   srrec(xr + m2, xi + m2, logm-2);
   m = 1 << logm; m4 = 3 * (m / 4);
   srrec(xr + m4, xi + m4, logm-2);
}
 
/*--------------------------------------------------------------------*
 *    Direct transform                                                *
 *--------------------------------------------------------------------*/
void  srfft(double *xr, double *xi, int logm)
{
   /* Call recursive routine */
   srrec(xr, xi, logm);
 
   /* Output array unshuffling using bit-reversed indices */
   if (logm > 1) {
      BR_permute(xr, logm);
      BR_permute(xi, logm);
   }
}
 
/*--------------------------------------------------------------------*
 *    Inverse transform.  Uses Duhamel's trick (Ref: P. Duhamel       *
 *    et. al., "On computing the inverse DFT", IEEE Trans. ASSP,      *
 *    Feb. 1988, pp. 285-286).                                        *
 *--------------------------------------------------------------------*/
void srifft(double *xr, double *xi, int logm)
{
   int      i, m;
   double    fac, *xrp, *xip;
 
   /* Call direct FFT, swapping real & imaginary addresses */
   srfft(xi, xr, logm);
 
   /* Normalization */
   m = 1 << logm;
   fac = 1.0 / m;
   xrp = xr; xip = xi;
   for (i = 0; i < m; i++) {
      *xrp++ *= fac;
      *xip++ *= fac;
   }
}

void rsfft(double *x, int logm)
{
   /* Call recursive routine */
   rsrec(x, logm);
 
   /* Output array unshuffling using bit-reversed indices */
   if (logm > 1) {
      BR_permute(x, logm);
   }
}
