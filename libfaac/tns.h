/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: tns.h,v 1.1 2001/02/28 18:39:34 menno Exp $
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
					 int dataSize,		  /* Size of the data array */
					 double* data,		  /* Data array */
					 double* rArray);	  /* Autocorrelation array */

static double LevinsonDurbin(int maxOrder,        /* Maximum filter order */
					  int dataSize,		   /* Size of the data array */
					  double* data,		   /* Data array */
					  double* kArray);	   /* Reflection coeff array */

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
