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
#include "quantize.h"

#define MAGIC_NUMBER  0.4054
enum {NULL_SF = 0};

// band sound masking
static void bmask(CoderInfo *coderInfo, double *xr, double *bandqual,
                  AACQuantCfg *aacquantCfg)
{
  int sfb, start, end, cnt;
  int *cb_offset = coderInfo->sfb_offset;
  int last;
  double avgenrg = coderInfo->avgenrg;
  double powm = 0.4;
  double quality = (double)aacquantCfg->quality/DEFQUAL;

  last = BLOCK_LEN_LONG;

  for (sfb = 0; sfb < coderInfo->sfbn; sfb++)
  {
    double avge, maxe;
    double target;

    start = cb_offset[sfb];
    end = cb_offset[sfb + 1];

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
    }
    else
    {
        target = NOISETONE * pow(avge/avgenrg, powm);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powm);

        target *= 0.45;
    }

    target *= 10.0 / (1.0 + ((double)(start+end)/last));

    bandqual[sfb] = target * quality;
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
    static const double sfstep = 20.0 / 1.5 / M_LN10;

    for (sb = 0; sb < coderInfo->sfbn; sb++)
    {
      double sfacfix;
      int sfac;
      double maxx;
      double rmsx;

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
          coderInfo->scale_factor[coderInfo->sfcnt++] = NULL_SF;
          continue;
      }

      sfac = lrint(log(bandqual[sb] / rmsx) * sfstep);
      sfacfix = exp(sfac / sfstep);
      coderInfo->scale_factor[coderInfo->sfcnt++] = sfac;

#if defined(__GNUC__) && defined(__SSE2__)
typedef float v4sf __attribute__ ((vector_size (16)));
typedef int v4si __attribute__ ((vector_size (16)));
      if (__builtin_cpu_supports("sse2"))
      {
          static const v4sf zero = {0, 0, 0, 0};
          static const v4sf magic = {MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER};

          for (cnt = start; cnt < end; cnt += 4)
          {
              float fin[4];
              fin[0] = xr[cnt];
              fin[1] = xr[cnt+1];
              fin[2] = xr[cnt+2];
              fin[3] = xr[cnt+3];

              v4sf x = __builtin_ia32_loadups(fin);
              x = __builtin_ia32_maxps(x, __builtin_ia32_subps(zero, x));

              v4sf fix = {sfacfix, sfacfix, sfacfix, sfacfix};
              x = __builtin_ia32_mulps(x, fix);
              x = __builtin_ia32_mulps(x , __builtin_ia32_sqrtps(x));
              x = __builtin_ia32_sqrtps(x);

              x = __builtin_ia32_addps(x, magic);
              v4si vi = __builtin_ia32_cvttps2dq(x);
              memcpy(xi+cnt,&vi,16);
          }

          continue;
      }
#endif

      for (cnt = start; cnt < end; cnt++)
      {
          double tmp = fabs(xr[cnt]);

          tmp *= sfacfix;
          tmp = sqrt(tmp * sqrt(tmp));

          xi[cnt] = (int)(tmp + MAGIC_NUMBER);
      }
    }
}

int BlocQuant(CoderInfo *coderInfo, double *xr, int *xi, AACQuantCfg *aacquantCfg)
{
    double bandlvl[MAX_SCFAC_BANDS];
    int cnt;
    int nonzero = 0;

    coderInfo->sfcnt = 0;

    for (cnt = 0; cnt < FRAME_LEN; cnt++)
        nonzero += (fabs(xr[cnt]) > 1E-20);

    SetMemory(xi, 0, FRAME_LEN*sizeof(xi[0]));
    if (nonzero)
    {
        bmask(coderInfo, xr, bandlvl, aacquantCfg);
        qlevel(coderInfo, xr, xi, bandlvl, aacquantCfg->pow43);
        return 1;
    }

    return 0;
}

void BandLimit(unsigned *bw, int rate, SR_INFO *sr, AACQuantCfg *aacquantCfg)
{
    // find max short frame band
    int max = *bw * (BLOCK_LEN_SHORT << 1) / rate;
    int cnt;
    int l;

    l = 0;
    for (cnt = 0; cnt < sr->num_cb_short; cnt++)
    {
        if (l >= max)
            break;
        l += sr->cb_width_short[cnt];
    }
    aacquantCfg->max_cbs = cnt;
    *bw = (double)l * rate / (BLOCK_LEN_SHORT << 1);

    // find max long frame band
    max = *bw * (BLOCK_LEN_LONG << 1) / rate;
    l = 0;
    for (cnt = 0; cnt < sr->num_cb_long; cnt++)
    {
        if (l >= max)
            break;
        l += sr->cb_width_long[cnt];
    }
    aacquantCfg->max_cbl = cnt;

    *bw = (double)l * rate / (BLOCK_LEN_LONG << 1);
}
