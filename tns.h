/*
 *	Function prototypes for TNS
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
  $Revision: 1.5 $
  $Date: 2000/10/05 08:39:03 $ (check in)
  $Author: menno $
  *************************************************************************/
 
#ifndef _TNS_H_INCLUDED
#define _TNS_H_INCLUDED
 
#include <math.h>
#include <stdio.h>
#include "tf_main.h"
#include "interface.h"

/*************************/
/* #defines              */
/*************************/
#define TNS_MAX_ORDER 20				   
#define DEF_TNS_GAIN_THRESH 1.4 //new2
#define DEF_TNS_COEFF_THRESH 0.1
#define DEF_TNS_COEFF_RES 4
#define DEF_TNS_RES_OFFSET 3
#define PI C_PI
#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif
#ifndef max
#define max(a,b) ( (a) > (b) ? (a) : (b) )
#endif

/**************************/
/* Structure definitions  */
/**************************/
typedef struct {
  int order;                           /* Filter order */
  int direction;		       /* Filtering direction */
  int coefCompress;		       /* Are coeffs compressed? */
  int length;                          /* Length, in bands */                     
  double aCoeffs[TNS_MAX_ORDER+1];     /* AR Coefficients */
  double kCoeffs[TNS_MAX_ORDER+1];     /* Reflection Coefficients */
  int index[TNS_MAX_ORDER+1];	       /* Coefficient indices */
} TNS_FILTER_DATA;


typedef struct {
  int numFilters;				/* Number of filters */
  int coefResolution;				/* Coefficient resolution */
  TNS_FILTER_DATA tnsFilter[1<<LEN_TNS_NFILTL];	/* TNS filters */
} TNS_WINDOW_DATA;


typedef struct {
  int tnsDataPresent;
  int tnsMinBandNumberLong;
  int tnsMinBandNumberShort;
  int tnsMaxBandsLong;
  int tnsMaxBandsShort;
  int tnsMaxOrderLong;
  int tnsMaxOrderShort;
  TNS_WINDOW_DATA windowData[NSHORT];	/* TNS data per window */
} TNS_INFO;


/*************************/
/* Function prototypes   */
/*************************/
void Autocorrelation(int maxOrder,        /* Maximum autocorr order */
		     int dataSize,		  /* Size of the data array */
		     double* data,		  /* Data array */
		     double* rArray);	  /* Autocorrelation array */

double LevinsonDurbin(int maxOrder,       /* Maximum filter order */
		      int dataSize,		   /* Size of the data array */
		      double* data,		   /* Data array */
		      double* kArray);	   /* Reflection coeff array */

void StepUp(int fOrder,
	    double* kArray,
	    double* aArray);

void QuantizeReflectionCoeffs(int fOrder,int coeffRes,double* rArray,int* indexArray);
int TruncateCoeffs(int fOrder,double threshold,double* kArray);
void TnsFilter(int length,double* spec,TNS_FILTER_DATA* filter);
int TnsInvFilter(int length,double* spec,TNS_FILTER_DATA* filter);
void TnsInit(long samplingRate,enum AAC_PROFILE profile,TNS_INFO* tnsInfo); 
int TnsEncode(int numberOfBands,int maxSfb,enum WINDOW_TYPE blockType,int* sfbOffsetTable,double* spec,TNS_INFO* tnsInfo,int use_tns);

#endif

