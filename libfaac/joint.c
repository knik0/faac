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
 * $Id: joint.c,v 1.1 2001/01/17 11:21:40 menno Exp $
 */

#include <stdlib.h> /* for max() define */
#include "channels.h"

void MSEncode(CoderInfo *coderInfo,
			  ChannelInfo *channelInfo,
			  double *spectrum[MAX_CHANNELS],
			  int numberOfChannels,
			  short msenable)
{
	int chanNum;
	int sfbNum;
	int lineNum;
	double sum,diff;

	/* Look for channel_pair_elements */
	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
		if (channelInfo[chanNum].present) {
			if ((channelInfo[chanNum].cpe)&&(channelInfo[chanNum].ch_is_left)) {
				int leftChan=chanNum;
				int rightChan = channelInfo[chanNum].paired_ch;
				channelInfo[leftChan].msInfo.is_present = 0;
				channelInfo[rightChan].msInfo.is_present = 0;

				/* Perform MS if block_types are the same */
				if ((coderInfo[leftChan].block_type==coderInfo[rightChan].block_type)&&(msenable)) { 
					int numGroups;
					int maxSfb;
					int g,w,line_offset;
					int startWindow,stopWindow;
					MSInfo *msInfoL;
					MSInfo *msInfoR;

					channelInfo[leftChan].common_window = 1;  /* Use common window */
					channelInfo[leftChan].msInfo.is_present = 1;
					channelInfo[rightChan].msInfo.is_present = 1;

					numGroups = coderInfo[leftChan].num_window_groups;
					maxSfb = coderInfo[leftChan].max_sfb;
					w=0;

					/* Determine which bands should be enabled */
					msInfoL = &(channelInfo[leftChan].msInfo);
					msInfoR = &(channelInfo[rightChan].msInfo);
					
					/* Perform sum and differencing on bands in which ms_used flag */
					/* has been set. */
					if (coderInfo[leftChan].block_type == ONLY_SHORT_WINDOW) {
						line_offset=0;
						startWindow = 0;
						for (g=0;g<numGroups;g++) {
							int numWindows = coderInfo[leftChan].window_group_length[g];
							stopWindow = startWindow + numWindows;
							for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
								/* Enable MS mask */
								int use_ms = 1;
								for (w=startWindow;w<stopWindow;w++) {
									use_ms = min(use_ms, msInfoL->ms_usedS[w][sfbNum]);
								}
								msInfoL->ms_used[g*maxSfb+sfbNum] = use_ms; /* write this to bitstream */
								msInfoR->ms_used[g*maxSfb+sfbNum] = use_ms;
								for (w=startWindow;w<stopWindow;w++) {
									msInfoL->ms_usedS[w][sfbNum] = use_ms;
									msInfoR->ms_usedS[w][sfbNum] = use_ms;
								}
								for (w=startWindow;w<stopWindow;w++) {
									if (msInfoL->ms_usedS[w][sfbNum]) {
										for (lineNum = coderInfo[leftChan].sfb_offset[sfbNum];
										lineNum < coderInfo[leftChan].sfb_offset[sfbNum+1];
										lineNum++)
										{
											line_offset = w*BLOCK_LEN_SHORT;
											sum=spectrum[leftChan][line_offset+lineNum]+spectrum[rightChan][line_offset+lineNum];
											diff=spectrum[leftChan][line_offset+lineNum]-spectrum[rightChan][line_offset+lineNum];
											spectrum[leftChan][line_offset+lineNum] = 0.5 * sum;
											spectrum[rightChan][line_offset+lineNum] = 0.5 * diff;
										}
									}
								}
							}
							startWindow = stopWindow;
						}
					} else {
						for (sfbNum = 0;sfbNum < maxSfb; sfbNum++) {
							msInfoR->ms_used[sfbNum] = msInfoL->ms_used[sfbNum];

							/* Enable MS mask */
							if (msInfoL->ms_used[sfbNum]) {
								for (lineNum = coderInfo[leftChan].sfb_offset[sfbNum];
								lineNum < coderInfo[leftChan].sfb_offset[sfbNum+1];
								lineNum++)
								{
									sum=spectrum[leftChan][lineNum]+spectrum[rightChan][lineNum];
									diff=spectrum[leftChan][lineNum]-spectrum[rightChan][lineNum];
									spectrum[leftChan][lineNum] = 0.5 * sum;
									spectrum[rightChan][lineNum] = 0.5 * diff;
								}
							}
						}
					}
				}
			}
		}
	}
}

