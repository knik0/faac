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
#include <stdio.h>
#include "quantize.h"
#include "huff2.h"

#ifdef HAVE_IMMINTRIN_H
# include <immintrin.h>
#endif

#ifdef __SSE2__
# ifdef __GNUC__
#  include <cpuid.h>
# endif
#endif

#ifdef _MSC_VER
# include <immintrin.h>
# include <intrin.h>
# define __SSE2__
# define bit_SSE2 (1 << 26)
#endif

#define MAGIC_NUMBER  0.4054
#define NOISEFLOOR 0.4

// band sound masking
static void bmask(CoderInfo *coderInfo, double *xr0, double *bandqual,
                  int gnum, double quality)
{
  int sfb, start, end, cnt;
  int *cb_offset = coderInfo->sfb_offset;
  int last;
  double avgenrg;
  double powm = 0.4;
  double totenrg = 0.0;
  int gsize = coderInfo->groups.len[gnum];
  double *xr;
  int win;
  int enrgcnt = 0;


  for (sfb = 0; sfb < coderInfo->sfbn; sfb++)
  {
      start = coderInfo->sfb_offset[sfb];
      end = coderInfo->sfb_offset[sfb + 1];

      xr = xr0;
      for (win = 0; win < gsize; win++)
      {
          for (cnt = start; cnt < end; cnt++)
          {
              totenrg += xr[cnt] * xr[cnt];
              enrgcnt++;
          }

          xr += BLOCK_LEN_SHORT;
      }
  }

  if (totenrg < ((NOISEFLOOR * NOISEFLOOR) * (double)enrgcnt))
  {
      for (sfb = 0; sfb < coderInfo->sfbn; sfb++)
          bandqual[sfb] = 0.0;

      return;
  }

  for (sfb = 0; sfb < coderInfo->sfbn; sfb++)
  {
    double avge, maxe;
    double target;

    start = cb_offset[sfb];
    end = cb_offset[sfb + 1];

    avge = 0.0;
    maxe = 0.0;
    xr = xr0;
    for (win = 0; win < gsize; win++)
    {
        for (cnt = start; cnt < end; cnt++)
        {
            double e = xr[cnt]*xr[cnt];
            avge += e;
            if (maxe < e)
                maxe = e;
        }
        xr += BLOCK_LEN_SHORT;
    }
    maxe *= gsize;

#define NOISETONE 0.2
    if (coderInfo->block_type == ONLY_SHORT_WINDOW)
    {
        last = BLOCK_LEN_SHORT;
        avgenrg = totenrg / last;
        avgenrg *= end - start;

        target = NOISETONE * pow(avge/avgenrg, powm);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powm);

        target *= 1.5;
    }
    else
    {
        last = BLOCK_LEN_LONG;
        avgenrg = totenrg / last;
        avgenrg *= end - start;

        target = NOISETONE * pow(avge/avgenrg, powm);
        target += (1.0 - NOISETONE) * 0.45 * pow(maxe/avgenrg, powm);
    }

    target *= 10.0 / (1.0 + ((double)(start+end)/last));

    bandqual[sfb] = target * quality;
  }
}

enum {MAXSHORTBAND = 36};
// use band quality levels to quantize a group of windows
static void qlevel(CoderInfo *coderInfo,
                   const double *xr0,
                   const double *bandqual,
                   int gnum,
                   int pnslevel
                  )
{
    int sb, cnt;
#ifndef __clang__
    /* 2^0.25 (1.50515 dB) step from AAC specs */
    static const double sfstep = 1.0 / log10(sqrt(sqrt(2.0)));
#else
    static const double sfstep = 20 / 1.50515;
#endif
    int gsize = coderInfo->groups.len[gnum];
    double pnsthr = 0.1 * pnslevel;
#ifdef __SSE2__
    int cpuid[4];
    int sse2 = 0;

    cpuid[3] = 0;
# ifdef __GNUC__
    __cpuid(1, cpuid[0], cpuid[1], cpuid[2], cpuid[3]);
# endif
# ifdef _MSC_VER
    __cpuid(cpuid, 1);
# endif
    if (cpuid[3] & bit_SSE2)
        sse2 = 1;
#endif

    for (sb = 0; sb < coderInfo->sfbn; sb++)
    {
      double sfacfix;
      int sfac;
      double rmsx;
      double etot;
      int xitab[8 * MAXSHORTBAND];
      int *xi;
      int start, end;
      const double *xr;
      int win;

      if (coderInfo->book[coderInfo->bandcnt] != HCB_NONE)
      {
          coderInfo->bandcnt++;
          continue;
      }

      start = coderInfo->sfb_offset[sb];
      end = coderInfo->sfb_offset[sb+1];

      etot = 0.0;
      xr = xr0;
      for (win = 0; win < gsize; win++)
      {
          for (cnt = start; cnt < end; cnt++)
          {
              double e = xr[cnt] * xr[cnt];
              etot += e;
          }
          xr += BLOCK_LEN_SHORT;
      }
      etot /= (double)gsize;
      rmsx = sqrt(etot / (end - start));

      if ((rmsx < NOISEFLOOR) || (!bandqual[sb]))
      {
          coderInfo->book[coderInfo->bandcnt++] = HCB_ZERO;
          continue;
      }

#ifndef DRM
      if (bandqual[sb] < pnsthr)
      {
          coderInfo->book[coderInfo->bandcnt] = HCB_PNS;
          coderInfo->sf[coderInfo->bandcnt] +=
              lrint(log10(etot) * (0.5 * sfstep));
          coderInfo->bandcnt++;
          continue;
      }
#endif

      sfac = lrint(log10(bandqual[sb] / rmsx) * sfstep);
      if ((SF_OFFSET - sfac) < 10)
          sfacfix = 0.0;
      else
          sfacfix = pow(10, sfac / sfstep);

      xr = xr0 + start;
      end -= start;
      xi = xitab;
      for (win = 0; win < gsize; win++)
      {
#ifdef __SSE2__
          if (sse2)
          {
              for (cnt = 0; cnt < end; cnt += 4)
              {
                  __m128 x = {xr[cnt], xr[cnt + 1], xr[cnt + 2], xr[cnt + 3]};

                  x = _mm_max_ps(x, -x);
                  x *= (__m128){sfacfix, sfacfix, sfacfix, sfacfix};
                  x *= _mm_sqrt_ps(x);
                  x = _mm_sqrt_ps(x);
                  x += (__m128){MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER};

                  *(__m128i*)(xi + cnt) = _mm_cvttps_epi32(x);
              }
              for (cnt = 0; cnt < end; cnt++)
              {
                  if (xr[cnt] < 0)
                      xi[cnt] = -xi[cnt];
              }
              xi += cnt;
              xr += BLOCK_LEN_SHORT;
              continue;
          }
#endif

          for (cnt = 0; cnt < end; cnt++)
          {
              double tmp = fabs(xr[cnt]);

              tmp *= sfacfix;
              tmp = sqrt(tmp * sqrt(tmp));

              xi[cnt] = (int)(tmp + MAGIC_NUMBER);
              if (xr[cnt] < 0)
                  xi[cnt] = -xi[cnt];
          }
          xi += cnt;
          xr += BLOCK_LEN_SHORT;
      }
      huffbook(coderInfo, xitab, gsize * end);
      coderInfo->sf[coderInfo->bandcnt++] += SF_OFFSET - sfac;
    }
}

int BlocQuant(CoderInfo *coder, double *xr, AACQuantCfg *aacquantCfg)
{
    double bandlvl[MAX_SCFAC_BANDS];
    int cnt;
    double *gxr;

    coder->global_gain = 0;

    coder->bandcnt = 0;
    coder->datacnt = 0;
#ifdef DRM
    coder->iLenReordSpData = 0; /* init length of reordered spectral data */
    coder->iLenLongestCW = 0; /* init length of longest codeword */
    coder->cur_cw = 0; /* init codeword counter */
#endif

    {
        int lastis;
        int lastsf;

        gxr = xr;
        for (cnt = 0; cnt < coder->groups.n; cnt++)
        {
            bmask(coder, gxr, bandlvl, cnt,
                  (double)aacquantCfg->quality/DEFQUAL);
            qlevel(coder, gxr, bandlvl, cnt, aacquantCfg->pnslevel);
            gxr += coder->groups.len[cnt] * BLOCK_LEN_SHORT;
        }

        coder->global_gain = 0;
        for (cnt = 0; cnt < coder->bandcnt; cnt++)
        {
            int book = coder->book[cnt];
            if (!book)
                continue;
            if ((book != HCB_INTENSITY) && (book != HCB_INTENSITY2))
            {
                coder->global_gain = coder->sf[cnt];
                break;
            }
        }

        lastsf = coder->global_gain;
        lastis = 0;
        // fixme: move SF range check to quantizer
        for (cnt = 0; cnt < coder->bandcnt; cnt++)
        {
            int book = coder->book[cnt];
            if ((book == HCB_INTENSITY) || (book == HCB_INTENSITY2))
            {
                int diff = coder->sf[cnt] - lastis;

                if (diff < -60)
                    diff = -60;
                if (diff > 60)
                    diff = 60;

                lastis += diff;
                coder->sf[cnt] = lastis;
            }
            else if (book != HCB_PNS)
            {
                int diff = coder->sf[cnt] - lastsf;

                if (diff < -60)
                    diff = -60;
                if (diff > 60)
                    diff = 60;

                lastsf += diff;
                coder->sf[cnt] = lastsf;
            }
        }
        return 1;
    }
    return 0;
}

void CalcBW(unsigned *bw, int rate, SR_INFO *sr, AACQuantCfg *aacquantCfg)
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
    if (aacquantCfg->pnslevel)
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
    aacquantCfg->max_l = l;

    *bw = (double)l * rate / (BLOCK_LEN_LONG << 1);
}

enum {MINSFB = 2};

static void calce(double *xr, int *bands, double e[NSFB_SHORT], int maxsfb,
                  int maxl)
{
    int sfb;
    int l;

    // mute lines above cutoff freq
    for (l = maxl; l < bands[maxsfb]; l++)
        xr[l] = 0.0;

    for (sfb = MINSFB; sfb < maxsfb; sfb++)
    {
        e[sfb] = 0;
        for (l = bands[sfb]; l < bands[sfb + 1]; l++)
            e[sfb] += xr[l] * xr[l];
    }
}

static void resete(double min[NSFB_SHORT], double max[NSFB_SHORT],
                   double e[NSFB_SHORT], int maxsfb)
{
    int sfb;
    for (sfb = MINSFB; sfb < maxsfb; sfb++)
        min[sfb] = max[sfb] = e[sfb];
}

#define PRINTSTAT 0
#if PRINTSTAT
static int groups = 0;
static int frames = 0;
#endif
void BlocGroup(double *xr, CoderInfo *coderInfo, AACQuantCfg *cfg)
{
    int win, sfb;
    double e[NSFB_SHORT];
    double min[NSFB_SHORT];
    double max[NSFB_SHORT];
    const double thr = 3.0;
    int win0;
    int fastmin;
    int maxsfb, maxl;

    if (coderInfo->block_type != ONLY_SHORT_WINDOW)
    {
        coderInfo->groups.n = 1;
        coderInfo->groups.len[0] = 1;
        return;
    }

    maxl = cfg->max_l / 8;
    maxsfb = cfg->max_cbs;
    fastmin = ((maxsfb - MINSFB) * 3) >> 2;

#ifdef DRM
    coderInfo->groups.n = 1;
    coderInfo->groups.len[0] = 8;
    return;
#endif

#if PRINTSTAT
    frames++;
#endif
    calce(xr, coderInfo->sfb_offset, e, maxsfb, maxl);
    resete(min, max, e, maxsfb);
    win0 = 0;
    coderInfo->groups.n = 0;
    for (win = 1; win < MAX_SHORT_WINDOWS; win++)
    {
        int fast = 0;

        calce(xr + win * BLOCK_LEN_SHORT, coderInfo->sfb_offset, e, maxsfb, maxl);
        for (sfb = MINSFB; sfb < maxsfb; sfb++)
        {
            if (min[sfb] > e[sfb])
                min[sfb] = e[sfb];
            if (max[sfb] < e[sfb])
                max[sfb] = e[sfb];

            if (max[sfb] > thr * min[sfb])
                fast++;
        }
        if (fast > fastmin)
        {
            coderInfo->groups.len[coderInfo->groups.n++] = win - win0;
            win0 = win;
            resete(min, max, e, maxsfb);
        }
    }
    coderInfo->groups.len[coderInfo->groups.n++] = win - win0;
#if PRINTSTAT
    groups += coderInfo->groups.n;
#endif
}

void BlocStat(void)
{
#if PRINTSTAT
    printf("frames:%d; groups:%d; g/f:%f\n", frames, groups, (double)groups/frames);
#endif
}
