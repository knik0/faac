/**************************************************************************

This software module was originally developed by

Mikko Suonio (Nokia)

in the course of development of the MPEG-2 NBC/MPEG-4 Audio standard
ISO/IEC 13818-7, 14496-1,2 and 3. This software module is an
implementation of a part of one or more MPEG-2 NBC/MPEG-4 Audio tools
as specified by the MPEG-2 NBC/MPEG-4 Audio standard. ISO/IEC gives
users of the MPEG-2 NBC/MPEG-4 Audio standards free license to this
software module or modifications thereof for use in hardware or
software products claiming conformance to the MPEG-2 NBC/ MPEG-4 Audio
standards. Those intending to use this software module in hardware or
software products are advised that this use may infringe existing
patents. The original developer of this software module and his/her
company, the subsequent editors and their companies, and ISO/IEC have
no liability for use of this software module or modifications thereof
in an implementation. Copyright is not released for non MPEG-2
NBC/MPEG-4 Audio conforming products. The original developer retains
full right to use the code for his/her own purpose, assign or donate
the code to a third party and to inhibit third party from using the
code for non MPEG-2 NBC/MPEG-4 Audio conforming products. This
copyright notice must be included in all copies or derivative works.

Copyright (c) 1997.

***************************************************************************/

#ifndef _NOK_LTP_COMMON_H
#define _NOK_LTP_COMMON_H

/*
  Macro:	MAX_SHORT_WINDOWS
  Purpose:	Number of short windows in one long window.
  Explanation:	-  */
#ifndef MAX_SHORT_WINDOWS
#define MAX_SHORT_WINDOWS NSHORT
#endif

/*
  Macro:	MAX_SCFAC_BANDS
  Purpose:	Maximum number of scalefactor bands in one frame.
  Explanation:	-  */
#ifndef MAX_SCFAC_BANDS
#define MAX_SCFAC_BANDS MAXBANDS
#endif

/*
  Macro:	BLOCK_LEN_LONG
  Purpose:	Length of one long window
  Explanation:	-  */
#ifndef BLOCK_LEN_LONG
#define BLOCK_LEN_LONG LN2
#endif


/*
  Macro:	NOK_MAX_BLOCK_LEN_LONG
  Purpose:	Informs the routine of the maximum block size used.
  Explanation:	This is needed since the TwinVQ long window
  		is different from the AAC long window.  */
#define	NOK_MAX_BLOCK_LEN_LONG BLOCK_LEN_LONG //(2 * BLOCK_LEN_LONG) 

/*
  Macro:	NOK_LT_BLEN
  Purpose:	Length of the history buffer.
  Explanation:	Has to hold two long windows of time domain data.  */
#ifndef	NOK_LT_BLEN
#define NOK_LT_BLEN (4 * NOK_MAX_BLOCK_LEN_LONG)
#endif

/*
  Type:		NOK_LT_PRED_STATUS
  Purpose:	Type of the struct holding the LTP encoding parameters.
  Explanation:	-  */
typedef struct
  {
    short buffer[NOK_LT_BLEN];
    double pred_mdct[2 * NOK_MAX_BLOCK_LEN_LONG];
    int weight_idx;
    double weight;
    int sbk_prediction_used[MAX_SHORT_WINDOWS];
    int sfb_prediction_used[MAX_SCFAC_BANDS];
    int *delay;
    int global_pred_flag;
    int side_info;
  }
NOK_LT_PRED_STATUS;


#endif /* _NOK_LTP_COMMON_H */
