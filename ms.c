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
 * MS stereo coding module
 *
 * Authors:
 * CL    Chuck Lueck, TI <lueck@ti.com>
 *
 * Changes:
 * 22-jan-98   CL   Initial revision.  Lacks psychoacoustics for MS stereo.
 ***************************************************************************************/

#include "psych.h"
#include "ms.h"

void MSPreprocess(double p_ratio_long[][MAX_SCFAC_BANDS],
				  double p_ratio_short[][MAX_SCFAC_BANDS],
				  CH_PSYCH_OUTPUT_LONG p_chpo_long[],
				  CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS],
				  Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
				  enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
				  AACQuantInfo* quantInfo,               /* Quant info */
				  int use_ms,
				  int numberOfChannels
				  )
{
	int chanNum;
	int sfbNum;

	static int block = 0;
	int used = 0, notused = 0;
	int realyused = 0;

	/* Look for channel_pair_elements */
	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
		if (channelInfo[chanNum].present) {
			if ((channelInfo[chanNum].cpe)&&(channelInfo[chanNum].ch_is_left)) {
				int leftChan=chanNum;
				int rightChan=channelInfo[chanNum].paired_ch;
				channelInfo[leftChan].ms_info.is_present=0;
				channelInfo[leftChan].common_window = 0;

				/* Perform MS if block_types are the same */
				if ((block_type[leftChan]==block_type[rightChan])&&(use_ms==0)) { 

					int numGroups;
					int groupIndex = 0;
					int maxSfb;
					int g,b,j;
					int use_ms_short;
					MS_Info *msInfo;

					numGroups = quantInfo[leftChan].num_window_groups;
					maxSfb = quantInfo[leftChan].max_sfb;

					/* Determine which bands should be enabled */
					msInfo = &(channelInfo[leftChan].ms_info);

					for (g=0;g<numGroups;g++) {
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							use_ms_short = 1;
							b = g*maxSfb+sfbNum;

							if (block_type[leftChan] == ONLY_SHORT_WINDOW) {

								for (j = groupIndex; j < quantInfo[leftChan].window_group_length[g]+groupIndex; j++) {
									use_ms_short = min(use_ms_short, p_chpo_short[1][j].use_ms[sfbNum]);
								}
								msInfo->ms_used[b] = use_ms_short;
								if (msInfo->ms_used[b]) {
									realyused = 1;
									used++;
									for (j = groupIndex; j < quantInfo[leftChan].window_group_length[g]+groupIndex; j++) {
										p_ratio_short[0][(g*maxSfb)+sfbNum] = p_chpo_short[2][j].p_ratio[sfbNum];
										p_ratio_short[1][(g*maxSfb)+sfbNum] = p_chpo_short[3][j].p_ratio[sfbNum];
										p_chpo_short[1][j].use_ms[sfbNum] = use_ms_short;
									}
								} else {
									notused++;
									for (j = groupIndex; j < quantInfo[leftChan].window_group_length[g]+groupIndex; j++) {
										p_ratio_short[0][(g*maxSfb)+sfbNum] = p_chpo_short[0][j].p_ratio[sfbNum];
										p_ratio_short[1][(g*maxSfb)+sfbNum] = p_chpo_short[1][j].p_ratio[sfbNum];
										p_chpo_short[1][j].use_ms[sfbNum] = use_ms_short;
									}
								}

							} else {

								msInfo->ms_used[b] = p_chpo_long[1].use_ms[sfbNum];
								if (msInfo->ms_used[b]) {
									realyused = 1;
									used++;
									p_ratio_long[0][sfbNum] = p_chpo_long[2].p_ratio[sfbNum];
									p_ratio_long[1][sfbNum] = p_chpo_long[3].p_ratio[sfbNum];
								} else {
									notused++;
									p_ratio_long[0][sfbNum] = p_chpo_long[0].p_ratio[sfbNum];
									p_ratio_long[1][sfbNum] = p_chpo_long[1].p_ratio[sfbNum];
								}

							}
						}
						groupIndex+=quantInfo[leftChan].window_group_length[g];
					}

					if (realyused) {
						channelInfo[leftChan].common_window = 1;  /* Use common window */
						channelInfo[leftChan].ms_info.is_present=1;
					}
				} else if ((block_type[leftChan]==block_type[rightChan])&&(use_ms == 1)) {
					int chan;
					int numGroups;
					int groupIndex = 0;
					int maxSfb;
					int g,b,j;
					MS_Info *msInfo;

					channelInfo[0].ms_info.is_present = 1;
					channelInfo[0].common_window = 1;

					for (chan = 0; chan < 2; chan++) {
						maxSfb = quantInfo[chan].max_sfb;

						/* Determine which bands should be enabled */
						msInfo = &(channelInfo[leftChan].ms_info);
						numGroups = quantInfo[chan].num_window_groups;

						for (g=0;g<numGroups;g++) {
							for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
								b = g*maxSfb+sfbNum;
								groupIndex = 0;

								if (block_type[chan] == ONLY_SHORT_WINDOW) {

									msInfo->ms_used[b] = 1;
									for (j = groupIndex; j < quantInfo[chan].window_group_length[g]+groupIndex; j++) {
										p_ratio_short[chan][(g*maxSfb)+sfbNum] = p_chpo_short[chan][j].p_ratio[sfbNum];
										p_chpo_short[1][j].use_ms[sfbNum] = 1;
									}

								} else {

									msInfo->ms_used[b] = 1;
									p_ratio_long[chan][sfbNum] = p_chpo_long[chan].p_ratio[sfbNum];
									p_chpo_long[1].use_ms[sfbNum] = 1;
									
								}
							}
							groupIndex+=quantInfo[chan].window_group_length[g];
						}
					}
				} else {
					int chan;
					int numGroups;
					int groupIndex = 0;
					int maxSfb;
					int g,b,j;
					MS_Info *msInfo;

					for (chan = 0; chan < 2; chan++) {
						maxSfb = quantInfo[chan].max_sfb;

						/* Determine which bands should be enabled */
						msInfo = &(channelInfo[leftChan].ms_info);
						numGroups = quantInfo[chan].num_window_groups;

						for (g=0;g<numGroups;g++) {
							for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
								b = g*maxSfb+sfbNum;
								groupIndex = 0;

								if (block_type[chan] == ONLY_SHORT_WINDOW) {

									msInfo->ms_used[b] = 0;
									for (j = groupIndex; j < quantInfo[chan].window_group_length[g]+groupIndex; j++) {
										p_ratio_short[chan][(g*maxSfb)+sfbNum] = p_chpo_short[chan][j].p_ratio[sfbNum];
										p_chpo_short[1][j].use_ms[sfbNum] = 0;
									}

								} else {

									msInfo->ms_used[b] = 0;
									p_ratio_long[chan][sfbNum] = p_chpo_long[chan].p_ratio[sfbNum];
									p_chpo_long[1].use_ms[sfbNum] = 0;
									
								}
							}
							groupIndex+=quantInfo[chan].window_group_length[g];
						}
					}
				}
			}
		}
	}

//	printf("%d used: %d, notused: %d\n", block++, used, notused);
}

void MSEnergy(double *spectral_line_vector[MAX_TIME_CHANNELS],
			  double energy[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			  CH_PSYCH_OUTPUT_LONG p_chpo_long[],
			  CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS],
			  int sfb_width_table[MAX_TIME_CHANNELS][MAX_SCFAC_BANDS],
			  Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
			  enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
			  AACQuantInfo* quantInfo,               /* Quant info */
			  int use_ms,
			  int numberOfChannels
			  )
{
	int chanNum, numWindows, bandNumber;
	int windowLength, w, j;
	int *p_use_ms;

	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
		double dtmp;

		/* Compute energy in each scalefactor band of each window */
		numWindows = (block_type[chanNum]==ONLY_SHORT_WINDOW) ?	8 : 1;
		windowLength = 1024/8;
		bandNumber=0;
		for (w=0;w<numWindows;w++) {
			int offset=0;
			int sfb;

			if (block_type[chanNum] == ONLY_SHORT_WINDOW) {
				p_use_ms = p_chpo_short[1][w].use_ms;
			} else {
				p_use_ms = p_chpo_long[1].use_ms;
			}

			j = w*windowLength;

			/* Only compute energy up to max_sfb */
			for(sfb=0; sfb< quantInfo[chanNum].max_sfb; sfb++ ) {
				/* calculate scale factor band energy */
				int width,i;
				energy[chanNum][bandNumber] = 0.0;
				width=sfb_width_table[chanNum][sfb];
				for(i=offset; i<(offset+width); i++ ) {
					if (((p_use_ms[sfb]||(use_ms==1)) && (chanNum == 0))&&(use_ms!=-1))
						dtmp = (spectral_line_vector[0][j]+spectral_line_vector[1][j])*0.5;
					else if (((p_use_ms[sfb]||(use_ms==1)) && (chanNum == 1))&&(use_ms!=-1))
						dtmp = (spectral_line_vector[0][j]-spectral_line_vector[1][j])*0.5;
					else
						dtmp = spectral_line_vector[chanNum][j];
					j++;
					energy[chanNum][bandNumber] += dtmp*dtmp;
				}
				energy[chanNum][bandNumber] = energy[chanNum][bandNumber] / width;
				bandNumber++;
				offset+=width;
			}  
		}
	}  /* for (chanNum... */
}

/* Perform MS encoding.  Spectrum is non-interleaved.  */
/* This would be a lot simpler on interleaved spectral data */
void MSEncode(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
			  Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
			  int sfb_offset_table[][MAX_SCFAC_BANDS+1],
			  enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
			  AACQuantInfo* quantInfo,               /* Quant info */
			  int numberOfChannels) {                /* Number of channels */

	int chanNum;
	int sfbNum;
	int lineNum;
	double sum,diff;

	/* Look for channel_pair_elements */
	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
		if (channelInfo[chanNum].present) {
			if ((channelInfo[chanNum].cpe)&&(channelInfo[chanNum].ch_is_left)) {
				int leftChan=chanNum;
				int rightChan=channelInfo[chanNum].paired_ch;
				channelInfo[leftChan].ms_info.is_present=0;

				/* Perform MS if block_types are the same */
				if (block_type[leftChan]==block_type[rightChan]) { 

					int numGroups;
					int maxSfb;
					int g,b,w,line_offset;
					int startWindow,stopWindow;
					IS_Info *isInfo;
					MS_Info *msInfo;

					channelInfo[leftChan].common_window = 1;  /* Use common window */
					channelInfo[leftChan].ms_info.is_present=1;
                                        w = 0;

					numGroups = quantInfo[leftChan].num_window_groups;
					maxSfb = quantInfo[leftChan].max_sfb;

					/* Determine which bands should be enabled */
					/* Right now, simply enable bands which do not use intensity stereo */
					isInfo = &(channelInfo[rightChan].is_info);
					msInfo = &(channelInfo[leftChan].ms_info);
					for (g=0;g<numGroups;g++) {
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							b = g*maxSfb+sfbNum;
							msInfo->ms_used[b] = ( (!isInfo->is_used[b])||(!isInfo->is_present) );
						}
					}
					
					/* Perform sum and differencing on bands in which ms_used flag */
					/* has been set. */
					line_offset=0;
					startWindow = 0;
					for (g=0;g<numGroups;g++) {
						int numWindows = quantInfo[leftChan].window_group_length[g];
						stopWindow = startWindow + numWindows;
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							/* Enable MS mask */
							if (msInfo->ms_used[g*maxSfb+sfbNum]) {
								for (w=startWindow;w<stopWindow;w++) {
									for (lineNum=sfb_offset_table[leftChan][sfbNum];
									lineNum<sfb_offset_table[leftChan][sfbNum+1];
									lineNum++) {
										line_offset = w*BLOCK_LEN_SHORT;
										sum=spectrum[leftChan][line_offset+lineNum]+spectrum[rightChan][line_offset+lineNum];
										diff=spectrum[leftChan][line_offset+lineNum]-spectrum[rightChan][line_offset+lineNum];
										spectrum[leftChan][line_offset+lineNum] = 0.5 * sum;
										spectrum[rightChan][line_offset+lineNum] = 0.5 * diff;
									}  /* for (lineNum... */
								}  /* for (w=... */
							}
						}  /* for (sfbNum... */
						startWindow = stopWindow;
					} /* for (g... */
				}  /* if (block_type... */
			}
		}  /* if (channelInfo[chanNum].present */
	}  /* for (chanNum... */
}

/* Perform MS encoding.  Spectrum is non-interleaved.  */
/* This would be a lot simpler on interleaved spectral data */
void MSEncodeSwitch(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
	      Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
	      int sfb_offset_table[][MAX_SCFAC_BANDS+1],
	      enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
	      AACQuantInfo* quantInfo,               /* Quant info */
	      int numberOfChannels)                  /* Number of channels */
{
	int chanNum;
	int sfbNum;
	int lineNum;
	double sum,diff;

	/* Look for channel_pair_elements */
//	for (chanNum=0;chanNum<numberOfChannels;chanNum++) {
	chanNum = 0;
		if (channelInfo[0].ms_info.is_present) {
				int leftChan=chanNum;
				int rightChan=channelInfo[chanNum].paired_ch;


					int numGroups;
					int maxSfb;
					int g,b,w,line_offset;
					int startWindow,stopWindow;
					IS_Info *isInfo;
					MS_Info *msInfo;

					channelInfo[leftChan].common_window = 1;  /* Use common window */
					channelInfo[leftChan].ms_info.is_present=1;

					numGroups = quantInfo[leftChan].num_window_groups;
					maxSfb = quantInfo[leftChan].max_sfb;
					w=0;

					/* Determine which bands should be enabled */
					/* Right now, simply enable bands which do not use intensity stereo */
					isInfo = &(channelInfo[rightChan].is_info);
					msInfo = &(channelInfo[leftChan].ms_info);
#if 1
					for (g=0;g<numGroups;g++) {
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							b = g*maxSfb+sfbNum;
							msInfo->ms_used[b] = (((!isInfo->is_used[b])||(!isInfo->is_present))&&msInfo->ms_used[b]);
						}
					}
#endif
					/* Perform sum and differencing on bands in which ms_used flag */
					/* has been set. */
					line_offset=0;
					startWindow = 0;
					for (g=0;g<numGroups;g++) {
						int numWindows = quantInfo[leftChan].window_group_length[g];
						stopWindow = startWindow + numWindows;
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							/* Enable MS mask */
							if (msInfo->ms_used[g*maxSfb+sfbNum]) {
								for (w=startWindow;w<stopWindow;w++) {
									for (lineNum=sfb_offset_table[leftChan][sfbNum];
									lineNum<sfb_offset_table[leftChan][sfbNum+1];
									lineNum++) {
										line_offset = w*BLOCK_LEN_SHORT;
										sum=spectrum[leftChan][line_offset+lineNum]+spectrum[rightChan][line_offset+lineNum];
										diff=spectrum[leftChan][line_offset+lineNum]-spectrum[rightChan][line_offset+lineNum];
										spectrum[leftChan][line_offset+lineNum] = 0.5 * sum;
										spectrum[rightChan][line_offset+lineNum] = 0.5 * diff;
									}  /* for (lineNum... */
								}  /* for (w=... */
							}
						}  /* for (sfbNum... */
						startWindow = stopWindow;
					} /* for (g... */
				}  /* if (block_type... */
//	}  /* for (chanNum... */
}


void MSReconstruct(double *spectrum[MAX_TIME_CHANNELS],   /* array of pointers to spectral data */
		   Ch_Info *channelInfo,                  /* Pointer to Ch_Info */
		   int sfb_offset_table[][MAX_SCFAC_BANDS+1],
		   enum WINDOW_TYPE block_type[MAX_TIME_CHANNELS], /* Block type */
		   AACQuantInfo* quantInfo,               /* Quant info */
		   int numberOfChannels)                 /* Number of channels */
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
				int rightChan=channelInfo[chanNum].paired_ch;

				MS_Info *msInfo;
				msInfo = &(channelInfo[leftChan].ms_info);
				if (msInfo->is_present) {
					int numGroups = quantInfo[leftChan].num_window_groups;
					int maxSfb = quantInfo[leftChan].max_sfb;
					int g,w,line_offset;
					int startWindow,stopWindow;
					w=0;

					/* Perform sum and differencing on bands in which ms_used flag */
					/* has been set. */
					line_offset=0;
					startWindow = 0;
					for (g=0;g<numGroups;g++) {
						int numWindows = quantInfo[leftChan].window_group_length[g];
						stopWindow = startWindow + numWindows;
						for (sfbNum=0;sfbNum<maxSfb;sfbNum++) {
							/* Enable MS mask */
							if ((msInfo->ms_used[g*maxSfb+sfbNum]) || (msInfo->is_present==2)) {
								for (w=startWindow;w<stopWindow;w++) {
									line_offset = w*BLOCK_LEN_SHORT;
									for (lineNum=sfb_offset_table[leftChan][sfbNum];
									lineNum<sfb_offset_table[leftChan][sfbNum+1];
									lineNum++) {
										sum=spectrum[leftChan][line_offset+lineNum];
										diff=spectrum[rightChan][line_offset+lineNum];
										spectrum[leftChan][line_offset+lineNum] = (sum+diff);
										spectrum[rightChan][line_offset+lineNum] = (sum-diff);
									}  /* for (lineNum... */
								}  /* for (w=start... */
							}  /* if (msInfo... */
						}  /* for (sfbNum... */
						startWindow = stopWindow;
					} /* for (g... */
				}  /* if (ms_info.is_present... */
			}  /* if (channelInfo... */
		}  /* if (channelInfo[chanNum].present */
	}  /* for (chanNum... */
}

