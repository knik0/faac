/*
 *	Multichannel configuration
 *
 *	Copyright (c) 1999 M. Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**************************************************************************
  Version Control Information			Method: CVS
  Identifiers:
  $Revision: 1.5 $
  $Date: 2000/11/01 14:05:32 $ (check in)
  $Author: menno $
  *************************************************************************/

#include "mc_enc.h"
#include "quant.h"

void DetermineChInfo(AACQuantInfo *quantInfo, int numChannels, int lfePresent) {
   
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

	int sceTag=0;
	int lfeTag=0;
	int cpeTag=0;
	int numChannelsLeft=numChannels;

	
	/* First element is sce, except for 2 channel case */
	if (numChannelsLeft!=2) {
		quantInfo[numChannels-numChannelsLeft].channelInfo.present = 1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.tag=sceTag++;
		quantInfo[numChannels-numChannelsLeft].channelInfo.cpe=0;
		quantInfo[numChannels-numChannelsLeft].channelInfo.lfe=0;    
		numChannelsLeft--;
	}

	/* Next elements are cpe's */
	while (numChannelsLeft>1) {
		/* Left channel info */
		quantInfo[numChannels-numChannelsLeft].channelInfo.present = 1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.tag=cpeTag++;
		quantInfo[numChannels-numChannelsLeft].channelInfo.cpe=1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.common_window=0;
		quantInfo[numChannels-numChannelsLeft].channelInfo.ch_is_left=1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.paired_ch=numChannels-numChannelsLeft+1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.lfe=0;    
		numChannelsLeft--;
		
		/* Right channel info */
		quantInfo[numChannels-numChannelsLeft].channelInfo.present = 1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.cpe=1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.common_window=0;
		quantInfo[numChannels-numChannelsLeft].channelInfo.ch_is_left=0;
		quantInfo[numChannels-numChannelsLeft].channelInfo.paired_ch=numChannels-numChannelsLeft-1;
		quantInfo[numChannels-numChannelsLeft].channelInfo.lfe=0;
		numChannelsLeft--;
	}

	/* Is there another channel left ? */
	if (numChannelsLeft) {
		if (lfePresent) { 
			quantInfo[numChannels-numChannelsLeft].channelInfo.present = 1;
			quantInfo[numChannels-numChannelsLeft].channelInfo.tag=lfeTag++;
			quantInfo[numChannels-numChannelsLeft].channelInfo.cpe=0;
			quantInfo[numChannels-numChannelsLeft].channelInfo.lfe=1; 
		} else {
			quantInfo[numChannels-numChannelsLeft].channelInfo.present = 1;
			quantInfo[numChannels-numChannelsLeft].channelInfo.tag=sceTag++;
			quantInfo[numChannels-numChannelsLeft].channelInfo.cpe=0;
			quantInfo[numChannels-numChannelsLeft].channelInfo.lfe=0;
		}
		numChannelsLeft--;
	}
}


