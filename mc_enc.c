/*******************************************************************************************
 *
 * Multichannel allocation module.
 *
 * Authors:
 * CL    Chuck Lueck, TI <lueck@ti.com>
 * RG    Ralf Geiger, FhG/IIS
 *
 * Changes:
 * 22-jan-98   CL   Initial revision.
 * 07-apr-98   RG   Added determination of LFE channel.
 ***************************************************************************************/

#include "mc_enc.h"

void DetermineChInfo(Ch_Info* chInfo, int numChannels) {
   
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
  int cpeTag=0;
  int numChannelsLeft=numChannels;

 
  /* First element is sce, except for 2 channel case */
  if (numChannelsLeft!=2) {
    chInfo[numChannels-numChannelsLeft].present = 1;
    chInfo[numChannels-numChannelsLeft].tag=sceTag++;
    chInfo[numChannels-numChannelsLeft].cpe=0;
    numChannelsLeft--;
  }

  /* Next elements are cpe's */
  while (numChannelsLeft>1) {
    /* Left channel info */
    chInfo[numChannels-numChannelsLeft].present = 1;
    chInfo[numChannels-numChannelsLeft].tag=cpeTag++;
    chInfo[numChannels-numChannelsLeft].cpe=1;
    chInfo[numChannels-numChannelsLeft].common_window=0;
    chInfo[numChannels-numChannelsLeft].ch_is_left=1;
    chInfo[numChannels-numChannelsLeft].paired_ch=numChannels-numChannelsLeft+1;
    numChannelsLeft--;
  
    /* Right channel info */
    chInfo[numChannels-numChannelsLeft].present = 1;
    chInfo[numChannels-numChannelsLeft].cpe=1;
    chInfo[numChannels-numChannelsLeft].common_window=0;
    chInfo[numChannels-numChannelsLeft].ch_is_left=0;
    chInfo[numChannels-numChannelsLeft].paired_ch=numChannels-numChannelsLeft-1;
    numChannelsLeft--;
  }
  
  /* Is there another channel left ? */
  if (numChannelsLeft) {
      chInfo[numChannels-numChannelsLeft].present = 1;
      chInfo[numChannels-numChannelsLeft].tag=sceTag++;
      chInfo[numChannels-numChannelsLeft].cpe=0;
    numChannelsLeft--;
  }
}

