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
 * $Id: frame.c,v 1.26 2001/09/21 12:40:02 eraser Exp $
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

#include "frame.h"
#include "coder.h"
#include "joint.h"
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
    hEncoder->config.bandWidth = config->bandWidth;
	hEncoder->config.outputFormat = config->outputFormat;

	assert((hEncoder->config.outputFormat == 0) || (hEncoder->config.outputFormat == 1));

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
    if (config->bitRate < MinBitrate())
        return 0;

    /* Bitrate check passed */
    hEncoder->config.bitRate = config->bitRate;

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
    hEncoder->config.mpegVersion = MPEG4;
    hEncoder->config.aacObjectType = LTP;
    hEncoder->config.allowMidside = 1;
    hEncoder->config.useLfe = 0;
    hEncoder->config.useTns = 0;
    hEncoder->config.bitRate = 64000; /* default bitrate / channel */
    hEncoder->config.bandWidth = 18000; /* default bandwidth */

	/*
		by default we have to be compatible with all previous software
		which assumes that we will generate ADTS
		/AV
	*/
	hEncoder->config.outputFormat = 1;

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
    PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
        hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
        hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short); 

    FilterBankInit(hEncoder);

    TnsInit(hEncoder);

    LtpInit(hEncoder);

    PredInit(hEncoder);

    AACQuantizeInit(hEncoder->coderInfo, hEncoder->numChannels);

    HuffmanInit(hEncoder->coderInfo, hEncoder->numChannels);

    /* Return handle */
    return hEncoder;
}

int FAACAPI faacEncClose(faacEncHandle hEncoder)
{
    unsigned int channel;

    /* Deinitialize coder functions */
    PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);

    FilterBankEnd(hEncoder);

    LtpEnd(hEncoder);

    AACQuantizeEnd(hEncoder->coderInfo, hEncoder->numChannels);

    HuffmanEnd(hEncoder->coderInfo, hEncoder->numChannels);

    /* Free remaining buffer memory */
    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        if (hEncoder->ltpTimeBuff[channel]) FreeMemory(hEncoder->ltpTimeBuff[channel]);
        if (hEncoder->sampleBuff[channel]) FreeMemory(hEncoder->sampleBuff[channel]);
        if (hEncoder->nextSampleBuff[channel]) FreeMemory(hEncoder->nextSampleBuff[channel]);
    }

    /* Free handle */
    if (hEncoder) FreeMemory(hEncoder);

    return 0;
}

int FAACAPI faacEncEncode(faacEncHandle hEncoder,
                          short *inputBuffer,
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

    /* Increase frame number */
    hEncoder->frameNum++;

    if (samplesInput == 0)
        hEncoder->flushFrame++;

    /* After 4 flush frames all samples have been encoded,
       return 0 bytes written */
    if (hEncoder->flushFrame == 4)
        return 0;

    /* Determine the channel configuration */
    GetChannelInfo(channelInfo, numChannels, useLfe);

    /* Update current sample buffers */
    for (channel = 0; channel < numChannels; channel++) {
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

        if (hEncoder->sampleBuff[channel])
            FreeMemory(hEncoder->sampleBuff[channel]);
        hEncoder->sampleBuff[channel] = hEncoder->nextSampleBuff[channel];
        hEncoder->nextSampleBuff[channel] = hEncoder->next2SampleBuff[channel];
        hEncoder->next2SampleBuff[channel] = hEncoder->next3SampleBuff[channel];
        hEncoder->next3SampleBuff[channel] = (double*)AllocMemory(FRAME_LEN*sizeof(double));

        if (samplesInput == 0) { /* start flushing*/
            for (i = 0; i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
        } else {
            for (i = 0; i < (int)(samplesInput/numChannels); i++)
                hEncoder->next3SampleBuff[channel][i] =
                    (double)inputBuffer[(i*numChannels)+channel];
            for (i = (int)(samplesInput/numChannels); i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
        }

        /* Psychoacoustics */
        /* Update buffers and run FFT on new samples */
        PsyBufferUpdate(&hEncoder->gpsyInfo, &hEncoder->psyInfo[channel],
            hEncoder->next3SampleBuff[channel]);
    }

    if (hEncoder->frameNum <= 3) /* Still filling up the buffers */
        return 0;

    /* Psychoacoustics */
    PsyCalculate(channelInfo, &hEncoder->gpsyInfo, hEncoder->psyInfo,
        hEncoder->srInfo->cb_width_long, hEncoder->srInfo->num_cb_long,
        hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short, numChannels);

    BlockSwitch(coderInfo, hEncoder->psyInfo, numChannels);

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

    MSEncode(coderInfo, channelInfo, hEncoder->freqBuff, numChannels, allowMidside);

    /* Quantize and code the signal */
    bitsToUse = (int)(bitRate*FRAME_LEN/sampleRate+0.5);
    for (channel = 0; channel < numChannels; channel++) {
        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            AACQuantize(&coderInfo[channel], &hEncoder->psyInfo[channel],
                &channelInfo[channel], hEncoder->srInfo->cb_width_short,
                hEncoder->srInfo->num_cb_short, hEncoder->freqBuff[channel], bitsToUse);
        } else {
            AACQuantize(&coderInfo[channel], &hEncoder->psyInfo[channel],
                &channelInfo[channel], hEncoder->srInfo->cb_width_long,
                hEncoder->srInfo->num_cb_long, hEncoder->freqBuff[channel], bitsToUse);
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
