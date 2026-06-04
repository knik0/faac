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

/* TNS analysis pre-gate thresholds.
 * Validated via corpus sweeps to prevent TNS activation on non-beneficial frames. */
#define TNS_ENERGY_FLOOR  0.16  /* Min MDCT RMS to avoid processing near-silent frames */
#define TNS_FLATNESS_K    1.7   /* Spectral flatness gate (L2^2*N/L1^2) */
#define TNS_PEAK_RATIO_MARGIN 1.2  /* Peak-to-mean ratio margin above Gaussian expected peak */

/* TNS break-even gain analysis constants. */
#define TNS_SPECTRAL_FRAC   0.65    /* Estimated fraction of frame bits for spectral data */
#define TNS_FIXED_OVERHEAD  14      /* Fixed bitstream overhead per TNS filter */
#define TNS_CALIBRATION     1.029   /* Calibration factor against corpus anchor */
#define TNS_THRESH_FLOOR    1.10    /* Minimum gain threshold for TNS utility */
#define TNS_THRESH_CAP      1.40    /* Maximum adaptive threshold cap */

/*************************/
/* Function prototypes   */
/*************************/
static void Autocorrelation(int maxOrder,
                     int dataSize,
                     const faac_real * restrict data,
                     faac_real * restrict rArray);

static faac_real LevinsonDurbin(int maxOrder,
                      int dataSize,
                      const faac_real * restrict data,
                      faac_real * restrict kArray);

static void StepUp(int fOrder, faac_real* kArray, faac_real* aArray);

static void QuantizeReflectionCoeffs(int fOrder,int coeffRes,faac_real* rArray,int* indexArray);
static int TruncateCoeffs(int fOrder,faac_real threshold,faac_real* kArray);
static void TnsInvFilter(int length,faac_real* spec,TnsFilterData* filter, faac_real *temp);

static void WhitenSpectrumForTns(const faac_real * restrict spec,
                                 faac_real * restrict out,
                                 const int * restrict sfbOffsetTable,
                                 const faac_real * restrict sfbEnergy,
                                 int startBand, int stopBand,
                                 int startLine, int stopLine);

/*****************************************************/
/* InitTns:                                          */
/*****************************************************/
void TnsInit(faacEncStruct* hEncoder)
{
    unsigned int channel;
    int fsIndex = hEncoder->sampleRateIdx;
    unsigned long bitratePerCh = (hEncoder->numChannels > 0)
        ? hEncoder->config.bitRate / hEncoder->numChannels : 0;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        TnsInfo *tnsInfo = &hEncoder->coderInfo[channel].tnsInfo;

        tnsInfo->tnsMaxBandsLong       = tnsMaxBandsLongLow[fsIndex];
        tnsInfo->tnsMaxBandsShort      = tnsMaxBandsShortLow[fsIndex];
        tnsInfo->tnsMaxOrderShort      = tnsMaxOrderShortLow;
        tnsInfo->tnsMinBandNumberLong  = tnsMinBandNumberLong[fsIndex];
        tnsInfo->tnsMinBandNumberShort = tnsMinBandNumberShort[fsIndex];

        /* Adaptive max order for long windows based on per-channel bitrate. */
        if (bitratePerCh >= 128000) {
            tnsInfo->tnsMaxOrderLong = 6;
        } else if (bitratePerCh >= 96000) {
            tnsInfo->tnsMaxOrderLong = 8;
        } else {
            tnsInfo->tnsMaxOrderLong = tnsMaxOrderLongLow; /* 12 */
        }

        /* Long-window gain threshold via break-even bit budget formula. */
        if (bitratePerCh == 0) {
            tnsInfo->gainThreshLong = (faac_real)TNS_THRESH_FLOOR;
        } else {
            int frame_bits    = (int)((unsigned long)bitratePerCh * FRAME_LEN
                                      / hEncoder->sampleRate);
            int spectral_bits = (int)(frame_bits * TNS_SPECTRAL_FRAC);
            int tns_overhead  = tnsInfo->tnsMaxOrderLong * DEF_TNS_COEFF_RES
                                + TNS_FIXED_OVERHEAD;
            int denom = spectral_bits - tns_overhead;
            faac_real thresh;
            if (denom <= 0) {
                thresh = (faac_real)TNS_THRESH_CAP;
            } else {
                thresh = ((faac_real)spectral_bits / (faac_real)denom)
                         * (faac_real)TNS_CALIBRATION;
                if (thresh < (faac_real)TNS_THRESH_FLOOR)
                    thresh = (faac_real)TNS_THRESH_FLOOR;
                if (thresh > (faac_real)TNS_THRESH_CAP)
                    thresh = (faac_real)TNS_THRESH_CAP;
            }
            tnsInfo->gainThreshLong = thresh;
        }

        /* Short-window gain threshold: applied per 128-line window. */
        if (bitratePerCh == 0) {
            tnsInfo->gainThreshShort = (faac_real)TNS_THRESH_FLOOR;
        } else {
            int frame_bits_s = (int)((unsigned long)bitratePerCh * FRAME_LEN
                                     / hEncoder->sampleRate);
            int spectral_s   = (int)(frame_bits_s * TNS_SPECTRAL_FRAC) / MAX_SHORT_WINDOWS;
            int overhead_s   = tnsInfo->tnsMaxOrderShort * DEF_TNS_COEFF_RES
                               + TNS_FIXED_OVERHEAD;
            int denom_s      = spectral_s - overhead_s;
            faac_real thresh_s;
            if (denom_s <= 0) {
                thresh_s = (faac_real)TNS_THRESH_CAP;
            } else {
                thresh_s = ((faac_real)spectral_s / (faac_real)denom_s)
                           * (faac_real)TNS_CALIBRATION;
                if (thresh_s < (faac_real)TNS_THRESH_FLOOR)
                    thresh_s = (faac_real)TNS_THRESH_FLOOR;
                if (thresh_s > (faac_real)TNS_THRESH_CAP)
                    thresh_s = (faac_real)TNS_THRESH_CAP;
            }
            /* Apply 20% spectral budget gate for short TNS filters. */
            {
                int s_total = (int)(frame_bits_s * TNS_SPECTRAL_FRAC);
                if (MAX_SHORT_WINDOWS * overhead_s * 5 >= s_total)
                    thresh_s = (faac_real)TNS_THRESH_CAP;
            }
            tnsInfo->gainThreshShort = thresh_s;
        }
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
               faac_real* spec,            /* Spectral data array */
               faac_real* temp)
{
    int numberOfWindows,windowSize;
    int startBand,stopBand,order;    /* Bands over which to apply TNS */
    int lengthInBands;               /* Length to filter, in bands */
    int w;
    int startIndex,length;
    faac_real gain;

    switch( blockType ) {
    case ONLY_SHORT_WINDOW :
        /* Short-window TNS disabled due to throughput cost vs MOS gain. */
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
        windowSize = BLOCK_LEN_LONG;
        startBand = tnsInfo->tnsMinBandNumberLong;
        stopBand = numberOfBands;
        lengthInBands = stopBand - startBand;
        order = tnsInfo->tnsMaxOrderLong;
        startBand = min(startBand,tnsInfo->tnsMaxBandsLong);
        stopBand = min(stopBand,tnsInfo->tnsMaxBandsLong);
        break;
    }

    faac_real gainThreshCur = (blockType == ONLY_SHORT_WINDOW)
        ? tnsInfo->gainThreshShort : tnsInfo->gainThreshLong;

    /* Make sure that start and stop bands < maxSfb */
    /* Make sure that start and stop bands >= 0 */
    startBand = min(startBand,maxSfb);
    stopBand = min(stopBand,maxSfb);
    startBand = max(startBand,0);
    stopBand = max(stopBand,0);

    tnsInfo->tnsDataPresent = 0;     /* default TNS not used */

    /* Pre-gate thresholds. */
    startIndex = sfbOffsetTable[startBand];
    length = sfbOffsetTable[stopBand] - startIndex;
    faac_real peak_thresh = (length > 0) ? (TNS_PEAK_RATIO_MARGIN
                                            * FAAC_SQRT(2.0 * FAAC_LOG((faac_real)length))) : 0.0;

    /* Perform analysis and filtering for each window */
    for (w=0;w<numberOfWindows;w++) {

        TnsWindowData* windowData = &tnsInfo->windowData[w];
        TnsFilterData* tnsFilter = windowData->tnsFilter;
        faac_real* k = tnsFilter->kCoeffs;    /* reflection coeffs */
        faac_real* a = tnsFilter->aCoeffs;    /* prediction coeffs */
        faac_real sfbEnergy[NSFB_LONG];
        faac_real* wspec = spec + w * windowSize;

        windowData->numFilters=0;
        windowData->coefResolution = DEF_TNS_COEFF_RES;

        /* Combined pre-gate and per-SFB energy accumulation. */
        {
            faac_real sumsq = 0.0, suma = 0.0, maxa = 0.0;
            int sfb;
            for (sfb = startBand; sfb < stopBand; sfb++) {
                faac_real e = 0.0;
                int j;
                int sfb_start = sfbOffsetTable[sfb];
                int sfb_end = sfbOffsetTable[sfb + 1];
                const faac_real *pspec = &wspec[sfb_start];
                int n = sfb_end - sfb_start;

                for (j = 0; j < n; j++) {
                    faac_real v = pspec[j];
                    faac_real va = FAAC_FABS(v);
                    e    += v * v;
                    suma += va;
                    if (va > maxa) maxa = va;
                }
                sfbEnergy[sfb] = e;
                sumsq += e;
            }

            if (sumsq < TNS_ENERGY_FLOOR * length
                || suma <= 0.0
                || sumsq * length < TNS_FLATNESS_K * suma * suma
                || maxa * length < peak_thresh * suma) {
                continue;
            }
        }

        /* Run Levinson-Durbin on whitened spectrum to focus on within-band correlation. */
        WhitenSpectrumForTns(wspec, temp, sfbOffsetTable, sfbEnergy,
                             startBand, stopBand,
                             startIndex, startIndex + length);
        gain = LevinsonDurbin(order,length,&temp[startIndex],k);

        if (gain > gainThreshCur) {
            int truncatedOrder;
            QuantizeReflectionCoeffs(order,DEF_TNS_COEFF_RES,k,tnsFilter->index);
            truncatedOrder = TruncateCoeffs(order,DEF_TNS_COEFF_THRESH,k);
            if (truncatedOrder == 0) continue;

            windowData->numFilters++;
            tnsInfo->tnsDataPresent=1;
            tnsFilter->direction = 0;
            tnsFilter->coefCompress = 0;
            tnsFilter->length = lengthInBands;
            tnsFilter->order = truncatedOrder;
            StepUp(truncatedOrder,k,a);
            TnsInvFilter(length,&wspec[startIndex],tnsFilter,temp);
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
                         faac_real* spec,               /* Spectral data array */
                         faac_real* temp)
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
            TnsInvFilter(length,&spec[startIndex],tnsFilter,temp);
        }
    }
}




/********************************************************/
/* TnsInvFilter:                                        */
/*   Apply inverse filtering to the spectrum.           */
/********************************************************/
static void TnsInvFilter(int length,faac_real* spec,TnsFilterData* filter, faac_real *temp)
{
    int i,j;
    int order=filter->order;
    faac_real* a=filter->aCoeffs;

    if (filter->direction) {
        /* Backward direction (high-to-low index) */
        temp[length-1] = spec[length-1];
        for (i = length-2; i > (length-1-order); i--) {
            faac_real acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= (length-1-i); j++)
                acc += temp[i+j] * a[j];
            spec[i] = acc;
        }
        for (i = length-1-order; i >= 0; i--) {
            faac_real acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= order; j++)
                acc += temp[i+j] * a[j];
            spec[i] = acc;
        }
    } else {
        /* Forward direction (low-to-high index) */
        temp[0] = spec[0];
        for (i = 1; i < order; i++) {
            faac_real acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= i; j++)
                acc += temp[i-j] * a[j];
            spec[i] = acc;
        }
        for (i = order; i < length; i++) {
            faac_real acc = spec[i];
            temp[i] = acc;
            for (j = 1; j <= order; j++)
                acc += temp[i-j] * a[j];
            spec[i] = acc;
        }
    }
}





/*****************************************************/
/* TruncateCoeffs:                                   */
/*   Zero out small tail coefficients.               */
/*****************************************************/
static int TruncateCoeffs(int fOrder,faac_real threshold,faac_real* kArray)
{
    int i;

    for (i = fOrder; i >= 0; i--) {
        kArray[i] = (FAAC_FABS(kArray[i])>threshold) ? kArray[i] : 0.0;
        if (kArray[i]!=0.0) return i;
    }

    return 0;
}

/*****************************************************/
/* QuantizeReflectionCoeffs:                         */
/*   Quantize reflection coefficients with clamping. */
/*****************************************************/
static void QuantizeReflectionCoeffs(int fOrder,
                              int coeffRes,
                              faac_real* kArray,
                              int* indexArray)
{
    faac_real iqfac,iqfac_m;
    int i;

    iqfac = ((1<<(coeffRes-1))-0.5)/(M_PI/2);
    iqfac_m = ((1<<(coeffRes-1))+0.5)/(M_PI/2);

    /* Range clamping prevents index wrapping and encoder/decoder mismatch. */
    {
        const int i_max =  (1 << (coeffRes - 1)) - 1;
        const int i_min = -(1 << (coeffRes - 1));
        for (i = 1; i <= fOrder; i++) {
            int idx = (kArray[i] >= 0)
                    ? (int)(0.5  + FAAC_ASIN(kArray[i]) * iqfac)
                    : (int)(-0.5 + FAAC_ASIN(kArray[i]) * iqfac_m);
            if (idx > i_max) idx = i_max;
            if (idx < i_min) idx = i_min;
            indexArray[i] = idx;
            kArray[i] = FAAC_SIN((faac_real)idx / (idx >= 0 ? iqfac : iqfac_m));
        }
    }
}

/*****************************************************/
/* Autocorrelation,                                  */
/*   Optimized single-pass autocorrelation.          */
/*****************************************************/
static void Autocorrelation(int maxOrder,
                     int dataSize,
                     const faac_real * restrict data,
                     faac_real * restrict rArray)
{
    int order, index;

    for (order = 0; order <= maxOrder; order++)
        rArray[order] = 0.0;

    for (index = 0; index < dataSize; index++) {
        faac_real d = data[index];
        int n = min(maxOrder, dataSize - 1 - index);
        rArray[0] += d * d;
        for (order = 1; order <= n; order++)
            rArray[order] += d * data[index + order];
    }
}



/*****************************************************/
/* LevinsonDurbin:                                   */
/*   Standard Levinson-Durbin recursion.             */
/*****************************************************/
static faac_real LevinsonDurbin(int fOrder,
                      int dataSize,
                      const faac_real * restrict data,
                      faac_real * restrict kArray)
{
    int order,i;
    faac_real signal;
    faac_real error, kTemp;
    faac_real aArray1[TNS_MAX_ORDER+1];
    faac_real aArray2[TNS_MAX_ORDER+1];
    faac_real rArray[TNS_MAX_ORDER+1] = {0};
    faac_real* aPtr = aArray1;
    faac_real* aLastPtr = aArray2;
    faac_real* aTemp;

    Autocorrelation(fOrder, dataSize, data, rArray);
    signal = rArray[0];

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
        aPtr[0]=1.0;
        aLastPtr[0]=1.0;
        error=rArray[0];

        /* Now perform recursion */
        for (order=1;order<=fOrder;order++) {
            kTemp = aLastPtr[0]*rArray[order-0];
            for (i=1;i<order;i++) {
                kTemp += aLastPtr[i]*rArray[order-i];
            }
            if (error <= 0.0 || FAAC_FABS(kTemp) >= error) {
                error = 0.0;
                break;
            }
            kTemp = -kTemp/error;
            kArray[order]=kTemp;
            aPtr[order]=kTemp;
            for (i=1;i<order;i++) {
                aPtr[i] = aLastPtr[i] + kTemp*aLastPtr[order-i];
            }
            error = error * (1 - kTemp*kTemp);
            if (error <= 0.0) break;

            /* Now make current iteration the last one */
            aTemp=aLastPtr;
            aLastPtr=aPtr;      /* Current becomes last */
            aPtr=aTemp;         /* Last becomes current */
        }
        if (error <= 0.0) return (faac_real)(TNS_THRESH_CAP + 1.0);
        return signal/error;
    }
}


/*****************************************************/
/* StepUp:                                           */
/*   Reflection coefficients to predictor coeffs.    */
/*****************************************************/
static void StepUp(int fOrder,faac_real* kArray,faac_real* aArray)
{
    faac_real aTemp[TNS_MAX_ORDER+2];
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

/*****************************************************/
/* WhitenSpectrumForTns:                             */
/*   Per-SFB whitening via inverse-sqrt normalization. */
/*****************************************************/
static void WhitenSpectrumForTns(const faac_real * restrict spec,
                                 faac_real * restrict out,
                                 const int * restrict sfbOffsetTable,
                                 const faac_real * restrict sfbEnergy,
                                 int startBand, int stopBand,
                                 int startLine, int stopLine)
{
    faac_real invE[NSFB_LONG];
    int sfb, i;

    if (startBand >= stopBand || startLine >= stopLine)
        return;

    for (sfb = startBand; sfb < stopBand; sfb++) {
        invE[sfb] = (sfbEnergy[sfb] > (faac_real)0.0)
                  ? (faac_real)1.0 / FAAC_SQRT(sfbEnergy[sfb])
                  : (faac_real)0.0;
    }

    /* RTL smoothing. */
    {
        sfb = stopBand - 1;
        int sfb_start = sfbOffsetTable[sfb];
        faac_real w = invE[sfb];
        out[stopLine - 1] = w;
        for (i = stopLine - 2; i >= startLine; i--) {
            if (i < sfb_start) {
                sfb--;
                w = invE[sfb];
                sfb_start = sfbOffsetTable[sfb];
            }
            out[i] = (faac_real)0.5 * (w + out[i + 1]);
        }
    }

    /* LTR smoothing and application. */
    {
        faac_real prev = out[startLine];
        out[startLine] = prev * spec[startLine];
        for (i = startLine + 1; i < stopLine; i++) {
            faac_real weight = (faac_real)0.5 * (out[i] + prev);
            prev = weight;
            out[i] = weight * spec[i];
        }
    }
}
