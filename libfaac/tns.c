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
 * $Id: tns.c,v 1.3 2001/03/12 16:58:37 menno Exp $
 */

#include <math.h>
#include "frame.h"
#include "coder.h"
#include "bitstream.h"
#include "tns.h"
#include "util.h"

/***********************************************/
/* TNS Profile/Frequency Dependent Parameters  */
/***********************************************/
static unsigned long tnsSupportedSamplingRates[13] =
{ 96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,0 };

/* Limit bands to > 2.0 kHz */
static unsigned short tnsMinBandNumberLong[12] =
{ 11, 12, 15, 16, 17, 20, 25, 26, 24, 28, 30, 31 };
static unsigned short tnsMinBandNumberShort[12] = 
{ 2, 2, 2, 3, 3, 4, 6, 6, 8, 10, 10, 12 };
       
/**************************************/
/* Main/Low Profile TNS Parameters    */
/**************************************/
static unsigned short tnsMaxBandsLongMainLow[12] = 
{ 31, 31, 34, 40, 42, 51, 46, 46, 42, 42, 42, 39 };

static unsigned short tnsMaxBandsShortMainLow[12] = 
{ 9, 9, 10, 14, 14, 14, 14, 14, 14, 14, 14, 14 };

static unsigned short tnsMaxOrderLongMain = 20;
static unsigned short tnsMaxOrderLongLow = 12;
static unsigned short tnsMaxOrderShortMainLow = 7;

/**************************************/
/* SSR Profile TNS Parameters         */
/**************************************/
static unsigned short tnsMaxBandsLongSSR[12] = 
{ 28, 28, 27, 26, 26, 26, 29, 29, 23, 23, 23, 19 };

static unsigned short tnsMaxBandsShortSSR[12] = 
{ 7, 7, 7, 6, 6, 6, 7, 7, 8, 8, 8, 7 };

static unsigned short tnsMaxOrderLongSSR = 12;
static unsigned short tnsMaxOrderShortSSR = 7;


/*****************************************************/
/* InitTns:                                          */
/*****************************************************/
void TnsInit(faacEncHandle hEncoder)
{
	unsigned int channel;
	int fsIndex = hEncoder->sampleRateIdx;
	int profile = hEncoder->config.aacProfile;

	for (channel = 0; channel < hEncoder->numChannels; channel++) {
		TnsInfo *tnsInfo = &hEncoder->coderInfo[channel].tnsInfo;

		switch( profile ) {
		case MAIN :
			tnsInfo->tnsMaxBandsLong = tnsMaxBandsLongMainLow[fsIndex];
			tnsInfo->tnsMaxBandsShort = tnsMaxBandsShortMainLow[fsIndex];
			tnsInfo->tnsMaxOrderLong = tnsMaxOrderLongMain;
			tnsInfo->tnsMaxOrderShort = tnsMaxOrderShortMainLow;
			break;
		case LOW :
			tnsInfo->tnsMaxBandsLong = tnsMaxBandsLongMainLow[fsIndex];
			tnsInfo->tnsMaxBandsShort = tnsMaxBandsShortMainLow[fsIndex];
			tnsInfo->tnsMaxOrderLong = tnsMaxOrderLongLow;
			tnsInfo->tnsMaxOrderShort = tnsMaxOrderShortMainLow;
			break;
		case SSR :
			tnsInfo->tnsMaxBandsLong = tnsMaxBandsLongSSR[fsIndex];
			tnsInfo->tnsMaxBandsShort = tnsMaxBandsShortSSR[fsIndex];
			tnsInfo->tnsMaxOrderLong = tnsMaxOrderLongSSR;
			tnsInfo->tnsMaxOrderShort = tnsMaxOrderShortSSR;
			break;
		}
		tnsInfo->tnsMinBandNumberLong = tnsMinBandNumberLong[fsIndex];
		tnsInfo->tnsMinBandNumberShort = tnsMinBandNumberShort[fsIndex];
	}
}


/*****************************************************/
/* TnsEncode:                                        */
/*****************************************************/
void TnsEncode(TnsInfo* tnsInfo,       /* TNS info */
			   int numberOfBands,       /* Number of bands per window */
			   int maxSfb,              /* max_sfb */
			   enum WINDOW_TYPE blockType,   /* block type */
			   int* sfbOffsetTable,     /* Scalefactor band offset table */
			   double* spec)            /* Spectral data array */
{
	int numberOfWindows,windowSize;
	int startBand,stopBand,order;    /* Bands over which to apply TNS */
	int lengthInBands;               /* Length to filter, in bands */
	int w;
	int startIndex,length;
	double gain;

	switch( blockType ) {
	case ONLY_SHORT_WINDOW :

		/* TNS not used for short blocks currently */
		tnsInfo->tnsDataPresent = 0;
		return;

		numberOfWindows = MAX_SHORT_WINDOWS;
		windowSize = BLOCK_LEN_SHORT;
		startBand = tnsInfo->tnsMinBandNumberShort;
		stopBand = numberOfBands; 
		lengthInBands = stopBand-startBand;
		order = tnsInfo->tnsMaxOrderShort;
		startBand = min(startBand,tnsInfo->tnsMaxBandsShort);
		stopBand = min(stopBand,tnsInfo->tnsMaxBandsShort);
		break;

	default: 
		numberOfWindows = 1;
		windowSize = BLOCK_LEN_SHORT;
		startBand = tnsInfo->tnsMinBandNumberLong;
		stopBand = numberOfBands;
		lengthInBands = stopBand - startBand;
		order = tnsInfo->tnsMaxOrderLong;
		startBand = min(startBand,tnsInfo->tnsMaxBandsLong);
		stopBand = min(stopBand,tnsInfo->tnsMaxBandsLong);
		break;
	}
	
	/* Make sure that start and stop bands < maxSfb */
	/* Make sure that start and stop bands >= 0 */
	startBand = min(startBand,maxSfb);
	stopBand = min(stopBand,maxSfb);
	startBand = max(startBand,0);
	stopBand = max(stopBand,0);

	tnsInfo->tnsDataPresent = 0;     /* default TNS not used */
	
	/* Perform analysis and filtering for each window */
	for (w=0;w<numberOfWindows;w++) {

		TnsWindowData* windowData = &tnsInfo->windowData[w];
		TnsFilterData* tnsFilter = windowData->tnsFilter;
		double* k = tnsFilter->kCoeffs;    /* reflection coeffs */
		double* a = tnsFilter->aCoeffs;    /* prediction coeffs */

		windowData->numFilters=0;
		windowData->coefResolution = DEF_TNS_COEFF_RES;
		startIndex = w * windowSize + sfbOffsetTable[startBand];
		length = sfbOffsetTable[stopBand] - sfbOffsetTable[startBand];
		gain = LevinsonDurbin(order,length,&spec[startIndex],k);

		if (gain>DEF_TNS_GAIN_THRESH) {  /* Use TNS */
			int truncatedOrder;
			windowData->numFilters++;
			tnsInfo->tnsDataPresent=1;
			tnsFilter->direction = 0;
			tnsFilter->coefCompress = 0;
			tnsFilter->length = lengthInBands;
			QuantizeReflectionCoeffs(order,DEF_TNS_COEFF_RES,k,tnsFilter->index);
			truncatedOrder = TruncateCoeffs(order,DEF_TNS_COEFF_THRESH,k);
			tnsFilter->order = truncatedOrder;
			StepUp(truncatedOrder,k,a);    /* Compute predictor coefficients */
			TnsInvFilter(length,&spec[startIndex],tnsFilter);      /* Filter */      
		}
	}
}


/*****************************************************/
/* TnsEncodeFilterOnly:                              */
/* This is a stripped-down version of TnsEncode()    */
/* which performs TNS analysis filtering only        */
/*****************************************************/
void TnsEncodeFilterOnly(TnsInfo* tnsInfo,           /* TNS info */
						 int numberOfBands,          /* Number of bands per window */
						 int maxSfb,                 /* max_sfb */
						 enum WINDOW_TYPE blockType, /* block type */
						 int* sfbOffsetTable,        /* Scalefactor band offset table */
						 double* spec)               /* Spectral data array */
{
	int numberOfWindows,windowSize;
	int startBand,stopBand;    /* Bands over which to apply TNS */
	int w;
	int startIndex,length;

	switch( blockType ) {
    case ONLY_SHORT_WINDOW :
		numberOfWindows = MAX_SHORT_WINDOWS;
		windowSize = BLOCK_LEN_SHORT;
		startBand = tnsInfo->tnsMinBandNumberShort;
		stopBand = numberOfBands; 
		startBand = min(startBand,tnsInfo->tnsMaxBandsShort);
		stopBand = min(stopBand,tnsInfo->tnsMaxBandsShort);
		break;
		
    default: 
		numberOfWindows = 1;
		windowSize = BLOCK_LEN_LONG;
		startBand = tnsInfo->tnsMinBandNumberLong;
		stopBand = numberOfBands;
		startBand = min(startBand,tnsInfo->tnsMaxBandsLong);
		stopBand = min(stopBand,tnsInfo->tnsMaxBandsLong);
		break;
	}
	
	/* Make sure that start and stop bands < maxSfb */
	/* Make sure that start and stop bands >= 0 */
	startBand = min(startBand,maxSfb);
	stopBand = min(stopBand,maxSfb);
	startBand = max(startBand,0);
	stopBand = max(stopBand,0);
	
	
	/* Perform filtering for each window */
	for(w=0;w<numberOfWindows;w++) 
	{
		TnsWindowData* windowData = &tnsInfo->windowData[w];
		TnsFilterData* tnsFilter = windowData->tnsFilter;
		
		startIndex = w * windowSize + sfbOffsetTable[startBand];
		length = sfbOffsetTable[stopBand] - sfbOffsetTable[startBand];
		
		if (tnsInfo->tnsDataPresent  &&  windowData->numFilters) {  /* Use TNS */
			TnsInvFilter(length,&spec[startIndex],tnsFilter);   
		}
	}
}


/*****************************************************/
/* TnsDecodeFilterOnly:                              */
/* This is a stripped-down version of TnsEncode()    */
/* which performs TNS synthesis filtering only       */
/*****************************************************/
void TnsDecodeFilterOnly(TnsInfo* tnsInfo,           /* TNS info */
						 int numberOfBands,          /* Number of bands per window */
						 int maxSfb,                 /* max_sfb */
						 enum WINDOW_TYPE blockType, /* block type */
						 int* sfbOffsetTable,        /* Scalefactor band offset table */
						 double* spec)               /* Spectral data array */
{
	int numberOfWindows,windowSize;
	int startBand,stopBand;    /* Bands over which to apply TNS */
	int w;
	int startIndex,length;

	switch( blockType ) {
    case ONLY_SHORT_WINDOW :
		numberOfWindows = MAX_SHORT_WINDOWS;
		windowSize = BLOCK_LEN_SHORT;
		startBand = tnsInfo->tnsMinBandNumberShort;
		stopBand = numberOfBands; 
		startBand = min(startBand,tnsInfo->tnsMaxBandsShort);
		stopBand = min(stopBand,tnsInfo->tnsMaxBandsShort);
		break;
		
    default: 
		numberOfWindows = 1;
		windowSize = BLOCK_LEN_LONG;
		startBand = tnsInfo->tnsMinBandNumberLong;
		stopBand = numberOfBands;
		startBand = min(startBand,tnsInfo->tnsMaxBandsLong);
		stopBand = min(stopBand,tnsInfo->tnsMaxBandsLong);
		break;
	}
	
	/* Make sure that start and stop bands < maxSfb */
	/* Make sure that start and stop bands >= 0 */
	startBand = min(startBand,maxSfb);
	stopBand = min(stopBand,maxSfb);
	startBand = max(startBand,0);
	stopBand = max(stopBand,0);
	
	
	/* Perform filtering for each window */
	for(w=0;w<numberOfWindows;w++) 
	{
		TnsWindowData* windowData = &tnsInfo->windowData[w];
		TnsFilterData* tnsFilter = windowData->tnsFilter;
		
		startIndex = w * windowSize + sfbOffsetTable[startBand];
		length = sfbOffsetTable[stopBand] - sfbOffsetTable[startBand];
		
		if (tnsInfo->tnsDataPresent  &&  windowData->numFilters) {  /* Use TNS */
			TnsFilter(length,&spec[startIndex],tnsFilter);   
		}
	}
}


/*****************************************************/
/* TnsFilter:                                        */
/*   Filter the given spec with specified length     */
/*   using the coefficients specified in filter.     */
/*   Not that the order and direction are specified  */
/*   withing the TNS_FILTER_DATA structure.          */
/*****************************************************/
static void TnsFilter(int length,double* spec,TnsFilterData* filter)
{
	int i,j,k=0;
	int order=filter->order;
	double* a=filter->aCoeffs;

	/* Determine loop parameters for given direction */
	if (filter->direction) {

		/* Startup, initial state is zero */
		for (i=length-2;i>(length-1-order);i--) {
			k++;
			for (j=1;j<=k;j++) {
				spec[i]-=spec[i+j]*a[j];
			}
		}
		
		/* Now filter completely inplace */
		for	(i=length-1-order;i>=0;i--) {
			for (j=1;j<=order;j++) {
				spec[i]-=spec[i+j]*a[j];
			}
		}


	} else {

		/* Startup, initial state is zero */
		for (i=1;i<order;i++) {
			for (j=1;j<=i;j++) {
				spec[i]-=spec[i-j]*a[j];
			}
		}
		
		/* Now filter completely inplace */
		for	(i=order;i<length;i++) {
			for (j=1;j<=order;j++) {
				spec[i]-=spec[i-j]*a[j];
			}
		}
	}
}


/********************************************************/
/* TnsInvFilter:                                        */
/*   Inverse filter the given spec with specified       */
/*   length using the coefficients specified in filter. */
/*   Not that the order and direction are specified     */
/*   withing the TNS_FILTER_DATA structure.             */
/********************************************************/
static void TnsInvFilter(int length,double* spec,TnsFilterData* filter)
{
	int i,j,k=0;
	int order=filter->order;
	double* a=filter->aCoeffs;
	double* temp;

    temp = (double *)AllocMemory(length * sizeof (double));

	/* Determine loop parameters for given direction */
	if (filter->direction) {

		/* Startup, initial state is zero */
		temp[length-1]=spec[length-1];
		for (i=length-2;i>(length-1-order);i--) {
			temp[i]=spec[i];
			k++;
			for (j=1;j<=k;j++) {
				spec[i]+=temp[i+j]*a[j];
			}
		}
		
		/* Now filter the rest */
		for	(i=length-1-order;i>=0;i--) {
			temp[i]=spec[i];
			for (j=1;j<=order;j++) {
				spec[i]+=temp[i+j]*a[j];
			}
		}


	} else {

		/* Startup, initial state is zero */
		temp[0]=spec[0];
		for (i=1;i<order;i++) {
			temp[i]=spec[i];
			for (j=1;j<=i;j++) {
				spec[i]+=temp[i-j]*a[j];
			}
		}
		
		/* Now filter the rest */
		for	(i=order;i<length;i++) {
			temp[i]=spec[i];
			for (j=1;j<=order;j++) {
				spec[i]+=temp[i-j]*a[j];
			}
		}
	}
	if (temp) FreeMemory(temp);
}





/*****************************************************/
/* TruncateCoeffs:                                   */
/*   Truncate the given reflection coeffs by zeroing */
/*   coefficients in the tail with absolute value    */
/*   less than the specified threshold.  Return the  */
/*   truncated filter order.                         */
/*****************************************************/
static int TruncateCoeffs(int fOrder,double threshold,double* kArray)
{
	int i;

	for (i = fOrder; i >= 0; i--) {
		kArray[i] = (fabs(kArray[i])>threshold) ? kArray[i] : 0.0;
		if (kArray[i]!=0.0) return i;
	}

	return 0;
}

/*****************************************************/
/* QuantizeReflectionCoeffs:                         */
/*   Quantize the given array of reflection coeffs   */
/*   to the specified resolution in bits.            */
/*****************************************************/
static void QuantizeReflectionCoeffs(int fOrder,
							  int coeffRes,
							  double* kArray,
							  int* indexArray)
{
	double iqfac,iqfac_m;
	int i;

	iqfac = ((1<<(coeffRes-1))-0.5)/(M_PI/2);
	iqfac_m = ((1<<(coeffRes-1))+0.5)/(M_PI/2);

	/* Quantize and inverse quantize */
	for (i=1;i<=fOrder;i++) {
		indexArray[i] = (int)(0.5+(asin(kArray[i])*((kArray[i]>=0)?iqfac:iqfac_m)));
		kArray[i] = sin((double)indexArray[i]/((indexArray[i]>=0)?iqfac:iqfac_m));
	}
}

/*****************************************************/
/* Autocorrelation,                                  */
/*   Compute the autocorrelation function            */
/*   estimate for the given data.                    */
/*****************************************************/
static void Autocorrelation(int maxOrder,        /* Maximum autocorr order */
					 int dataSize,		  /* Size of the data array */
					 double* data,		  /* Data array */
					 double* rArray)  	  /* Autocorrelation array */
{
	int order,index;

	for (order=0;order<=maxOrder;order++) {
		rArray[order]=0.0;
		for (index=0;index<dataSize;index++) {
			rArray[order]+=data[index]*data[index+order];
		}
		dataSize--;
	}
}



/*****************************************************/
/* LevinsonDurbin:                                   */
/*   Compute the reflection coefficients for the     */
/*   given data using LevinsonDurbin recursion.      */
/*   Return the prediction gain.                     */
/*****************************************************/
static double LevinsonDurbin(int fOrder,          /* Filter order */
					  int dataSize,		   /* Size of the data array */
					  double* data,		   /* Data array */
					  double* kArray)  	   /* Reflection coeff array */
{
	int order,i;
	double signal;
	double error, kTemp;				/* Prediction error */
	double aArray1[TNS_MAX_ORDER+1];	/* Predictor coeff array */
	double aArray2[TNS_MAX_ORDER+1];	/* Predictor coeff array 2 */
	double rArray[TNS_MAX_ORDER+1];		/* Autocorrelation coeffs */
	double* aPtr = aArray1;				/* Ptr to aArray1 */
	double* aLastPtr = aArray2;			/* Ptr to aArray2 */
	double* aTemp;

	/* Compute autocorrelation coefficients */
	Autocorrelation(fOrder,dataSize,data,rArray);
	signal=rArray[0];	/* signal energy */

	/* Set up pointers to current and last iteration */ 
	/* predictor coefficients.						 */
	aPtr = aArray1;
	aLastPtr = aArray2;
	/* If there is no signal energy, return */
	if (!signal) {
		kArray[0]=1.0;
		for (order=1;order<=fOrder;order++) {
			kArray[order]=0.0;
		}
		return 0;

	} else {

		/* Set up first iteration */
		kArray[0]=1.0;
		aPtr[0]=1.0;		/* Ptr to predictor coeffs, current iteration*/
		aLastPtr[0]=1.0;	/* Ptr to predictor coeffs, last iteration */
		error=rArray[0];

		/* Now perform recursion */
		for (order=1;order<=fOrder;order++) {
			kTemp = aLastPtr[0]*rArray[order-0];
			for (i=1;i<order;i++) {
				kTemp += aLastPtr[i]*rArray[order-i];
			}
			kTemp = -kTemp/error;
			kArray[order]=kTemp;
			aPtr[order]=kTemp;
			for (i=1;i<order;i++) {
				aPtr[i] = aLastPtr[i] + kTemp*aLastPtr[order-i];
			}
			error = error * (1 - kTemp*kTemp);
			
			/* Now make current iteration the last one */
			aTemp=aLastPtr;
			aLastPtr=aPtr;		/* Current becomes last */
			aPtr=aTemp;			/* Last becomes current */
		}
		return signal/error;	/* return the gain */
	}
}


/*****************************************************/
/* StepUp:                                           */
/*   Convert reflection coefficients into            */
/*   predictor coefficients.                         */
/*****************************************************/
static void StepUp(int fOrder,double* kArray,double* aArray)
{
	double aTemp[TNS_MAX_ORDER+2];
	int i,order;

	aArray[0]=1.0;
	aTemp[0]=1.0;
	for (order=1;order<=fOrder;order++) {
		aArray[order]=0.0;
		for (i=1;i<=order;i++) {
			aTemp[i] = aArray[i] + kArray[order]*aArray[order-i];
		}
		for (i=1;i<=order;i++) {
			aArray[i]=aTemp[i];
		}
	}
}
