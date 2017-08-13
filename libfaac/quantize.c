/****************************************************************************
    Quantizer core functions
    quality setting, error distribution, etc.

    Copyright (C) 2017 Krzysztof Nikiel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/

#include <math.h>
#include "util.h"

// band sound masking
static void bmask(CoderInfo *coderInfo, double *xr, double *bandqual,
                  double quality)
{
  int sfb, start, end, cnt;
  int last = coderInfo->lastx;
  int lastsb = 0;
  int *cb_offset = coderInfo->sfb_offset;
  int num_cb = coderInfo->nr_of_sfb;
  double avgenrg = coderInfo->avgenrg;
  double pows = 0.16;
  double powl = 0.9 * pows;
  enum {MIDF=34};

  for (sfb = 0; sfb < num_cb; sfb++)
  {
    if (last > cb_offset[sfb])
      lastsb = sfb;
  }

  for (sfb = 0; sfb < num_cb; sfb++)
  {
    double avge, maxe;
    double target;

    start = cb_offset[sfb];
    end = cb_offset[sfb + 1];

    if (sfb > lastsb)
    {
      bandqual[sfb] = 0;
      continue;
    }

    avge = 0.0;
    maxe = 0.0;
    for (cnt = start; cnt < end; cnt++)
    {
        double e = xr[cnt]*xr[cnt];
        avge += e;
        if (maxe < e)
            maxe = e;
    }
    avge /= (end - start);

#define NOISETONE 0.2
    if (coderInfo->block_type == ONLY_SHORT_WINDOW)
    {
        target = NOISETONE * pow(avge/avgenrg, pows);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powl);

        target *= 0.9 + (40.0 / (fabs(start + end - MIDF) + 32));
    }
    else
    {
        target = NOISETONE * pow(avge/avgenrg, pows);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powl);

        target *= 0.9 + (40.0 / (0.125 * fabs(start + end - (8*MIDF)) + 32));

        target *= 0.45;
    }
    bandqual[sfb] = 5.0 * target * quality;
  }
}

// use band quality levels to quantize a block
static void qlevel(CoderInfo *coderInfo,
                   const double *xr34,
                   int *xi,
                   const double *bandqual)
{
    int sb, cnt;
    int start, end;
    const double ifqstep = pow(2.0, 0.1875);
    const double log_ifqstep = 1.0 / log(ifqstep);

    for (sb = 0; sb < coderInfo->nr_of_sfb; sb++)
    {
      double sfacfix;
      int sfac;
      double maxx;

      start = coderInfo->sfb_offset[sb];
      end = coderInfo->sfb_offset[sb+1];

      maxx = 0.0;
      for (cnt = start; cnt < end; cnt++)
      {
          if (xr34[cnt] > maxx)
            maxx = xr34[cnt];
      }

      if (maxx < 10.0)
      {
          for (cnt = start; cnt < end; cnt++)
              xi[cnt] = 0;
          coderInfo->scale_factor[sb] = 10;
          continue;
      }

      sfac = (int)(log(bandqual[sb] / maxx) * log_ifqstep - 0.5);
      coderInfo->scale_factor[sb] = sfac;
      sfacfix = exp(sfac / log_ifqstep);
      for (cnt = start; cnt < end; cnt++)
          xi[cnt] = (int)(sfacfix * xr34[cnt] + 0.5);
    }
}

int BlocQuant(CoderInfo *coderInfo, double *xr, int *xi, double quality)
{
    double xr34[FRAME_LEN];
    double bandlvl[MAX_SCFAC_BANDS];
    int cnt;
    int nonzero = 0;

    for (cnt = 0; cnt < FRAME_LEN; cnt++) {
        double temp = fabs(xr[cnt]);
        xr34[cnt] = sqrt(temp * sqrt(temp));
        nonzero += (temp > 1E-20);
    }

    SetMemory(xi, 0, FRAME_LEN*sizeof(xi[0]));
    if (nonzero)
    {
        bmask(coderInfo, xr, bandlvl, quality);
        qlevel(coderInfo, xr34, xi, bandlvl);
        return 1;
    }

    return 0;
}
