/*
 *	Function prototypes for writing/counting AAC syntactic elements
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
  $Revision: 1.7 $
  $Date: 2000/10/05 13:04:05 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef AAC_SE_ENC
#define AAC_SE_ENC

#include "bitstream.h"
#include "quant.h"
#include "nok_ltp_enc.h"

extern int max_pred_sfb;



/*****************************************************************************/
/* Write AAC fill bits to the bitStream                                      */
/*****************************************************************************/
int WriteAACFillBits(BsBitStream* ptrBs,  /* Pointer to bit stream */
		     int numBits,        /* Number of bits neede to fill */
				int writeFlag);

int WriteADTSHeader(AACQuantInfo* quantInfo,   /* AACQuantInfo structure */
					BsBitStream* fixedStream,  /* Pointer to bitstream */
					int used_bits,
					int writeFlag);            /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteSCE(...), write a single-channel element to the bitstream.           */
/*****************************************************************************/
int WriteSCE(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
	     int tag,
	     BsBitStream* fixedStream, /* Pointer to bitstream */
	     int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteLFE(...), write a lfe-channel element to the bitstream.              */
/*****************************************************************************/
int WriteLFE(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
	     int tag,
	     BsBitStream* fixedStream, /* Pointer to bitstream */
	     int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteCPE(...), write a channel_pair_element to the bitstream.           */
/*****************************************************************************/
int WriteCPE(AACQuantInfo* quantInfoL,   /* AACQuantInfo structure, left */
             AACQuantInfo* quantInfoR,   /* AACQuantInfo structure, right */
	     int tag,
	     int commonWindow,
	     MS_Info* ms_info,
	     BsBitStream* fixedStream,  /* Pointer to bitstream */
	     int writeFlag);            /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteICS(...), write an individual_channel_stream element to the bitstream.*/
/*****************************************************************************/
int WriteICS(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
	     int commonWindow,
	     BsBitStream* fixedStream, /* Pointer to bitstream */
	     int writeFlag);           /* 1 means write, 0 means count only */ 

/*****************************************************************************/
/* WriteICSInfo(...), write a individual_channel_stream information          */
/*  to the bitstream.                                                        */
/*****************************************************************************/
int WriteICSInfo(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
		 BsBitStream* fixedStream, /* Pointer to bitstream */
		 int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteLTP_PredictorData(...), write LTP predictor data.                    */
/*****************************************************************************/
int WriteLTP_PredictorData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
                           BsBitStream* fixedStream, /* Pointer to bitstream */
                           int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WritePredictorData(...), write predictor data.                            */
/*****************************************************************************/
int WritePredictorData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
		       BsBitStream* fixedStream, /* Pointer to bitstream */
		       int writeFlag);           /* 1 means write, 0 means count only */   

/*****************************************************************************/
/* WritePulseData(...), write pulse data.                            */
/*****************************************************************************/
int WritePulseData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
		   BsBitStream* fixedStream, /* Pointer to bitstream */
		   int writeFlag);           /* 1 means write, 0 means count only */ 

/*****************************************************************************/
/* WriteTNSData(...), write TNS data.                            */
/*****************************************************************************/
int WriteTNSData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
		 BsBitStream* fixedStream, /* Pointer to bitstream */
		 int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteGainControlData(...), write gain control data.                       */
/*****************************************************************************/
int WriteGainControlData(
//                         AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
			 BsBitStream* fixedStream, /* Pointer to bitstream */
			 int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteSpectralData(...), write spectral data.                              */
/*****************************************************************************/
int WriteSpectralData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
		      BsBitStream* fixedStream, /* Pointer to bitstream */
		      int writeFlag);           /* 1 means write, 0 means count only */

#endif

