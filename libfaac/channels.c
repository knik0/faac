/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
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
 * $Id: channels.c,v 1.4 2001/06/08 18:01:09 menno Exp $
 */

#include "channels.h"
#include "coder.h"
#include "util.h"

/* If LFE present                                                       */
/*  Num channels       # of SCE's       # of CPE's       #of LFE's      */
/*  ============       ==========       ==========       =========      */
/*      1                  1                0               0           */
/*      2                  0                1               0           */
/*      3                  1                1               0           */
/*      4                  1                1               1           */
/*      5                  1                2               0           */
/* For more than 5 channels, use the following elements:                */
/*      2*N                1                2*(N-1)         1           */
/*      2*N+1              1                2*N             0           */
/*                                                                      */
/* Else:                                                                */
/*                                                                      */
/*  Num channels       # of SCE's       # of CPE's       #of LFE's      */
/*  ============       ==========       ==========       =========      */
/*      1                  1                0               0           */
/*      2                  0                1               0           */
/*      3                  1                1               0           */
/*      4                  2                1               0           */
/*      5                  1                2               0           */
/* For more than 5 channels, use the following elements:                */
/*      2*N                2                2*(N-1)         0           */
/*      2*N+1              1                2*N             0           */

void GetChannelInfo(ChannelInfo *channelInfo, int numChannels, int useLfe)
{
    int sceTag = 0;
    int lfeTag = 0;
    int cpeTag = 0;
    int numChannelsLeft = numChannels;


    /* First element is sce, except for 2 channel case */
    if (numChannelsLeft != 2) {
        channelInfo[numChannels-numChannelsLeft].present = 1;
        channelInfo[numChannels-numChannelsLeft].tag = sceTag++;
        channelInfo[numChannels-numChannelsLeft].cpe = 0;
        channelInfo[numChannels-numChannelsLeft].lfe = 0;
        numChannelsLeft--;
    }

    /* Next elements are cpe's */
    while (numChannelsLeft > 1) {
        /* Left channel info */
        channelInfo[numChannels-numChannelsLeft].present = 1;
        channelInfo[numChannels-numChannelsLeft].tag = cpeTag++;
        channelInfo[numChannels-numChannelsLeft].cpe = 1;
        channelInfo[numChannels-numChannelsLeft].common_window = 0;
        channelInfo[numChannels-numChannelsLeft].ch_is_left = 1;
        channelInfo[numChannels-numChannelsLeft].paired_ch = numChannels-numChannelsLeft+1;
        channelInfo[numChannels-numChannelsLeft].lfe = 0;
        numChannelsLeft--;

        /* Right channel info */
        channelInfo[numChannels-numChannelsLeft].present = 1;
        channelInfo[numChannels-numChannelsLeft].cpe = 1;
        channelInfo[numChannels-numChannelsLeft].common_window = 0;
        channelInfo[numChannels-numChannelsLeft].ch_is_left = 0;
        channelInfo[numChannels-numChannelsLeft].paired_ch = numChannels-numChannelsLeft-1;
        channelInfo[numChannels-numChannelsLeft].lfe = 0;
        numChannelsLeft--;
    }

    /* Is there another channel left ? */
    if (numChannelsLeft) {
        if (useLfe) {
            channelInfo[numChannels-numChannelsLeft].present = 1;
            channelInfo[numChannels-numChannelsLeft].tag = lfeTag++;
            channelInfo[numChannels-numChannelsLeft].cpe = 0;
            channelInfo[numChannels-numChannelsLeft].lfe = 1;
        } else {
            channelInfo[numChannels-numChannelsLeft].present = 1;
            channelInfo[numChannels-numChannelsLeft].tag = sceTag++;
            channelInfo[numChannels-numChannelsLeft].cpe = 0;
            channelInfo[numChannels-numChannelsLeft].lfe = 0;
        }
        numChannelsLeft--;
    }
}
