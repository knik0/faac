/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2003 Krzysztof Nikiel
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
#include "channels.h"
#include "util.h"


static void midside(CoderInfo *coder, ChannelInfo *channel,
                    double *sl0, double *sr0, int *mscnt,
                    int wstart, int wend,
                    double mutethr
                   )
{
    int sfb;
    int win;

    for (sfb = 0; sfb < coder->sfbn; sfb++)
    {
        int ms = 0;
        int l, start, end;
        double sum, diff;
        double enrgs, enrgd, enrgl, enrgr;
        double maxs, maxd, maxl, maxr;

        start = coder->sfb_offset[sfb];
        end = coder->sfb_offset[sfb + 1];

        enrgs = enrgd = enrgl = enrgr = 0.0;
        maxs = maxd = maxl = maxr = 0.0;
        for (win = wstart; win < wend; win++)
        {
            double *sl = sl0 + win * BLOCK_LEN_SHORT;
            double *sr = sr0 + win * BLOCK_LEN_SHORT;

            for (l = start; l < end; l++)
            {
                double lx = sl[l];
                double rx = sr[l];

                sum = 0.5 * (lx + rx);
                diff = 0.5 * (lx - rx);

                enrgs += sum * sum;
                maxs = max(maxs, fabs(sum));

                enrgd += diff * diff;
                maxd = max(maxd, fabs(diff));

                enrgl += lx * lx;
                enrgr += rx * rx;

                maxl = max(maxl, fabs(lx));
                maxr = max(maxr, fabs(rx));
            }
        }

        if (min(enrgs, enrgd) < mutethr * max(enrgs, enrgd))
        {
            ms = 1;
            for (win = wstart; win < wend; win++)
            {
                double *sl = sl0 + win * BLOCK_LEN_SHORT;
                double *sr = sr0 + win * BLOCK_LEN_SHORT;
                for (l = start; l < end; l++)
                {
                    sum = sl[l] + sr[l];
                    diff = sl[l] - sr[l];
                    if (enrgs < enrgd)
                        sum = 0.0;
                    else
                        diff = 0.0;
                    sl[l] = 0.5 * sum;
                    sr[l] = 0.5 * diff;
                }
            }
        }
        if (min(enrgl, enrgr) < mutethr * max(enrgl, enrgr))
        {
            for (win = wstart; win < wend; win++)
            {
                double *sl = sl0 + win * BLOCK_LEN_SHORT;
                double *sr = sr0 + win * BLOCK_LEN_SHORT;
                for (l = start; l < end; l++)
                {
                    if (enrgl < enrgr)
                        sl[l] = 0.0;
                    else
                        sr[l] = 0.0;
                }
            }
        }

        channel->msInfo.ms_used[*mscnt] = ms;
        (*mscnt)++;
    }
}


void MSEncode(CoderInfo *coder,
              ChannelInfo *channel,
              double *s[MAX_CHANNELS],
              int maxchan,
              double quality)
{
    int chn;
    int usems;
    double mutethr;
    static const double thr075 = 0.09; /* ~0.75dB */
    static const double thrmax = 0.25; /* ~2dB */

    if (quality > 0.01)
    {
        usems = 1;
        mutethr = thr075 / quality;
        if (mutethr > thrmax)
            mutethr = thrmax;
    }
    else
    {
        usems = 0;
        mutethr = 0.0;
    }

    for (chn = 0; chn < maxchan; chn++)
    {
        int rch;
        int cnt;
        int group;
        int mscnt = 0;
        int start = 0;

        if (!channel[chn].present)
            continue;
        if (!((channel[chn].cpe) && (channel[chn].ch_is_left)))
            continue;

        rch = channel[chn].paired_ch;

        channel[chn].msInfo.is_present = 0;
        channel[rch].msInfo.is_present = 0;

        if (!usems)
            continue;

        if (coder[chn].block_type != coder[rch].block_type)
            continue;
        if (coder[chn].groups.n != coder[rch].groups.n)
            continue;
        for (cnt = 0; cnt < coder[chn].groups.n; cnt++)
            if (coder[chn].groups.len[cnt] != coder[rch].groups.len[cnt])
                goto skip;

        channel[chn].common_window = 1;
        channel[chn].msInfo.is_present = 1;
        channel[rch].msInfo.is_present = 1;

        for (group = 0; group < coder->groups.n; group++)
        {
            int end = start + coder->groups.len[group];
            midside(coder + chn, channel + chn, s[chn], s[rch], &mscnt,
                    start, end, mutethr);
            start = end;
        }
        skip:;
    }
}
