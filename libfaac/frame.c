/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: frame.c,v 1.46 2003/09/07 16:48:31 knik Exp $
 */

/*
 * CHANGES:
 *  2001/01/17: menno: Added frequency cut off filter.
 *  2001/02/28: menno: Added Temporal Noise Shaping.
 *  2001/03/05: menno: Added Long Term Prediction.
 *  2001/05/01: menno: Added backward prediction.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "frame.h"
#include "coder.h"
#include "midside.h"
#include "channels.h"
#include "bitstream.h"
#include "filtbank.h"
#include "aacquant.h"
#include "util.h"
#include "huffman.h"
#include "psych.h"
#include "tns.h"
#include "ltp.h"
#include "backpred.h"
#include "version.h"

static char *libfaacName = FAAC_VERSION " (" __DATE__ ")";
static char *libCopyright =
  "FAAC - Freeware Advanced Audio Coder (http://www.audiocoding.com/)\n"
  " Portions Copyright (C) 2001 Menno Bakker\n"
  " Portions Copyright (C) 2002,2003 Krzysztof Nikiel\n"
  "This software is based on the ISO MPEG-4 reference source code.\n";

static const psymodellist_t psymodellist[] = {
  {&psymodel2, "knipsycho psychoacoustic"},
  {NULL}
};

static SR_INFO srInfo[12+1];
static const int bwmax = 15000;
static const double bwfac = 0.45;

int FAACAPI faacEncGetDecoderSpecificInfo(faacEncHandle hEncoder,unsigned char** ppBuffer,unsigned long* pSizeOfDecoderSpecificInfo)
{
    BitStream* pBitStream = NULL;

    if((hEncoder == NULL) || (ppBuffer == NULL) || (pSizeOfDecoderSpecificInfo == NULL)) {
        return -1;
    }

    if(hEncoder->config.mpegVersion == MPEG2){
        return -2; /* not supported */
    }

    *pSizeOfDecoderSpecificInfo = 2;
    *ppBuffer = malloc(2);

    if(*ppBuffer != NULL){

        memset(*ppBuffer,0,*pSizeOfDecoderSpecificInfo);
        pBitStream = OpenBitStream(*pSizeOfDecoderSpecificInfo, *ppBuffer);
        PutBit(pBitStream, hEncoder->config.aacObjectType + 1, 5);

        /*
        temporary fix,
        when object type defines will be changed to values defined by ISO 14496-3
        "+ 1" shall be removed

        /AV
        */

        PutBit(pBitStream, hEncoder->sampleRateIdx, 4);
        PutBit(pBitStream, hEncoder->numChannels, 4);
        CloseBitStream(pBitStream);

        return 0;
    } else {
        return -3;
    }
}


faacEncConfigurationPtr FAACAPI faacEncGetCurrentConfiguration(faacEncHandle hEncoder)
{
    faacEncConfigurationPtr config = &(hEncoder->config);

    return config;
}

int FAACAPI faacEncSetConfiguration(faacEncHandle hEncoder,
                                    faacEncConfigurationPtr config)
{
    hEncoder->config.allowMidside = config->allowMidside;
    hEncoder->config.useLfe = config->useLfe;
    hEncoder->config.useTns = config->useTns;
    hEncoder->config.aacObjectType = config->aacObjectType;
    hEncoder->config.mpegVersion = config->mpegVersion;
    hEncoder->config.outputFormat = config->outputFormat;
    hEncoder->config.inputFormat = config->inputFormat;

    assert((hEncoder->config.outputFormat == 0) || (hEncoder->config.outputFormat == 1));

    switch( hEncoder->config.inputFormat )
    {
        case FAAC_INPUT_16BIT:
        //case FAAC_INPUT_24BIT:
        case FAAC_INPUT_32BIT:
        case FAAC_INPUT_FLOAT:
            break;

        default:
            return 0;
            break;
    }

    /* No SSR supported for now */
    if (hEncoder->config.aacObjectType == SSR)
        return 0;

    /* LTP only with MPEG4 */
    if ((hEncoder->config.aacObjectType == LTP) && (hEncoder->config.mpegVersion != MPEG4))
        return 0;

    /* Re-init TNS for new profile */
    TnsInit(hEncoder);

    /* Check for correct bitrate */
    if (config->bitRate > MaxBitrate(hEncoder->sampleRate))
        return 0;
#if 0
    if (config->bitRate < MinBitrate())
        return 0;
#endif

    if (config->bitRate)
    {
      int i;
      static struct {
    int rate; // per channel at 44100 sampling frequency
        int cutoff;
      } rates[] =
      {
    {29700, 5000},
    {37700, 7000},
    {48500, 10000},
    {64000, 15000},
    {78500, 20000},
    {0, 0}
      };
      int f0, f1;
      int r0, r1;

      config->quantqual = 100;

      config->bitRate = (double)config->bitRate * 44100 / hEncoder->sampleRate;

      f0 = f1 = rates[0].cutoff;
      r0 = r1 = rates[0].rate;
      for (i = 0; rates[i].rate; i++)
      {
    f0 = f1;
    f1 = rates[i].cutoff;
    r0 = r1;
    r1 = rates[i].rate;
    if (rates[i].rate >= config->bitRate)
      break;
      }

      if (config->bitRate > r1)
        config->bitRate = r1;
      if (config->bitRate < r0)
    config->bitRate = r0;

      if (f1 > f0)
    config->bandWidth =
      pow((double)config->bitRate / r1,
          log((double)f1 / f0) / log ((double)r1 / r0)) * (double)f1;
      else
    config->bandWidth = f1;

      config->bandWidth =
    (double)config->bandWidth * hEncoder->sampleRate / 44100;
      config->bitRate = (double)config->bitRate * hEncoder->sampleRate / 44100;
    }

    hEncoder->config.bitRate = config->bitRate;

    if (!config->bandWidth)
    {
      config->bandWidth = bwfac * hEncoder->sampleRate;
      if (config->bandWidth > bwmax)
    config->bandWidth = bwmax;
    }

    hEncoder->config.bandWidth = config->bandWidth;

    // check bandwidth
    if (hEncoder->config.bandWidth < 100)
      hEncoder->config.bandWidth = 100;
    if (hEncoder->config.bandWidth > (hEncoder->sampleRate / 2))
      hEncoder->config.bandWidth = hEncoder->sampleRate / 2;

    if (config->quantqual > 500)
      config->quantqual = 500;
    if (config->quantqual < 10)
      config->quantqual = 10;
    hEncoder->config.quantqual = config->quantqual;

    // reset psymodel
    hEncoder->psymodel->PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);
    if (config->psymodelidx >= (sizeof(psymodellist) / sizeof(psymodellist[0]) - 1))
      config->psymodelidx = (sizeof(psymodellist) / sizeof(psymodellist[0])) - 2;
    hEncoder->config.psymodelidx = config->psymodelidx;
    hEncoder->psymodel = psymodellist[hEncoder->config.psymodelidx].model;
    hEncoder->psymodel->PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
          hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
          hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
          hEncoder->srInfo->num_cb_short);

    /* OK */
    return 1;
}

faacEncHandle FAACAPI faacEncOpen(unsigned long sampleRate,
                                  unsigned int numChannels,
                                  unsigned long *inputSamples,
                                  unsigned long *maxOutputBytes)
{
    unsigned int channel;
    faacEncHandle hEncoder;

    *inputSamples = 1024*numChannels;
    *maxOutputBytes = (6144/8)*numChannels;

    hEncoder = (faacEncStruct*)AllocMemory(sizeof(faacEncStruct));
    SetMemory(hEncoder, 0, sizeof(faacEncStruct));

    hEncoder->numChannels = numChannels;
    hEncoder->sampleRate = sampleRate;
    hEncoder->sampleRateIdx = GetSRIndex(sampleRate);

    /* Initialize variables to default values */
    hEncoder->frameNum = 0;
    hEncoder->flushFrame = 0;

    /* Default configuration */
    hEncoder->config.version = FAAC_CFG_VERSION;
    hEncoder->config.name = libfaacName;
    hEncoder->config.copyright = libCopyright;
    hEncoder->config.mpegVersion = MPEG4;
    hEncoder->config.aacObjectType = LTP;
    hEncoder->config.allowMidside = 1;
    hEncoder->config.useLfe = 1;
    hEncoder->config.useTns = 0;
    hEncoder->config.bitRate = 0; /* default bitrate / channel */
    hEncoder->config.bandWidth = bwfac * hEncoder->sampleRate;
    if (hEncoder->config.bandWidth > bwmax)
      hEncoder->config.bandWidth = bwmax;
    hEncoder->config.quantqual = 100;
    hEncoder->config.psymodellist = psymodellist;
    hEncoder->config.psymodelidx = 0;
    hEncoder->psymodel =
      hEncoder->config.psymodellist[hEncoder->config.psymodelidx].model;

    /*
        by default we have to be compatible with all previous software
        which assumes that we will generate ADTS
        /AV
    */
    hEncoder->config.outputFormat = 1;

    /*
        be compatible with software which assumes 24bit in 32bit PCM
    */
    hEncoder->config.inputFormat = FAAC_INPUT_32BIT;

    /* find correct sampling rate depending parameters */
    hEncoder->srInfo = &srInfo[hEncoder->sampleRateIdx];

    for (channel = 0; channel < numChannels; channel++) {
        hEncoder->coderInfo[channel].prev_window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].block_type = ONLY_LONG_WINDOW;
        hEncoder->coderInfo[channel].num_window_groups = 1;
        hEncoder->coderInfo[channel].window_group_length[0] = 1;

        /* FIXME: Use sr_idx here */
        hEncoder->coderInfo[channel].max_pred_sfb = GetMaxPredSfb(hEncoder->sampleRateIdx);

        hEncoder->sampleBuff[channel] = NULL;
        hEncoder->nextSampleBuff[channel] = NULL;
        hEncoder->next2SampleBuff[channel] = NULL;
        hEncoder->ltpTimeBuff[channel] = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));
        SetMemory(hEncoder->ltpTimeBuff[channel], 0, 2*BLOCK_LEN_LONG*sizeof(double));
    }

    /* Initialize coder functions */
    hEncoder->psymodel->PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
        hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
        hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short);

    FilterBankInit(hEncoder);

    TnsInit(hEncoder);

    LtpInit(hEncoder);

    PredInit(hEncoder);

    AACQuantizeInit(hEncoder, hEncoder->coderInfo, hEncoder->numChannels);

    HuffmanInit(hEncoder->coderInfo, hEncoder->numChannels);

    /* Return handle */
    return hEncoder;
}

int FAACAPI faacEncClose(faacEncHandle hEncoder)
{
    unsigned int channel;

    /* Deinitialize coder functions */
    hEncoder->psymodel->PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);

    FilterBankEnd(hEncoder);

    LtpEnd(hEncoder);

    AACQuantizeEnd(hEncoder, hEncoder->coderInfo, hEncoder->numChannels);

    HuffmanEnd(hEncoder->coderInfo, hEncoder->numChannels);

    /* Free remaining buffer memory */
    for (channel = 0; channel < hEncoder->numChannels; channel++) {
      if (hEncoder->ltpTimeBuff[channel])
    FreeMemory(hEncoder->ltpTimeBuff[channel]);
      if (hEncoder->sampleBuff[channel])
    FreeMemory(hEncoder->sampleBuff[channel]);
      if (hEncoder->nextSampleBuff[channel])
    FreeMemory(hEncoder->nextSampleBuff[channel]);
      if (hEncoder->next2SampleBuff[channel])
    FreeMemory (hEncoder->next2SampleBuff[channel]);
      if (hEncoder->next3SampleBuff[channel])
    FreeMemory (hEncoder->next3SampleBuff[channel]);
    }

    /* Free handle */
    if (hEncoder) FreeMemory(hEncoder);

    return 0;
}

int FAACAPI faacEncEncode(faacEncHandle hEncoder,
                          int32_t *inputBuffer,
                          unsigned int samplesInput,
                          unsigned char *outputBuffer,
                          unsigned int bufferSize
                          )
{
    unsigned int channel, i;
    int sb, frameBytes;
    unsigned int bitsToUse, offset;
    BitStream *bitStream; /* bitstream used for writing the frame to */
    TnsInfo *tnsInfo_for_LTP;
    TnsInfo *tnsDecInfo;

    /* local copy's of parameters */
    ChannelInfo *channelInfo = hEncoder->channelInfo;
    CoderInfo *coderInfo = hEncoder->coderInfo;
    unsigned int numChannels = hEncoder->numChannels;
    unsigned int sampleRate = hEncoder->sampleRate;
    unsigned int aacObjectType = hEncoder->config.aacObjectType;
    unsigned int mpegVersion = hEncoder->config.mpegVersion;
    unsigned int useLfe = hEncoder->config.useLfe;
    unsigned int useTns = hEncoder->config.useTns;
    unsigned int allowMidside = hEncoder->config.allowMidside;
    unsigned int bitRate = hEncoder->config.bitRate;
    unsigned int bandWidth = hEncoder->config.bandWidth;
    unsigned int quantqual = hEncoder->config.quantqual;

    /* Increase frame number */
    hEncoder->frameNum++;

    if (samplesInput == 0)
        hEncoder->flushFrame++;

    /* After 4 flush frames all samples have been encoded,
       return 0 bytes written */
    if (hEncoder->flushFrame > 4)
        return 0;

    /* Determine the channel configuration */
    GetChannelInfo(channelInfo, numChannels, useLfe);

    /* Update current sample buffers */
    for (channel = 0; channel < numChannels; channel++) {
    double *tmp;

        if (hEncoder->sampleBuff[channel]) {
            for(i = 0; i < FRAME_LEN; i++) {
                hEncoder->ltpTimeBuff[channel][i] = hEncoder->sampleBuff[channel][i];
            }
        }
        if (hEncoder->nextSampleBuff[channel]) {
            for(i = 0; i < FRAME_LEN; i++) {
                hEncoder->ltpTimeBuff[channel][FRAME_LEN + i] =
                    hEncoder->nextSampleBuff[channel][i];
            }
        }

    if (!hEncoder->sampleBuff[channel])
      hEncoder->sampleBuff[channel] = (double*)AllocMemory(FRAME_LEN*sizeof(double));
    tmp = hEncoder->sampleBuff[channel];
        hEncoder->sampleBuff[channel] = hEncoder->nextSampleBuff[channel];
        hEncoder->nextSampleBuff[channel] = hEncoder->next2SampleBuff[channel];
        hEncoder->next2SampleBuff[channel] = hEncoder->next3SampleBuff[channel];
    hEncoder->next3SampleBuff[channel] = tmp;

        if (samplesInput == 0)
        {
            /* start flushing*/
            for (i = 0; i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
        }
        else
        {
            /* handle the various input formats */
            switch( hEncoder->config.inputFormat )
            {
                case FAAC_INPUT_16BIT:
                    for (i = 0; i < (int)(samplesInput/numChannels); i++)
                    {
                        hEncoder->next3SampleBuff[channel][i] =
                            (double)((short*)inputBuffer)[(i*numChannels)+channel];
                    }
                    break;

                case FAAC_INPUT_32BIT:
                    for (i = 0; i < (int)(samplesInput/numChannels); i++)
                    {
                        hEncoder->next3SampleBuff[channel][i] =
                            (1.0/256) *  (double)inputBuffer[(i*numChannels)+channel];
                    }
                    break;

                case FAAC_INPUT_FLOAT:
                    for (i = 0; i < (int)(samplesInput/numChannels); i++)
                    {
                        hEncoder->next3SampleBuff[channel][i] =
                            ((float*)inputBuffer)[(i*numChannels)+channel];
                    }
                    break;

                default:
                    return -1; /* invalid input format */
                    break;
            }

            for (i = (int)(samplesInput/numChannels); i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
        }

        /* Psychoacoustics */
    /* Update buffers and run FFT on new samples */
    /* LFE psychoacoustic can run without it */
    if (!channelInfo[channel].lfe || channelInfo[channel].cpe)
    {
      hEncoder->psymodel->PsyBufferUpdate(&hEncoder->gpsyInfo, &hEncoder->psyInfo[channel],
					      hEncoder->next3SampleBuff[channel], bandWidth,
					      hEncoder->srInfo->cb_width_short,
					      hEncoder->srInfo->num_cb_short);
    }
    }

    if (hEncoder->frameNum <= 3) /* Still filling up the buffers */
        return 0;

    /* Psychoacoustics */
    hEncoder->psymodel->PsyCalculate(channelInfo, &hEncoder->gpsyInfo, hEncoder->psyInfo,
        hEncoder->srInfo->cb_width_long, hEncoder->srInfo->num_cb_long,
        hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short, numChannels);

    hEncoder->psymodel->BlockSwitch(coderInfo, hEncoder->psyInfo, numChannels);

    /* AAC Filterbank, MDCT with overlap and add */
    for (channel = 0; channel < numChannels; channel++) {
        int k;

        FilterBank(hEncoder,
            &coderInfo[channel],
            hEncoder->sampleBuff[channel],
            hEncoder->freqBuff[channel],
            hEncoder->overlapBuff[channel],
            MOVERLAPPED);

        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            for (k = 0; k < 8; k++) {
                specFilter(hEncoder->freqBuff[channel]+k*BLOCK_LEN_SHORT,
                    sampleRate, bandWidth, BLOCK_LEN_SHORT);
            }
        } else {
            specFilter(hEncoder->freqBuff[channel], sampleRate,
                bandWidth, BLOCK_LEN_LONG);
        }
    }

    /* TMP: Build sfb offset table and other stuff */
    for (channel = 0; channel < numChannels; channel++) {
        channelInfo[channel].msInfo.is_present = 0;

        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            coderInfo[channel].max_sfb = hEncoder->srInfo->num_cb_short;
            coderInfo[channel].nr_of_sfb = hEncoder->srInfo->num_cb_short;

            coderInfo[channel].num_window_groups = 1;
            coderInfo[channel].window_group_length[0] = 8;
            coderInfo[channel].window_group_length[1] = 0;
            coderInfo[channel].window_group_length[2] = 0;
            coderInfo[channel].window_group_length[3] = 0;
            coderInfo[channel].window_group_length[4] = 0;
            coderInfo[channel].window_group_length[5] = 0;
            coderInfo[channel].window_group_length[6] = 0;
            coderInfo[channel].window_group_length[7] = 0;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].nr_of_sfb; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_short[sb];
            }
            coderInfo[channel].sfb_offset[coderInfo[channel].nr_of_sfb] = offset;
        } else {
            coderInfo[channel].max_sfb = hEncoder->srInfo->num_cb_long;
            coderInfo[channel].nr_of_sfb = hEncoder->srInfo->num_cb_long;

            coderInfo[channel].num_window_groups = 1;
            coderInfo[channel].window_group_length[0] = 1;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].nr_of_sfb; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_long[sb];
            }
            coderInfo[channel].sfb_offset[coderInfo[channel].nr_of_sfb] = offset;
        }
    }

    /* Perform TNS analysis and filtering */
    for (channel = 0; channel < numChannels; channel++) {
        if ((!channelInfo[channel].lfe) && (useTns)) {
            TnsEncode(&(coderInfo[channel].tnsInfo),
                coderInfo[channel].max_sfb,
                coderInfo[channel].max_sfb,
                coderInfo[channel].block_type,
                coderInfo[channel].sfb_offset,
                hEncoder->freqBuff[channel]);
        } else {
            coderInfo[channel].tnsInfo.tnsDataPresent = 0;      /* TNS not used for LFE */
        }
    }

    for(channel = 0; channel < numChannels; channel++)
    {
        if((coderInfo[channel].tnsInfo.tnsDataPresent != 0) && (useTns))
            tnsInfo_for_LTP = &(coderInfo[channel].tnsInfo);
        else
            tnsInfo_for_LTP = NULL;

        if(channelInfo[channel].present && (!channelInfo[channel].lfe) &&
            (coderInfo[channel].block_type != ONLY_SHORT_WINDOW) &&
            (mpegVersion == MPEG4) && (aacObjectType == LTP))
        {
            LtpEncode(hEncoder,
                &coderInfo[channel],
                &(coderInfo[channel].ltpInfo),
                tnsInfo_for_LTP,
                hEncoder->freqBuff[channel],
                hEncoder->ltpTimeBuff[channel]);
        } else {
            coderInfo[channel].ltpInfo.global_pred_flag = 0;
        }
    }

    for(channel = 0; channel < numChannels; channel++)
    {
        if ((aacObjectType == MAIN) && (!channelInfo[channel].lfe)) {
            int numPredBands = min(coderInfo[channel].max_pred_sfb, coderInfo[channel].nr_of_sfb);
            PredCalcPrediction(hEncoder->freqBuff[channel],
                coderInfo[channel].requantFreq,
                coderInfo[channel].block_type,
                numPredBands,
                (coderInfo[channel].block_type==ONLY_SHORT_WINDOW)?
                hEncoder->srInfo->cb_width_short:hEncoder->srInfo->cb_width_long,
                coderInfo,
                channelInfo,
                channel);
        } else {
            coderInfo[channel].pred_global_flag = 0;
        }
    }

    for (channel = 0; channel < numChannels; channel++) {
      if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
    SortForGrouping(&coderInfo[channel],
            &hEncoder->psyInfo[channel],
            &channelInfo[channel],
            hEncoder->srInfo->cb_width_short,
            hEncoder->freqBuff[channel]);
      }
      CalcAvgEnrg(&coderInfo[channel], hEncoder->freqBuff[channel]);

      // reduce LFE bandwidth
      if (!channelInfo[channel].cpe && channelInfo[channel].lfe)
      {
    coderInfo[channel].nr_of_sfb = coderInfo[channel].max_sfb = 3;
      }
    }

    MSEncode(coderInfo, channelInfo, hEncoder->freqBuff, numChannels, allowMidside);

    /* Quantize and code the signal */
    if (quantqual)
      bitsToUse = quantqual;
    else
    bitsToUse = (int)(bitRate*FRAME_LEN/sampleRate+0.5);
    for (channel = 0; channel < numChannels; channel++) {
        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            AACQuantize(hEncoder, &coderInfo[channel], &hEncoder->psyInfo[channel],
                &channelInfo[channel], hEncoder->srInfo->cb_width_short,
                hEncoder->srInfo->num_cb_short, hEncoder->freqBuff[channel], bitsToUse);
        } else {
            AACQuantize(hEncoder, &coderInfo[channel], &hEncoder->psyInfo[channel],
                &channelInfo[channel], hEncoder->srInfo->cb_width_long,
                hEncoder->srInfo->num_cb_long, hEncoder->freqBuff[channel], bitsToUse);
        }
    }

    // fix max_sfb in CPE mode
    for (channel = 0; channel < numChannels; channel++)
    {
    if (channelInfo[channel].present
        && (channelInfo[channel].cpe)
        && (channelInfo[channel].ch_is_left))
    {
      CoderInfo *cil, *cir;

      cil = &coderInfo[channel];
      cir = &coderInfo[channelInfo[channel].paired_ch];

      cil->max_sfb = cir->max_sfb = max(cil->max_sfb, cir->max_sfb);
          cil->nr_of_sfb = cir->nr_of_sfb = cil->max_sfb;
    }
    }

    MSReconstruct(coderInfo, channelInfo, numChannels);

    for (channel = 0; channel < numChannels; channel++)
    {
        /* If short window, reconstruction not needed for prediction */
        if ((coderInfo[channel].block_type == ONLY_SHORT_WINDOW)) {
            int sind;
            for (sind = 0; sind < 1024; sind++) {
                coderInfo[channel].requantFreq[sind] = 0.0;
            }
        } else {

            if((coderInfo[channel].tnsInfo.tnsDataPresent != 0) && (useTns))
                tnsDecInfo = &(coderInfo[channel].tnsInfo);
            else
                tnsDecInfo = NULL;

            if ((!channelInfo[channel].lfe) && (aacObjectType == LTP)) {  /* no reconstruction needed for LFE channel*/

                LtpReconstruct(&coderInfo[channel], &(coderInfo[channel].ltpInfo),
                    coderInfo[channel].requantFreq);

                if(tnsDecInfo != NULL)
                    TnsDecodeFilterOnly(&(coderInfo[channel].tnsInfo), coderInfo[channel].nr_of_sfb,
                    coderInfo[channel].max_sfb, coderInfo[channel].block_type,
                    coderInfo[channel].sfb_offset, coderInfo[channel].requantFreq);

                IFilterBank(hEncoder, &coderInfo[channel],
                    coderInfo[channel].requantFreq,
                    coderInfo[channel].ltpInfo.time_buffer,
                    coderInfo[channel].ltpInfo.ltp_overlap_buffer,
                    MOVERLAPPED);

                LtpUpdate(&(coderInfo[channel].ltpInfo),
                    coderInfo[channel].ltpInfo.time_buffer,
                    coderInfo[channel].ltpInfo.ltp_overlap_buffer,
                    BLOCK_LEN_LONG);
            }
        }
    }

    /* Write the AAC bitstream */
    bitStream = OpenBitStream(bufferSize, outputBuffer);

    WriteBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannels);

    /* Close the bitstream and return the number of bytes written */
    frameBytes = CloseBitStream(bitStream);

#ifdef _DEBUG
    printf("%4d %4d\n", hEncoder->frameNum-3, frameBytes);
#endif

    return frameBytes;
}


/* Scalefactorband data table */
static SR_INFO srInfo[12+1] =
{
    { 96000, 41, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28,
            36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 36
        }
    }, { 88200, 41, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28,
            36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 36
        }
    }, { 64000, 47, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 12, 12, 12, 16, 16, 16, 20, 24, 24, 28,
            36, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
            40, 40, 40, 40, 40
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 32
        }
    }, { 48000, 49, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
        }, {
            4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16
        }
    }, { 44100, 49, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 96
        }, {
            4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16
        }
    }, { 32000, 51, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28,
            28, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32
        },{
            4,  4,  4,  4,  4,  8,  8,  8,  12, 12, 12, 16, 16, 16
        }
    }, { 24000, 47, 15,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
            36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
        }, {
            4,  4,  4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 20
        }
    }, { 22050, 47, 15,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
            36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 64
        }, {
            4,  4,  4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 20
        }
    }, { 16000, 43, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
        }
    }, { 12000, 43, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
        }
    }, { 11025, 43, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 64
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 20
        }
    }, { 8000, 40, 15,
        {
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 16,
            16, 16, 16, 16, 16, 16, 20, 20, 20, 20, 24, 24, 24, 28,
            28, 32, 36, 36, 40, 44, 48, 52, 56, 60, 64, 80
        }, {
            4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 12, 16, 20, 20
        }
    },
    { -1 }
};

/*
$Log: frame.c,v $
Revision 1.46  2003/09/07 16:48:31  knik
Updated psymodel call. Updated bitrate/cutoff mapping table.

Revision 1.45  2003/08/23 15:02:13  knik
last frame moved back to the library

Revision 1.44  2003/08/15 11:42:08  knik
removed single silent flush frame

Revision 1.43  2003/08/11 09:43:47  menno
thread safety, some tables added to the encoder context

Revision 1.42  2003/08/09 11:39:30  knik
LFE support enabled by default

Revision 1.41  2003/08/08 10:02:09  menno
Small fix

Revision 1.40  2003/08/07 08:17:00  knik
Better LFE support (reduced bandwidth)

Revision 1.39  2003/08/02 11:32:10  stux
added config.inputFormat, and associated defines and code, faac now handles native endian 16bit, 24bit and float input. Added faacEncGetDecoderSpecificInfo to the dll exports, needed for MP4. Updated DLL .dsp to compile without error. Updated CFG_VERSION to 102. Version number might need to be updated as the API has technically changed. Did not update libfaac.pdf

Revision 1.38  2003/07/10 19:17:01  knik
24-bit input

Revision 1.37  2003/06/26 19:20:09  knik
Mid/Side support.
Copyright info moved from frontend.
Fixed memory leak.

Revision 1.36  2003/05/12 17:53:16  knik
updated ABR table

Revision 1.35  2003/05/10 09:39:55  knik
added approximate ABR setting
modified default cutoff

Revision 1.34  2003/05/01 09:31:39  knik
removed ISO psyodel
disabled m/s coding
fixed default bandwidth
reduced max_sfb check

Revision 1.33  2003/04/13 08:37:23  knik
version number moved to version.h

Revision 1.32  2003/03/27 17:08:23  knik
added quantizer quality and bandwidth setting

Revision 1.31  2002/10/11 18:00:15  menno
small bugfix

Revision 1.30  2002/10/08 18:53:01  menno
Fixed some memory leakage

Revision 1.29  2002/08/19 16:34:43  knik
added one additional flush frame
fixed sample buffer memory allocation

*/
