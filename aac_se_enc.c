/*********************************************************************
 *
 * Module for writing/counting AAC syntactic elements
 *
 * Authors:
 * CL    Chuck Lueck, TI <lueck@ti.com>
 * ADD   Alberto Duenas, TI <alberto@ndsuk.com>
 * RG    Ralf Geiger,  FhG/IIS
 * Changes:
 * 07-jun-97   CL   Initial revision.
 * 14-sep-97   CL   Modified WritePredictorData to actually write
 *                  predictor data.  Still need to add resets.
 * 20-oct-97   CL   Updated WriteTNSData to support TNS.
 * 03-Dec-97   ADD  Addding the prediction reset
 * 22-Jan-98   CL   Added support for CPE's and common windows.
 * 07-Apr-98   RG   Added WriteLFE to write LFE channel element.
 *
**********************************************************************/

#include <stdlib.h>
#include "aac_se_enc.h"
#include "aac_qc.h"

int max_pred_sfb;

/*****************************************************************************/
/* WriteAACFillBits(...)                                                     */
/* Write fill_elements to the bitstream.	                             */
/* Number of bits written is LEN_SE_ID + FIL_COUNT_LEN + a multiple of 8.    */
/* Return number of bits left over (less than 7 ).                           */
/*****************************************************************************/
int WriteAACFillBits(BsBitStream* ptrBs,  /* Pointer to bitstream */
		     int numBits,         /* Number of bits needed to fill */
			 int writeFlag)
{
	int numberOfBitsLeft=numBits;

	/* Need at least (LEN_SE_ID + LEN_F_CNT) bits for a fill_element */
	int minNumberOfBits = LEN_SE_ID + LEN_F_CNT;
	while (numberOfBitsLeft>=minNumberOfBits) {
		int numberOfBytes;
		int maxCount;

		if (writeFlag) {
			BsPutBit(ptrBs,ID_FIL,LEN_SE_ID);	/* Write fill_element ID */
		}
		numberOfBitsLeft-=minNumberOfBits;	/* Subtract for ID,count */

		numberOfBytes=(int)(numberOfBitsLeft/LEN_BYTE);
		maxCount = (1<<LEN_F_CNT) - 1;  /* Max count without escaping */

		/* if we have less than maxCount bytes, write them now */
		if (numberOfBytes<maxCount) {
			int i;
			if (writeFlag) {
				BsPutBit(ptrBs,numberOfBytes,LEN_F_CNT);
				for (i=0;i<numberOfBytes;i++) {
					BsPutBit(ptrBs,0,LEN_BYTE);
				}
			}
			/* otherwise, we need to write an escape count */
		} else {
			int maxEscapeCount,maxNumberOfBytes,escCount;
			int i;
			if (writeFlag) {
				BsPutBit(ptrBs,maxCount,LEN_F_CNT);
			}
			maxEscapeCount = (1<<LEN_BYTE) - 1;  /* Max escape count */
			maxNumberOfBytes = maxCount + maxEscapeCount;
			numberOfBytes = (numberOfBytes > maxNumberOfBytes ) ?
				(maxNumberOfBytes) : (numberOfBytes);
			escCount = numberOfBytes - maxCount;
			if (writeFlag) {
				BsPutBit(ptrBs,escCount,LEN_BYTE);
				for (i=0;i<numberOfBytes-1;i++) {
					BsPutBit(ptrBs,0,LEN_BYTE);
				}
			}
		}
		numberOfBitsLeft -= LEN_BYTE*numberOfBytes;
	}
	return numberOfBitsLeft;
}

int WriteADTSHeader(AACQuantInfo* quantInfo,   /* AACQuantInfo structure */
					BsBitStream* fixedStream,  /* Pointer to bitstream */
					int used_bits,
					int writeFlag)             /* 1 means write, 0 means count only */
{
	if (writeFlag) {
		/* Fixed ADTS header */
		BsPutBit(fixedStream, 0xFFFF, 12); // 12 bit Syncword
		BsPutBit(fixedStream, 1, 1); // ID
		BsPutBit(fixedStream, 0, 2); // layer
		BsPutBit(fixedStream, 1, 1); // protection absent
		BsPutBit(fixedStream, quantInfo->profile, 2); // profile
		BsPutBit(fixedStream, quantInfo->srate_idx, 4); // sampling rate
		BsPutBit(fixedStream, 0, 1); // private bit
		BsPutBit(fixedStream, 1, 3); // ch. config (must be > 0)
		BsPutBit(fixedStream, 0, 1); // original/copy
		BsPutBit(fixedStream, 0, 1); // home
		BsPutBit(fixedStream, 0, 2); // emphasis

		/* Variable ADTS header */
		BsPutBit(fixedStream, 0, 1); // copyr. id. bit
		BsPutBit(fixedStream, 0, 1); // copyr. id. start
		BsPutBit(fixedStream, bit2byte(used_bits), 13); // number of bits
		BsPutBit(fixedStream, 0x7FF, 11); // buffer fullness (0x7FF for VBR)
		BsPutBit(fixedStream, 0, 2); // raw data blocks (0+1=1)
	}

	return 58;
}


/*****************************************************************************/
/* WriteSCE(...), write a single-channel element to the bitstream.           */
/*****************************************************************************/
int WriteSCE(AACQuantInfo* quantInfo,   /* AACQuantInfo structure */
			 int tag,
			 BsBitStream* fixedStream,  /* Pointer to bitstream */
			 int writeFlag)             /* 1 means write, 0 means count only */
{
	int bit_count=0;

	if (writeFlag) {
		/* write ID_SCE, single_element_channel() identifier */
		BsPutBit(fixedStream,ID_SCE,LEN_SE_ID);  

		/* write the element_identifier_tag */
		BsPutBit(fixedStream,tag,LEN_TAG);  
	}
	
	bit_count += LEN_SE_ID;
	bit_count += LEN_TAG;
	
	/* Write an individual_channel_stream element */
	bit_count += WriteICS(quantInfo,0,fixedStream,writeFlag);
	
	return bit_count;
}

/*****************************************************************************/
/* WriteLFE(...), write a lfe-channel element to the bitstream.              */
/*****************************************************************************/
int WriteLFE(AACQuantInfo* quantInfo,   /* AACQuantInfo structure */
			 int tag,
			 BsBitStream* fixedStream,  /* Pointer to bitstream */
			 int writeFlag)             /* 1 means write, 0 means count only */
{
	int bit_count=0;
	
	if (writeFlag) {
		/* write ID_LFE, lfe_element_channel() identifier */
		BsPutBit(fixedStream,ID_LFE,LEN_SE_ID);  

		/* write the element_identifier_tag */
		BsPutBit(fixedStream,tag,LEN_TAG);  
	}
	
	bit_count += LEN_SE_ID;
	bit_count += LEN_TAG;
	
	/* Write an individual_channel_stream element */
	bit_count += WriteICS(quantInfo,0,fixedStream,writeFlag);
	
	return bit_count;
}


/*****************************************************************************/
/* WriteCPE(...), write a channel_pair_element to the bitstream.           */
/*****************************************************************************/
int WriteCPE(AACQuantInfo* quantInfoL,   /* AACQuantInfo structure, left */
             AACQuantInfo* quantInfoR,   /* AACQuantInfo structure, right */
			 int tag,
			 int commonWindow,          /* common_window flag */
			 MS_Info* ms_info,          /* MS stereo information */
			 BsBitStream* fixedStream,  /* Pointer to bitstream */
			 int writeFlag)             /* 1 means write, 0 means count only */
{
	int bit_count=0;

	if (writeFlag) {
		/* write ID_CPE, single_element_channel() identifier */
		BsPutBit(fixedStream,ID_CPE,LEN_SE_ID);  

		/* write the element_identifier_tag */
		BsPutBit(fixedStream,tag,LEN_TAG);  /* Currently, this is zero */

		/* common_window? */
		BsPutBit(fixedStream,commonWindow,LEN_COM_WIN);
	}
	
	bit_count += LEN_SE_ID;
	bit_count += LEN_TAG;
	bit_count += LEN_COM_WIN;

	/* if common_window, write ics_info */
	if (commonWindow) {
		int numWindows,maxSfb;
		bit_count = WriteICSInfo(quantInfoL,fixedStream,writeFlag);
		numWindows=quantInfoL->num_window_groups;
		maxSfb = quantInfoL->max_sfb;
		if (writeFlag) {
			BsPutBit(fixedStream,ms_info->is_present,LEN_MASK_PRES);
			if (ms_info->is_present==1) {
				int g;
				int b;
				for (g=0;g<numWindows;g++) {
					for (b=0;b<maxSfb;b++) {
						BsPutBit(fixedStream,ms_info->ms_used[g*maxSfb+b],LEN_MASK);
					}
				}
			}
		}
		bit_count += LEN_MASK_PRES;
		bit_count += (ms_info->is_present==1)*numWindows*maxSfb*LEN_MASK;
	}
	
	/* Write individual_channel_stream elements */
	bit_count += WriteICS(quantInfoL,commonWindow,fixedStream,writeFlag);
	bit_count += WriteICS(quantInfoR,commonWindow,fixedStream,writeFlag);
	
	return bit_count;
}


/*****************************************************************************/
/* WriteICS(...), write an individual_channel_stream element to the bitstream.*/
/*****************************************************************************/
int WriteICS(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
			 int commonWindow,           /* Common window flag */
			 BsBitStream* fixed_stream,  /* Pointer to bitstream */
			 int writeFlag)              /* 1 means write, 0 means count only */
{
	/* this function writes out an individual_channel_stream to the bitstream and */
	/* returns the number of bits written to the bitstream */
	int bit_count = 0;
	int output_book_vector[SFB_NUM_MAX*2];
	writeFlag = ( writeFlag != 0 );

	/* Write the 8-bit global_gain */
	BsPutBit(fixed_stream,quantInfo->common_scalefac,writeFlag*LEN_GLOB_GAIN);  
	bit_count += LEN_GLOB_GAIN;

	/* Write ics information */
	if (!commonWindow) {
		bit_count += WriteICSInfo(quantInfo,fixed_stream,writeFlag);
	}

	/* Write section_data() information to the bitstream */
	bit_count += sort_book_numbers(quantInfo,output_book_vector,fixed_stream,writeFlag);

	/* Write scale_factor_data() information */
	bit_count += write_scalefactor_bitstream(fixed_stream,writeFlag,quantInfo);

	/* Write pulse_data() */
	bit_count += WritePulseData(quantInfo,fixed_stream,writeFlag);

	/* Write TNS data */
	bit_count += WriteTNSData(quantInfo,fixed_stream,writeFlag);
	
	/* Write gain control data */
	bit_count += WriteGainControlData(quantInfo,fixed_stream,writeFlag);

	/* Write out spectral_data() */
	bit_count += WriteSpectralData(quantInfo,fixed_stream,writeFlag);

	/* Return number of bits */
	return(bit_count);
}


/*****************************************************************************/
/* WriteICSInfo(...), write individual_channel_stream information            */
/*  to the bitstream.                                                        */
/*****************************************************************************/
int WriteICSInfo(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
		 BsBitStream* fixed_stream,  /* Pointer to bitstream */
		 int writeFlag)              /* 1 means write, 0 means count only */
{
  int grouping_bits;
  int max_sfb;
  int bit_count = 0;

  /* Compute number of scalefactor bands */
//  if (quantInfo->max_sfb*quantInfo->num_window_groups != quantInfo->nr_of_sfb)
    //CommonExit(-1,"Wrong number of scalefactorbands");
  max_sfb =   quantInfo->max_sfb;

  if (writeFlag) {
    /* write out ics_info() information */
    BsPutBit(fixed_stream,0,LEN_ICS_RESERV);  /* reserved Bit*/
  
    /* Write out window sequence */
    BsPutBit(fixed_stream,quantInfo->block_type,LEN_WIN_SEQ);  /* short window */

    /* Write out window shape */ 
    BsPutBit(fixed_stream,quantInfo->window_shape,LEN_WIN_SH);  /* window shape */
  }
    
  bit_count += LEN_ICS_RESERV;
  bit_count += LEN_WIN_SEQ;
  bit_count += LEN_WIN_SH;

  /* For short windows, write out max_sfb and scale_factor_grouping */
  if (quantInfo -> block_type == ONLY_SHORT_WINDOW){
    if (writeFlag) {
      BsPutBit(fixed_stream,max_sfb,LEN_MAX_SFBS); 
      grouping_bits = find_grouping_bits(quantInfo->window_group_length,quantInfo->num_window_groups);
      BsPutBit(fixed_stream,grouping_bits,MAX_SHORT_IN_LONG_BLOCK - 1);  /* the grouping bits */
    }
    bit_count += LEN_MAX_SFBS;
    bit_count += MAX_SHORT_IN_LONG_BLOCK - 1;
  }

  /* Otherwise, write out max_sfb and predictor data */
  else { /* block type is either start, stop, or long */
    if (writeFlag) {
      BsPutBit(fixed_stream,max_sfb,LEN_MAX_SFBL);
    }
    bit_count += LEN_MAX_SFBL;
    bit_count += WriteLTP_PredictorData(quantInfo,fixed_stream,writeFlag);
  }

  return bit_count;
}

/*****************************************************************************/
/* WriteLTP_PredictorData(...), write LTP predictor data.                    */
/*****************************************************************************/
int WriteLTP_PredictorData(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
                           BsBitStream* fixed_stream,  /* Pointer to bitstream */
                           int writeFlag)              /* 1 means write, 0 means count only */  
{
	int bit_count = 0;

	bit_count += nok_ltp_encode (fixed_stream, quantInfo->block_type, quantInfo->nr_of_sfb, 
		quantInfo->ltpInfo, writeFlag);

	return (bit_count);
}

/*****************************************************************************/
/* WritePulseData(...), write pulse data.                            */
/*****************************************************************************/
int WritePulseData(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
		   BsBitStream* fixed_stream,  /* Pointer to bitstream */
		   int writeFlag)              /* 1 means write, 0 means count only */
{
	int i, bit_count = 0;

	if (quantInfo->pulseInfo.pulse_data_present) {
		if (writeFlag) {
			BsPutBit(fixed_stream,1,LEN_PULSE_PRES);  /* no pulse_data_present */
			BsPutBit(fixed_stream,quantInfo->pulseInfo.number_pulse,LEN_NEC_NPULSE);  /* no pulse_data_present */
			BsPutBit(fixed_stream,quantInfo->pulseInfo.pulse_start_sfb,LEN_NEC_ST_SFB);  /* no pulse_data_present */
		}
		bit_count += LEN_NEC_NPULSE + LEN_NEC_ST_SFB;

		for (i = 0; i < quantInfo->pulseInfo.number_pulse+1; i++) {
			if (writeFlag) {
				BsPutBit(fixed_stream,quantInfo->pulseInfo.pulse_offset[i],LEN_NEC_POFF);
				BsPutBit(fixed_stream,quantInfo->pulseInfo.pulse_amp[i],LEN_NEC_PAMP);
			}
			bit_count += LEN_NEC_POFF + LEN_NEC_PAMP;
		}
	} else {
		if (writeFlag) {
			BsPutBit(fixed_stream,0,LEN_PULSE_PRES);  /* no pulse_data_present */
		}
	}
	bit_count += LEN_PULSE_PRES;
	return bit_count;
}

/*****************************************************************************/
/* WriteTNSData(...), write TNS data.                            */
/*****************************************************************************/
int WriteTNSData(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
		 BsBitStream* fixed_stream,  /* Pointer to bitstream */
		 int writeFlag)              /* 1 means write, 0 means count only */ 
{
  int bit_count = 0;
  int numWindows = 1;
  int len_tns_nfilt;
  int len_tns_length;
  int len_tns_order;
  int filtNumber;
  int resInBits;
  int bitsToTransmit;
  unsigned long unsignedIndex;
  int w;

  TNS_INFO* tnsInfoPtr = quantInfo->tnsInfo;

  if (writeFlag) {
    BsPutBit(fixed_stream,tnsInfoPtr->tnsDataPresent,LEN_TNS_PRES);
  }
  bit_count += LEN_TNS_PRES;

  /* If TNS is not present, bail */
  if (!tnsInfoPtr->tnsDataPresent) {
    return bit_count;
  }

  /* Set window-dependent TNS parameters */
  if (quantInfo->block_type == ONLY_SHORT_WINDOW) {
    numWindows = MAX_SHORT_IN_LONG_BLOCK;
    len_tns_nfilt = LEN_TNS_NFILTS;
    len_tns_length = LEN_TNS_LENGTHS;
    len_tns_order = LEN_TNS_ORDERS;
  } else {
    numWindows = 1;
    len_tns_nfilt = LEN_TNS_NFILTL;
    len_tns_length = LEN_TNS_LENGTHL;
    len_tns_order = LEN_TNS_ORDERL;
  }

  /* Write TNS data */
  bit_count += numWindows * len_tns_nfilt;
  for (w=0;w<numWindows;w++) {
    TNS_WINDOW_DATA* windowDataPtr = &tnsInfoPtr->windowData[w];
    int numFilters = windowDataPtr->numFilters;
    if (writeFlag) {
      BsPutBit(fixed_stream,numFilters,len_tns_nfilt); /* n_filt[] = 0 */
    }
    if (numFilters) {
      bit_count += LEN_TNS_COEFF_RES;
      resInBits = windowDataPtr->coefResolution;
      resInBits = windowDataPtr->coefResolution;
      if (writeFlag) {
	BsPutBit(fixed_stream,resInBits-DEF_TNS_RES_OFFSET,LEN_TNS_COEFF_RES);
      }
      bit_count += numFilters * (len_tns_length+len_tns_order);
      for (filtNumber=0;filtNumber<numFilters;filtNumber++) {
	TNS_FILTER_DATA* tnsFilterPtr=&windowDataPtr->tnsFilter[filtNumber];
	int order = tnsFilterPtr->order;
	if (writeFlag) {
	  BsPutBit(fixed_stream,tnsFilterPtr->length,len_tns_length);
	  BsPutBit(fixed_stream,order,len_tns_order);
	}
	if (order) {
	  bit_count += (LEN_TNS_DIRECTION + LEN_TNS_COMPRESS);
	  if (writeFlag) {
	    BsPutBit(fixed_stream,tnsFilterPtr->direction,LEN_TNS_DIRECTION);
	    BsPutBit(fixed_stream,tnsFilterPtr->coefCompress,LEN_TNS_COMPRESS);
	  }
          bitsToTransmit = resInBits - tnsFilterPtr->coefCompress;
	  bit_count += order * bitsToTransmit;
	  if (writeFlag) {
	    int i;
	    for (i=1;i<=order;i++) {
	      unsignedIndex = (unsigned long) (tnsFilterPtr->index[i])&(~(~0<<bitsToTransmit));
	      BsPutBit(fixed_stream,unsignedIndex,bitsToTransmit);
	    }
	  }
	}
      }
    }
  }
  return bit_count;
}


/*****************************************************************************/
/* WriteGainControlData(...), write gain control data.                       */
/*****************************************************************************/
int WriteGainControlData(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
			 BsBitStream* fixed_stream,  /* Pointer to bitstream */
			 int writeFlag)              /* 1 means write, 0 means count only */  
{
	int bit_count = 0;
	bit_count += LEN_GAIN_PRES;

	if (writeFlag) {
		BsPutBit(fixed_stream,0,LEN_GAIN_PRES);
	}
	return bit_count;
}


/*****************************************************************************/
/* WriteSpectralData(...), write spectral data.                              */
/*****************************************************************************/
int WriteSpectralData(AACQuantInfo* quantInfo,    /* AACQuantInfo structure */
		      BsBitStream* fixed_stream,  /* Pointer to bitstream */
		      int writeFlag)              /* 1 means write, 0 means count only */
{
	int bit_count = 0;
	int numSpectral = quantInfo->spectralCount;

	/* set up local pointers to data and len */
	/* data array contains data to be written */
	/* len array contains lengths of data words */
	int* data = quantInfo -> data;
	int* len = quantInfo -> len;

	if (writeFlag) {
		int i;
		for(i=0;i<numSpectral;i++) {
			if (len[i] > 0) {  /* only send out non-zero codebook data */
				BsPutBit(fixed_stream,data[i],len[i]); /* write data */
				bit_count += len[i];                   /* update bit_count */
			}
		}
	} else {
		int i;
		for(i=0;i<numSpectral;i++) {
			bit_count += len[i];              /* update bit_count */
		}
	}

	return bit_count;
}

