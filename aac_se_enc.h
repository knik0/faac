/***********

This software module was originally developed by Texas 
Instruments in the course of development of the MPEG-2 NBC/MPEG-4 
Audio standard ISO/IEC13818-7, 14496-1, 2 and 3. This software module is an 
implementation of a part of one or more MPEG-2 NBC/MPEG-4 Audio tools as 
specified by the MPEG-2 NBC/MPEG-4 Audio standard. ISO/IEC  gives users of the
MPEG-2NBC/MPEG-4 Audio standards free license to this software module
or modifications thereof for use in hardware or software products
claiming conformance to the MPEG-2 NBC/MPEG-4 Audio  standards. Those
intending to use this software module in hardware or software products
are advised that this use may infringe existing patents. The original
developer of this software module, the subsequent
editors and their companies, and ISO/IEC have no liability for use of
this software module or modifications thereof in an
implementation. Copyright is not released for non MPEG-2 NBC/MPEG-4
Audio conforming products. The original developer retains full right to
use the code for the developer's own purpose, assign or donate the code to a
third party and to inhibit third party from using the code for non
MPEG-2 NBC/MPEG-4 Audio conforming products. This copyright notice
must be included in all copies or derivative works. Copyright 1996.  

***********/
/*********************************************************************
 *
 * Function prototypes for writing/counting AAC syntactic elements
 *
 * Authors:
 * CL    Chuck Lueck, TI <lueck@ti.com>
 * RG    Ralf Geiger, FhG/IIS
 *
 * Changes:
 * 07-jun-97   CL   Initial revision.
 * 14-sep-97   CL   Updated WritePredictorData to support prediction.
 * 20-oct-97   CL   Updated WriteTNSData to support TNS.
 * 22-Jan-98   CL   Added support for CPE's and common windows.
 * 07-Apr-98   RG   Added WriteLFE to write LFE channel element.
 *
**********************************************************************/


#ifndef AAC_SE_ENC
#define AAC_SE_ENC

#include "interface.h"
#include "bitstream.h"
#include "all.h"
#include "aac_qc.h"
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
int WriteGainControlData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
			 BsBitStream* fixedStream, /* Pointer to bitstream */
			 int writeFlag);           /* 1 means write, 0 means count only */

/*****************************************************************************/
/* WriteSpectralData(...), write spectral data.                              */
/*****************************************************************************/
int WriteSpectralData(AACQuantInfo* quantInfo,  /* Pointer to AACQuantInfo structure */
		      BsBitStream* fixedStream, /* Pointer to bitstream */
		      int writeFlag);           /* 1 means write, 0 means count only */

#endif

