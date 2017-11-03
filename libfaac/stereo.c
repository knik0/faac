/****************************************************************************
    Intensity Stereo

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
#include "stereo.h"
#include "huff2.h"


static void stereo(CoderInfo *cl, CoderInfo *cr,
                   double *sl0, double *sr0, int *sfcnt,
                   int wstart, int wend, double phthr
                  )
{
    int sfb;
    int win;
    int sfmin;

    if (!phthr)
        return;

    phthr = 1.0 / phthr;

    if (cl->block_type == ONLY_SHORT_WINDOW)
        sfmin = 1;
    else
        sfmin = 8;

    (*sfcnt) += sfmin;

    for (sfb = sfmin; sfb < cl->sfbn; sfb++)
    {
        int l, start, end;
        double sum, diff;
        double enrgs, enrgd, enrgl, enrgr;
        int hcb = HCB_NONE;
        const double step = 10/1.50515;
        double ethr;
        double vfix, efix;

        start = cl->sfb_offset[sfb];
        end = cl->sfb_offset[sfb + 1];

        enrgs = enrgd = enrgl = enrgr = 0.0;
        for (win = wstart; win < wend; win++)
        {
            double *sl = sl0 + win * BLOCK_LEN_SHORT;
            double *sr = sr0 + win * BLOCK_LEN_SHORT;

            for (l = start; l < end; l++)
            {
                double lx = sl[l];
                double rx = sr[l];

                sum = lx + rx;
                diff = lx - rx;

                enrgs += sum * sum;
                enrgd += diff * diff;
                enrgl += lx * lx;
                enrgr += rx * rx;
            }
        }

        ethr = sqrt(enrgl) + sqrt(enrgr);
        ethr *= ethr;
        ethr *= phthr;
        efix = enrgl + enrgr;
        if (enrgs >= ethr)
        {
            hcb = HCB_INTENSITY;
            vfix = sqrt(efix / enrgs);
        }
        else if (enrgd >= ethr)
        {
            hcb = HCB_INTENSITY2;
            vfix = sqrt(efix / enrgd);
        }

        if (hcb != HCB_NONE)
        {
            int sf = lrint(log10(enrgl / efix) * step);
            int pan = lrint(log10(enrgr/efix) * step) - sf;

            if (pan > 30)
            {
                cl->book[*sfcnt] = HCB_ZERO;
                (*sfcnt)++;
                continue;
            }
            if (pan < -30)
            {
                cr->book[*sfcnt] = HCB_ZERO;
                (*sfcnt)++;
                continue;
            }
            cl->sf[*sfcnt] = sf;
            cr->sf[*sfcnt] = -pan;
            cr->book[*sfcnt] = hcb;

            for (win = wstart; win < wend; win++)
            {
                double *sl = sl0 + win * BLOCK_LEN_SHORT;
                double *sr = sr0 + win * BLOCK_LEN_SHORT;
                for (l = start; l < end; l++)
                {
                    if (hcb == HCB_INTENSITY)
                        sum = sl[l] + sr[l];
                    else
                        sum = sl[l] - sr[l];

                    sl[l] = sum * vfix;
                }
            }
        }
        (*sfcnt)++;
    }
}

static void midside(CoderInfo *coder, ChannelInfo *channel,
                    double *sl0, double *sr0, int *sfcnt,
                    int wstart, int wend,
                    double thrmid, double thrside
                   )
{
    int sfb;
    int win;
    int sfmin;

    if (coder->block_type == ONLY_SHORT_WINDOW)
        sfmin = 1;
    else
        sfmin = 8;

    for (sfb = 0; sfb < sfmin; sfb++)
    {
        channel->msInfo.ms_used[*sfcnt] = 0;
        (*sfcnt)++;
    }
    for (sfb = sfmin; sfb < coder->sfbn; sfb++)
    {
        int ms = 0;
        int l, start, end;
        double sum, diff;
        double enrgs, enrgd, enrgl, enrgr;

        start = coder->sfb_offset[sfb];
        end = coder->sfb_offset[sfb + 1];

        enrgs = enrgd = enrgl = enrgr = 0.0;
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
                enrgd += diff * diff;
                enrgl += lx * lx;
                enrgr += rx * rx;
            }
        }

        if ((min(enrgl, enrgr) * thrmid) >= max(enrgs, enrgd))
        {
            enum {PH_NONE, PH_IN, PH_OUT};
            int phase = PH_NONE;

            if ((enrgs * thrmid * 2.0) >= (enrgl + enrgr))
            {
                ms = 1;
                phase = PH_IN;
            }
            else if ((enrgd * thrmid * 2.0) >= (enrgl + enrgr))
            {
                ms = 1;
                phase = PH_OUT;
            }

            if (ms)
            {
                for (win = wstart; win < wend; win++)
                {
                    double *sl = sl0 + win * BLOCK_LEN_SHORT;
                    double *sr = sr0 + win * BLOCK_LEN_SHORT;
                    for (l = start; l < end; l++)
                    {
                        if (phase == PH_IN)
                        {
                            sum = sl[l] + sr[l];
                            diff = 0;
                        }
                        else
                        {
                            sum = 0;
                            diff = sl[l] - sr[l];
                        }

                        sl[l] = 0.5 * sum;
                        sr[l] = 0.5 * diff;
                    }
                }
            }
        }

        if (min(enrgl, enrgr) <= (thrside * max(enrgl, enrgr)))
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

        channel->msInfo.ms_used[*sfcnt] = ms;
        (*sfcnt)++;
    }
}


void AACstereo(CoderInfo *coder,
               ChannelInfo *channel,
               double *s[MAX_CHANNELS],
               int maxchan,
               double quality,
               int mode
              )
{
    int chn;
    static const double thr075 = 1.09 /* ~0.75dB */ - 1.0;
    static const double thrmax = 1.25 /* ~2dB */ - 1.0;
    static const double sidemin = 0.1; /* -20dB */
    static const double sidemax = 0.3; /* ~-10.5dB */
    static const double isthrmax = M_SQRT2 - 1.0;
    double thrmid, thrside;
    double isthr;

    thrmid = 1.0;
    thrside = 0.0;
    isthr = 1.0;

    switch (mode)
    {
    case JOINT_MS:
        thrmid = thr075 / quality;
        if (thrmid > thrmax)
            thrmid = thrmax;

        thrside = sidemin / quality;
        if (thrside > sidemax)
            thrside = sidemax;

        thrmid += 1.0;
        break;
    case JOINT_IS:
        isthr = 0.18 / (quality * quality);
        if (isthr > isthrmax)
            isthr = isthrmax;

        isthr += 1.0;
        break;
    }

    // convert into energy
    thrmid *= thrmid;
    thrside *= thrside;
    isthr *= isthr;

    for (chn = 0; chn < maxchan; chn++)
    {
        int group;
        int bookcnt = 0;
        CoderInfo *cp = coder + chn;

        if (!channel[chn].present)
            continue;

        for (group = 0; group < cp->groups.n; group++)
        {
            int band;
            for (band = 0; band < cp->sfbn; band++)
            {
                cp->book[bookcnt] = HCB_NONE;
                cp->sf[bookcnt] = 0;
                bookcnt++;
            }
        }
    }
    for (chn = 0; chn < maxchan; chn++)
    {
        int rch;
        int cnt;
        int group;
        int sfcnt = 0;
        int start = 0;

        if (!channel[chn].present)
            continue;
        if (!((channel[chn].cpe) && (channel[chn].ch_is_left)))
            continue;

        rch = channel[chn].paired_ch;

        channel[chn].common_window = 0;
        channel[chn].msInfo.is_present = 0;
        channel[rch].msInfo.is_present = 0;

        if (coder[chn].block_type != coder[rch].block_type)
            continue;
        if (coder[chn].groups.n != coder[rch].groups.n)
            continue;

        channel[chn].common_window = 1;
        for (cnt = 0; cnt < coder[chn].groups.n; cnt++)
            if (coder[chn].groups.len[cnt] != coder[rch].groups.len[cnt])
            {
                channel[chn].common_window = 0;
                goto skip;
            }

        if (mode == JOINT_MS)
        {
            channel[chn].common_window = 1;
            channel[chn].msInfo.is_present = 1;
            channel[rch].msInfo.is_present = 1;
        }

        for (group = 0; group < coder->groups.n; group++)
        {
            int end = start + coder->groups.len[group];
            switch(mode) {
            case JOINT_MS:
                midside(coder + chn, channel + chn, s[chn], s[rch], &sfcnt,
                        start, end, thrmid, thrside);
                break;
            case JOINT_IS:
                stereo(coder + chn, coder + rch, s[chn], s[rch], &sfcnt, start, end, isthr);
                break;
            }
            start = end;
        }
        skip:;
    }
}
