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
  double powm = 0.25;
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
        target = NOISETONE * pow(avge/avgenrg, powm);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powm);

        target *= 0.9 + (40.0 / (fabs(start + end - MIDF) + 32));
    }
    else
    {
        target = NOISETONE * pow(avge/avgenrg, powm);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powm);

        target *= 0.9 + (40.0 / (0.125 * fabs(start + end - (8*MIDF)) + 32));

        target *= 0.45;
    }
    bandqual[sfb] = 3.5 * target * quality;
  }
}

// use band quality levels to quantize a block
static void qlevel(CoderInfo *coderInfo,
                   const double *xr,
                   int *xi,
                   const double *bandqual, double *pow43)
{
    int sb, cnt;
    int start, end;
    // 1.5dB step
    static const double sfstep = 20.0 / 1.5 / log(10);

    for (sb = 0; sb < coderInfo->nr_of_sfb; sb++)
    {
      double sfacfix;
      int sfac;
      double maxx;
      double rmsx;
      double ein, eout;

      start = coderInfo->sfb_offset[sb];
      end = coderInfo->sfb_offset[sb+1];

      maxx = 0.0;
      rmsx = 0.0;
      for (cnt = start; cnt < end; cnt++)
      {
          double e = xr[cnt] * xr[cnt];
          if (maxx < e)
            maxx = e;
          rmsx += e;
      }
      rmsx /= (end - start);
      rmsx = sqrt(rmsx);
      maxx = sqrt(maxx);

      if (maxx < 10.0)
      {
          for (cnt = start; cnt < end; cnt++)
              xi[cnt] = 0;
          coderInfo->scale_factor[sb] = 10;
          continue;
      }

      ein = eout = 0;
      sfacfix = bandqual[sb] / rmsx;
      for (cnt = start; cnt < end; cnt++)
      {
          double tmp = fabs(xr[cnt]);

          ein += tmp *tmp;

          tmp *= sfacfix;
          tmp = sqrt(tmp * sqrt(tmp));

          xi[cnt] = (int)(tmp + 0.5);

          tmp = pow43[xi[cnt]];
          eout += tmp * tmp;
      }

      if (eout < 1.0)
          sfac = 10;
      else
          sfac = lrint(log(sqrt(eout / ein)) * sfstep);

      coderInfo->scale_factor[sb] = sfac;
    }
}

int BlocQuant(CoderInfo *coderInfo, double *xr, int *xi, double quality, double *pow43)
{
    double bandlvl[MAX_SCFAC_BANDS];
    int cnt;
    int nonzero = 0;

    for (cnt = 0; cnt < FRAME_LEN; cnt++)
        nonzero += (fabs(xr[cnt]) > 1E-20);

    SetMemory(xi, 0, FRAME_LEN*sizeof(xi[0]));
    if (nonzero)
    {
        bmask(coderInfo, xr, bandlvl, quality);
        qlevel(coderInfo, xr, xi, bandlvl, pow43);
        return 1;
    }

    return 0;
}
