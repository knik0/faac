/************************* MPEG-2 NBC Audio Decoder **************************
 *                                                                           *
"This software module was originally developed in the course of 
development of the MPEG-2 NBC/MPEG-4 Audio standard ISO/IEC 13818-7, 
14496-1,2 and 3. This software module is an implementation of a part of one or more 
MPEG-2 NBC/MPEG-4 Audio tools as specified by the MPEG-2 NBC/MPEG-4 
Audio standard. ISO/IEC  gives users of the MPEG-2 NBC/MPEG-4 Audio 
standards free license to this software module or modifications thereof for use in 
hardware or software products claiming conformance to the MPEG-2 NBC/MPEG-4
Audio  standards. Those intending to use this software module in hardware or 
software products are advised that this use may infringe existing patents. 
The original developer of this software module and his/her company, the subsequent 
editors and their companies, and ISO/IEC have no liability for use of this software 
module or modifications thereof in an implementation. Copyright is not released for 
non MPEG-2 NBC/MPEG-4 Audio conforming products.The original developer
retains full right to use the code for his/her  own purpose, assign or donate the 
code to a third party and to inhibit third party from using the code for non 
MPEG-2 NBC/MPEG-4 Audio conforming products. This copyright notice must
be included in all copies or derivative works." 
Copyright(c)1996.
 *                                                                           *
 ****************************************************************************/

/*******************************************************************************************
 *
 * IS stereo coding module
 *
 * Authors:
 * CL    Chuck Lueck, TI <lueck@ti.com>
 *
 * Changes:
 * 22-jan-98   CL   Initial revision.  Simple IS stereo support.
 ***************************************************************************************/


#include "is.h"

/* Perform IS encoding.  Spectrum is non-interleaved.  */
/* This would be a lot simpler on interleaved spectral data */
void ISEncode(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
	      Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
	      int sfb_offset_table[][MAX_SCFAC_BANDS+1],
	      enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
	      AACQuantInfo* quantInfo,
	      int numberOfChannels)
{
	int chanNum;
	int sfbNum;
	int lineNum,line_offset;
	int startWindow,stopWindow,w;

	/* Look for channel_pair_elements */
	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
		if (channelInfo[chanNum].present) {
			if ((channelInfo[chanNum].cpe)&&(channelInfo[chanNum].ch_is_left)) {
				int leftChan=chanNum;
				int rightChan=channelInfo[chanNum].paired_ch;
				channelInfo[leftChan].is_info.is_present=0;
				channelInfo[rightChan].is_info.is_present=0;

				/* Perform IS if block_types are the same */
				if (block_type[leftChan]==block_type[rightChan]) {
					int numGroups;
					int maxSfb;
					int g,b,minBand;
					IS_Info *isInfo;

					channelInfo[leftChan].common_window = 1;  /* Use common window */
					channelInfo[rightChan].is_info.is_present=1;

					numGroups = quantInfo[leftChan].num_window_groups;
					maxSfb = quantInfo[leftChan].max_sfb;

					/* Currently, enable Intensity Stereo above band IS_MIN_BAND */
					isInfo = &(channelInfo[rightChan].is_info);
					minBand = (block_type[rightChan]==ONLY_SHORT_WINDOW) ? IS_MIN_BAND_S : IS_MIN_BAND_L;
					minBand = (minBand>maxSfb) ? maxSfb : minBand;
					startWindow=0;
					for (g=0;g<numGroups;g++) {

						int numWindows;

						/* Disable lower frequency bands */
						for (sfbNum=0;sfbNum<minBand;sfbNum++) {
							b = g*maxSfb+sfbNum;
							isInfo->is_used[b] = 0;  /* Disable IS */
						}

						/* Enable IS bands */
						numWindows = quantInfo[leftChan].window_group_length[g];
						stopWindow = startWindow + numWindows;
						for (sfbNum=minBand;sfbNum<maxSfb;sfbNum++) {
							double ratio,log_ratio;
							int is_position;

							/* First, compute energies and intensity stereo position */
							double energy_l=0.0;   /* Left channel energy */
							double energy_r=0.0;   /* Right channel energy */
							double energy_s=0.0;   /* Sum channel energy */
							for (w=startWindow;w<stopWindow;w++) {
								line_offset = w*BLOCK_LEN_SHORT;
								for (lineNum=sfb_offset_table[rightChan][sfbNum];
								lineNum<sfb_offset_table[rightChan][sfbNum+1];
								lineNum++) {
									double l = spectrum[leftChan][line_offset+lineNum];
									double r = spectrum[rightChan][line_offset+lineNum];
									double s = l+r;
									energy_l += l*l;
									energy_r += r*r;
									energy_s += s*s;
								}
							}
							b = g*maxSfb+sfbNum;
							ratio = energy_l/energy_r;
							log_ratio = 2.0 * log(ratio) / log(2.0);
							is_position = (int) floor( log_ratio + 0.5);
							isInfo->fac[b] = is_position;
							quantInfo[rightChan].scale_factor[b] = is_position;
							isInfo->is_used[b] = 1;  /* Enable IS */
							isInfo->sign[b] = 0;     /* In phase only */

							/* Now replace right and left spectral coefficients */
							for (w=startWindow;w<stopWindow;w++) {
								line_offset = w*BLOCK_LEN_SHORT;
								for (lineNum=sfb_offset_table[rightChan][sfbNum];
								lineNum<sfb_offset_table[rightChan][sfbNum+1];
								lineNum++) {
									double l = spectrum[leftChan][line_offset+lineNum];
									double r = spectrum[rightChan][line_offset+lineNum];
									double new_spec = (l+r) * sqrt(energy_l/(energy_s+1));
									spectrum[leftChan][line_offset+lineNum] = new_spec;  /* Replace left */
									spectrum[rightChan][line_offset+lineNum] = 0.0;
								}
							}
						}
						startWindow = stopWindow;
					}
				}  /* if (block_type... */
			}  /* if ((channelInfo... */
		}  /* if (channelInfo... */
	}  /* for (chanNum... */
} 


void ISReconstruct(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
				   Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
				   int sfb_offset_table[][MAX_SCFAC_BANDS+1],
//				   enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
				   AACQuantInfo* quantInfo,
				   int numberOfChannels)                 /* Number of channels */
{
	int chanNum;
	int sfbNum;
	int lineNum,line_offset;
	int startWindow,stopWindow,w;

	/* Look for channel_pair_elements */
	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
		if (channelInfo[chanNum].present) {
			if ((channelInfo[chanNum].cpe)&&(channelInfo[chanNum].ch_is_left)) {
				int leftChan=chanNum;
				int rightChan=channelInfo[chanNum].paired_ch;

				/* If intensity stereo is present in right channel, reconstruct spectrum */
				IS_Info *isInfo;
				isInfo = &(channelInfo[rightChan].is_info);
				if (isInfo->is_present) {
					int numGroups = quantInfo[rightChan].num_window_groups;
					int maxSfb = quantInfo[rightChan].max_sfb;
					int g,b;

					/* Currently, enable Intensity Stereo above band IS_MIN_BAND */
					startWindow=0;
					for (g=0;g<numGroups;g++) {

						/* Enable IS bands */
						int numWindows = quantInfo[leftChan].window_group_length[g];
						stopWindow = startWindow + numWindows;
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							b = g*maxSfb+sfbNum;
							if (isInfo->is_used[b]) {
								double scale = pow( 0.5, 0.25 * ((double)isInfo->fac[b])); 
								scale = scale * (-2.0 * isInfo->sign[b] + 1.0);
								for (w=startWindow;w<stopWindow;w++) {
									line_offset = w*BLOCK_LEN_SHORT;
									for (lineNum=sfb_offset_table[rightChan][sfbNum];
									lineNum<sfb_offset_table[rightChan][sfbNum+1];
									lineNum++) {
										spectrum[rightChan][line_offset+lineNum] = 
											spectrum[leftChan][line_offset+lineNum] * scale;
									}
								}
							}  /* if (isInfo... */
						}  /* for (sfbNum... */
						startWindow = stopWindow;
					}
				}  /* if (block_type... */
			}  /* if ((channelInfo... */
		}  /* if (channelInfo... */
	}  /* for (chanNum... */
} 

