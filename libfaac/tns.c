/**********************************************************************

This software module was originally developed by Texas Instruments
and edited by         in the course of
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
 * $Id: tns.c,v 1.11 2012/03/01 18:34:17 knik Exp $
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
/* Limit bands to > 2.0 kHz */
static unsigned short tnsMinBandNumberLong[12] =
{ 11, 12, 15, 16, 17, 20, 25, 26, 24, 28, 30, 31 };
static unsigned short tnsMinBandNumberShort[12] =
{ 2, 2, 2, 3, 3, 4, 6, 6, 8, 10, 10, 12 };

/**************************************/
/* Low Profile TNS Parameters         */
/**************************************/
static unsigned short tnsMaxBandsLongLow[12] =
{ 31, 31, 34, 40, 42, 51, 46, 46, 42, 42, 42, 39 };

static unsigned short tnsMaxBandsShortLow[12] =
{ 9, 9, 10, 14, 14, 14, 14, 14, 14, 14, 14, 14 };

static unsigned short tnsMaxOrderLongLow = 12;
static unsigned short tnsMaxOrderShortLow = 7;


/*************************/
/* Function prototypes   */
/*************************/
static void Autocorrelation(int maxOrder,                    /* Maximum autocorr order */
                            int dataSize,                    /* Size of the data array */
                            const float * restrict data, /* Data array */
                            float * restrict rArray);    /* Autocorrelation array */

static float LevinsonDurbin(int maxOrder,                    /* Maximum filter order */
                                int dataSize,                    /* Size of the data array */
                                const float * restrict data, /* Data array */
                                float * restrict kArray);    /* Reflection coeff array */

static void StepUp(int fOrder, float* kArray, float* aArray);

static void QuantizeReflectionCoeffs(int fOrder,int coeffRes,float* rArray,int* indexArray);
static int TruncateCoeffs(int fOrder,float threshold,float* kArray);
static void TnsInvFilter(int length, float * restrict spec,
                         const TnsFilterData * restrict filter,
                         float * restrict temp);

/*****************************************************/
/* InitTns:                                          */
/*****************************************************/
void TnsInit(faacEncStruct* hEncoder)
{
    unsigned int channel;
    int fsIndex = hEncoder->sampleRateIdx;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        TnsInfo *tnsInfo = &hEncoder->coderInfo[channel].tnsInfo;

        tnsInfo->tnsMaxBandsLong = tnsMaxBandsLongLow[fsIndex];
        tnsInfo->tnsMaxBandsShort = tnsMaxBandsShortLow[fsIndex];
        tnsInfo->tnsMaxOrderLong = tnsMaxOrderLongLow;
        tnsInfo->tnsMaxOrderShort = tnsMaxOrderShortLow;
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
               float* spec,            /* Spectral data array */
               float* temp)
{
    int numberOfWindows,windowSize;
    int startBand,stopBand,order;    /* Bands over which to apply TNS */
    int lengthInBands;               /* Length to filter, in bands */
    int w, i;
    int startIndex,length;
    float gain;

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
        float* k = tnsFilter->kCoeffs;    /* reflection coeffs */
        float* a = tnsFilter->aCoeffs;    /* prediction coeffs */

        windowData->numFilters=0;
        windowData->coefResolution = DEF_TNS_COEFF_RES;
        startIndex = w * windowSize + sfbOffsetTable[startBand];
        length = sfbOffsetTable[stopBand] - sfbOffsetTable[startBand];
        gain = LevinsonDurbin(order,length,&spec[startIndex],k);

        if (gain>DEF_TNS_GAIN_THRESH) {  /* Use TNS */
            int truncatedOrder;
            QuantizeReflectionCoeffs(order,DEF_TNS_COEFF_RES,k,tnsFilter->index);
            truncatedOrder = TruncateCoeffs(order,DEF_TNS_COEFF_THRESH,k);
            if (truncatedOrder == 0) continue;

            windowData->numFilters++;
            tnsInfo->tnsDataPresent=1;
            tnsFilter->direction = 0;

            tnsFilter->coefCompress = 1;
            for (i = 1; i <= truncatedOrder; i++) {
                int limit = 1 << (DEF_TNS_COEFF_RES - 2);
                if (tnsFilter->index[i] < -limit || tnsFilter->index[i] >= limit) {
                    tnsFilter->coefCompress = 0;
                    break;
                }
            }

            tnsFilter->length = lengthInBands;
            tnsFilter->order = truncatedOrder;
            StepUp(truncatedOrder,k,a);    /* Compute predictor coefficients */
            TnsInvFilter(length,&spec[startIndex],tnsFilter,temp);      /* Filter */
        }
    }
}



/********************************************************/
/* TnsInvFilter:                                        */
/*   Inverse filter the given spec with specified       */
/*   length using the coefficients specified in filter. */
/*   Note that the order and direction are specified    */
/*   within the TNS_FILTER_DATA structure.              */
/********************************************************/
static void TnsInvFilter(int length, float * restrict spec,
                         const TnsFilterData * restrict filter,
                         float * restrict temp)
{
    int i, j;
    const int order = filter->order;
    const float * restrict a = filter->aCoeffs;

    /* Determine loop parameters for given direction */
    if (filter->direction) {
        /* Backward direction (high-to-low index) */
        temp[length-1] = spec[length-1];
        for (i = length-2; i > (length-1-order); i--) {
            float acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= (length-1-i); j++)
                acc += temp[i+j] * a[j];
            spec[i] = acc;
        }
        /* Now filter the rest */
        for (i = length-1-order; i >= 0; i--) {
            float acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= order; j++)
                acc += temp[i+j] * a[j];
            spec[i] = acc;
        }
    } else {
        /* Forward direction (low-to-high index) */
        temp[0] = spec[0];
        for (i = 1; i < order; i++) {
            float acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= i; j++)
                acc += temp[i-j] * a[j];
            spec[i] = acc;
        }

        /* Now filter the rest */
        for (i = order; i < length; i++) {
            float acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= order; j++)
                acc += temp[i-j] * a[j];
            spec[i] = acc;
        }
    }
}





/*****************************************************/
/* TruncateCoeffs:                                   */
/*   Truncate the given reflection coeffs by zeroing */
/*   coefficients in the tail with absolute value    */
/*   less than the specified threshold.  Return the  */
/*   truncated filter order.                         */
/*****************************************************/
static int TruncateCoeffs(int fOrder,float threshold,float* kArray)
{
    int i;

    for (i = fOrder; i >= 0; i--) {
        kArray[i] = (fabsf(kArray[i])>threshold) ? kArray[i] : 0.0f;
        if (kArray[i]!=0.0f) return i;
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
                              float* kArray,
                              int* indexArray)
{
    float iqfac,iqfac_m;
    int i;

    iqfac = ((1<<(coeffRes-1))-0.5f)/(M_PI/2);
    iqfac_m = ((1<<(coeffRes-1))+0.5f)/(M_PI/2);

    /* Quantize and inverse quantize */
    for (i=1;i<=fOrder;i++) {
        indexArray[i] = (kArray[i]>=0)?(int)(0.5f+(asinf(kArray[i])*iqfac)):(int)(-0.5f+(asinf(kArray[i])*iqfac_m));
        kArray[i] = sinf((float)indexArray[i]/((indexArray[i]>=0)?iqfac:iqfac_m));
    }
}

/*****************************************************/
/* Autocorrelation,                                  */
/*   Compute the autocorrelation function            */
/*   estimate for the given data.                    */
/*****************************************************/
static void Autocorrelation(int maxOrder,                      /* Maximum autocorr order */
                            int dataSize,                     /* Size of the data array */
                            const float * restrict  data, /* Data array */
                            float * restrict rArray)      /* Autocorrelation array */
{
    int order, index;

    for (order = 0; order <= maxOrder; order++)
        rArray[order] = 0.0f;

    int limit = dataSize - maxOrder;
    for (index = 0; index < limit; index++) {
        const float d = data[index];
        const float * restrict dp = &data[index + 1];
        rArray[0] += d * d;
        for (order = 1; order <= maxOrder; order++)
            rArray[order] += d * dp[order - 1];
    }

    for (; index < dataSize; index++) {
        const float d = data[index];
        int n = dataSize - 1 - index;
        rArray[0] += d * d;
        for (order = 1; order <= n; order++)
            rArray[order] += d * data[index + order];
    }
}


/*****************************************************/
/* LevinsonDurbin:                                   */
/*   Compute the reflection coefficients for the     */
/*   given data using LevinsonDurbin recursion.      */
/*   Return the prediction gain.                     */
/*****************************************************/
static float LevinsonDurbin(int fOrder,                      /* Filter order */
                                int dataSize,                    /* Size of the data array */
                                const float * restrict data, /* Data array */
                                float * restrict kArray)     /* Reflection coeff array */
{
    int order,i;
    float signal;
    float error, kTemp;                /* Prediction error */
    float aArray1[TNS_MAX_ORDER+1];    /* Predictor coeff array */
    float aArray2[TNS_MAX_ORDER+1];    /* Predictor coeff array 2 */
    float rArray[TNS_MAX_ORDER+1] = {0}; /* Autocorrelation coeffs */
    float* aPtr = aArray1;             /* Ptr to aArray1 */
    float* aLastPtr = aArray2;         /* Ptr to aArray2 */
    float* aTemp;

    /* Compute autocorrelation coefficients */
    Autocorrelation(fOrder,dataSize,data,rArray);
    signal=rArray[0];   /* signal energy */

    /* Set up pointers to current and last iteration */
    /* predictor coefficients.                       */
    aPtr = aArray1;
    aLastPtr = aArray2;
    /* If there is no signal energy, return */
    if (!signal) {
        kArray[0]=1.0f;
        for (order=1;order<=fOrder;order++) {
            kArray[order]=0.0f;
        }
        return 0;

    } else {

        /* Set up first iteration */
        kArray[0]=1.0f;
        aPtr[0]=1.0f;        /* Ptr to predictor coeffs, current iteration*/
        aLastPtr[0]=1.0f;    /* Ptr to predictor coeffs, last iteration */
        error=rArray[0];

        /* Now perform recursion */
        for (order=1;order<=fOrder;order++) {
            kTemp = aLastPtr[0]*rArray[order-0];
            for (i=1;i<order;i++) {
                kTemp += aLastPtr[i]*rArray[order-i];
            }
            if (error <= 0.0f || fabsf(kTemp) >= error) {
                error = 0.0f;
                break;
            }
            kTemp = -kTemp/error;
            kArray[order]=kTemp;
            aPtr[order]=kTemp;
            for (i=1;i<order;i++) {
                aPtr[i] = aLastPtr[i] + kTemp*aLastPtr[order-i];
            }
            error = error * (1 - kTemp*kTemp);
            if (error <= 0.0f) break;

            /* Now make current iteration the last one */
            aTemp=aLastPtr;
            aLastPtr=aPtr;      /* Current becomes last */
            aPtr=aTemp;         /* Last becomes current */
        }
        /* If perfect prediction, trigger TNS */
        if (error <= 0.0f) return DEF_TNS_GAIN_THRESH + 1.0f;
        return signal/error;    /* return the gain */
    }
}


/*****************************************************/
/* StepUp:                                           */
/*   Convert reflection coefficients into            */
/*   predictor coefficients.                         */
/*****************************************************/
static void StepUp(int fOrder,float* kArray,float* aArray)
{
    float aTemp[TNS_MAX_ORDER+2];
    int i,order;

    aArray[0]=1.0f;
    aTemp[0]=1.0f;
    for (order=1;order<=fOrder;order++) {
        aArray[order]=0.0f;
        for (i=1;i<=order;i++) {
            aTemp[i] = aArray[i] + kArray[order]*aArray[order-i];
        }
        for (i=1;i<=order;i++) {
            aArray[i]=aTemp[i];
        }
    }
}
