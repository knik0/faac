/**************************************************************************

This software module was originally developed by
Nokia in the course of development of the MPEG-2 AAC/MPEG-4 
Audio standard ISO/IEC13818-7, 14496-1, 2 and 3.
This software module is an implementation of a part
of one or more MPEG-2 AAC/MPEG-4 Audio tools as specified by the
MPEG-2 aac/MPEG-4 Audio standard. ISO/IEC  gives users of the
MPEG-2aac/MPEG-4 Audio standards free license to this software module
or modifications thereof for use in hardware or software products
claiming conformance to the MPEG-2 aac/MPEG-4 Audio  standards. Those
intending to use this software module in hardware or software products
are advised that this use may infringe existing patents. The original
developer of this software module, the subsequent
editors and their companies, and ISO/IEC have no liability for use of
this software module or modifications thereof in an
implementation. Copyright is not released for non MPEG-2 aac/MPEG-4
Audio conforming products. The original developer retains full right to
use the code for the developer's own purpose, assign or donate the code to a
third party and to inhibit third party from using the code for non
MPEG-2 aac/MPEG-4 Audio conforming products. This copyright notice
must be included in all copies or derivative works.
Copyright (c)1997.  

***************************************************************************/

#ifndef _NOK_LTP_ENC_H
#define _NOK_LTP_ENC_H

#include "nok_ltp_common.h"

extern void nok_init_lt_pred (NOK_LT_PRED_STATUS * lt_status);

extern int nok_ltp_enc(double *p_spectrum, double *p_time_signal,
		       enum WINDOW_TYPE win_type, Window_shape win_shape,
		       int *sfb_offset, int num_of_sfb,
		       NOK_LT_PRED_STATUS *lt_status);

extern void nok_ltp_reconstruct(double *p_spectrum, enum WINDOW_TYPE win_type, 
                                Window_shape win_shape, 
                                int *sfb_offset, int num_of_sfb,
                                NOK_LT_PRED_STATUS *lt_status);

extern int nok_ltp_encode (BsBitStream *bs, enum WINDOW_TYPE win_type, int num_of_sfb, 
                           NOK_LT_PRED_STATUS *lt_status, int write_flag);

#endif /* not defined _NOK_LTP_ENC_H */

