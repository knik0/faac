/**********************************************************************

This software module was originally developed by
and edited by Texas Instruments in the course of
development of the MPEG-2 NBC/MPEG-4 Audio standard
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
**********************************************************************/
/*
 * $Id: tns.h,v 1.4 2001/09/04 18:39:36 menno Exp $
 */

#ifndef TNS_H
#define TNS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/*************************/
/* Function prototypes   */
/*************************/
static void Autocorrelation(int maxOrder,        /* Maximum autocorr order */
                     int dataSize,        /* Size of the data array */
                     double* data,        /* Data array */
                     double* rArray);     /* Autocorrelation array */

static double LevinsonDurbin(int maxOrder,        /* Maximum filter order */
                      int dataSize,        /* Size of the data array */
                      double* data,        /* Data array */
                      double* kArray);     /* Reflection coeff array */

static void StepUp(int fOrder, double* kArray, double* aArray);

static void QuantizeReflectionCoeffs(int fOrder,int coeffRes,double* rArray,int* indexArray);
static int TruncateCoeffs(int fOrder,double threshold,double* kArray);
static void TnsFilter(int length,double* spec,TnsFilterData* filter);
static void TnsInvFilter(int length,double* spec,TnsFilterData* filter);
void TnsInit(faacEncHandle hEncoder);
void TnsEncode(TnsInfo* tnsInfo, int numberOfBands,int maxSfb,enum WINDOW_TYPE blockType,
               int* sfbOffsetTable,double* spec);
void TnsEncodeFilterOnly(TnsInfo* tnsInfo, int numberOfBands, int maxSfb,
                         enum WINDOW_TYPE blockType, int *sfbOffsetTable, double *spec);
void TnsDecodeFilterOnly(TnsInfo* tnsInfo, int numberOfBands, int maxSfb,
                         enum WINDOW_TYPE blockType, int *sfbOffsetTable, double *spec);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TNS_H */
