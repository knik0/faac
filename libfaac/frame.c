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
#include "tns.h"
#include "version.h"

#if FAAC_RELEASE
static char *libfaacName = FAAC_VERSION;
#else
static char *libfaacName = FAAC_VERSION ".1 (" __DATE__ ") UNSTABLE";
#endif
static char *libCopyright =
  "FAAC - Freeware Advanced Audio Coder (http://www.audiocoding.com/)\n"
  " Copyright (C) 1999,2000,2001  Menno Bakker\n"
  " Copyright (C) 2002,2003  Krzysztof Nikiel\n"
  "This software is based on the ISO MPEG-4 reference source code.\n";

static const psymodellist_t psymodellist[] = {
  {&psymodel2, "knipsycho psychoacoustic"},
  {NULL}
};

static SR_INFO srInfo[12+1];

// default bandwidth/samplerate ratio
static const double bwfac = 0.42;

int FAACAPI faacEncGetVersion( char **faac_id_string,
			      				char **faac_copyright_string)
{
  if (faac_id_string)
    *faac_id_string = libfaacName;

  if (faac_copyright_string)
    *faac_copyright_string = libCopyright;

  return FAAC_CFG_VERSION;
}


int FAACAPI faacEncGetDecoderSpecificInfo(faacEncHandle hpEncoder,unsigned char** ppBuffer,unsigned long* pSizeOfDecoderSpecificInfo)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
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
        PutBit(pBitStream, hEncoder->config.aacObjectType, 5);
        PutBit(pBitStream, hEncoder->sampleRateIdx, 4);
        PutBit(pBitStream, hEncoder->numChannels, 4);
        CloseBitStream(pBitStream);

        return 0;
    } else {
        return -3;
    }
}


faacEncConfigurationPtr FAACAPI faacEncGetCurrentConfiguration(faacEncHandle hpEncoder)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    faacEncConfigurationPtr config = &(hEncoder->config);

    return config;
}

int FAACAPI faacEncSetConfiguration(faacEncHandle hpEncoder,
                                    faacEncConfigurationPtr config)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    int i;
    int maxqual = hEncoder->config.outputFormat ? MAXQUALADTS : MAXQUAL;

    hEncoder->config.allowMidside = config->allowMidside;
    hEncoder->config.useLfe = config->useLfe;
    hEncoder->config.useTns = config->useTns;
    hEncoder->config.aacObjectType = config->aacObjectType;
    hEncoder->config.mpegVersion = config->mpegVersion;
    hEncoder->config.outputFormat = config->outputFormat;
    hEncoder->config.inputFormat = config->inputFormat;
    hEncoder->config.shortctl = config->shortctl;

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

    if (hEncoder->config.aacObjectType != LOW)
        return 0;

    /* Re-init TNS for new profile */
    TnsInit(hEncoder);

    /* Check for correct bitrate */
    if (config->bitRate > MaxBitrate(hEncoder->sampleRate))
        config->bitRate = MaxBitrate(hEncoder->sampleRate);
#if 0
    if (config->bitRate < MinBitrate())
        return 0;
#endif

    if (config->bitRate && !config->bandWidth)
    {
		static struct {
			int rate; // per channel at 44100 sampling frequency
			int cutoff;
		}	rates[] = {
#ifdef DRM
            /* DRM uses low bit-rates. We've chosen higher bandwidth values and
               decrease the quantizer quality at the same time to preserve the
               low bit-rate */
            {4500,  1200},
            {9180,  2500},
            {11640, 3000},
            {14500, 4000},
            {17460, 5500},
            {20960, 6250},
            {40000, 12000},
#else
			{29500, 5000},
			{37500, 7000},
			{47000, 10000},
			{64000, 16000},
			{76000, 20000},
			{128000, 20000},
#endif
			{0, 0}
		};

		int f0, f1;
		int r0, r1;

#ifdef DRM
        double tmpbitRate = (double)config->bitRate;
#else
        double tmpbitRate = (double)config->bitRate * 44100 / hEncoder->sampleRate;
#endif

        config->quantqual = 100;

		f0 = f1 = rates[0].cutoff;
		r0 = r1 = rates[0].rate;
		
		for (i = 0; rates[i].rate; i++)
		{
			f0 = f1;
			f1 = rates[i].cutoff;
			r0 = r1;
			r1 = rates[i].rate;
			if (rates[i].rate >= tmpbitRate)
				break;
		}

        if (tmpbitRate > r1)
            tmpbitRate = r1;
        if (tmpbitRate < r0)
            tmpbitRate = r0;

		if (f1 > f0)
            config->bandWidth =
                    pow((double)tmpbitRate / r1,
                    log((double)f1 / f0) / log ((double)r1 / r0)) * (double)f1;
		else
			config->bandWidth = f1;

#ifndef DRM
        config->bandWidth = (double)config->bitRate * hEncoder->sampleRate * bwfac / 60000.0;
#endif
    }

    hEncoder->config.bitRate = config->bitRate;

    if (!config->bandWidth)
    {
        config->bandWidth = bwfac * hEncoder->sampleRate;
    }

    hEncoder->config.bandWidth = config->bandWidth;

    // check bandwidth
    if (hEncoder->config.bandWidth < 100)
		hEncoder->config.bandWidth = 100;
    if (hEncoder->config.bandWidth > (hEncoder->sampleRate / 2))
		hEncoder->config.bandWidth = hEncoder->sampleRate / 2;

    if (config->quantqual > maxqual)
        config->quantqual = maxqual;
    if (config->quantqual < MINQUAL)
        config->quantqual = MINQUAL;

    hEncoder->config.quantqual = config->quantqual;

    /* set quantization quality */
    hEncoder->aacquantCfg.quality = config->quantqual;
    BandLimit(&hEncoder->config.bandWidth,
              hEncoder->sampleRate,
              hEncoder->srInfo,
              &hEncoder->aacquantCfg);

    // reset psymodel
    hEncoder->psymodel->PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);
    if (config->psymodelidx >= (sizeof(psymodellist) / sizeof(psymodellist[0]) - 1))
		config->psymodelidx = (sizeof(psymodellist) / sizeof(psymodellist[0])) - 2;

    hEncoder->config.psymodelidx = config->psymodelidx;
    hEncoder->psymodel = (psymodel_t *)psymodellist[hEncoder->config.psymodelidx].ptr;
    hEncoder->psymodel->PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
			hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
			hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
			hEncoder->srInfo->num_cb_short);
	
	/* load channel_map */
	for( i = 0; i < MAX_CHANNELS; i++ )
		hEncoder->config.channel_map[i] = config->channel_map[i];

    /* OK */
    return 1;
}

faacEncHandle FAACAPI faacEncOpen(unsigned long sampleRate,
                                  unsigned int numChannels,
                                  unsigned long *inputSamples,
                                  unsigned long *maxOutputBytes)
{
    unsigned int channel;
    faacEncStruct* hEncoder;

    if (numChannels > MAX_CHANNELS)
	return NULL;

    *inputSamples = FRAME_LEN*numChannels;
    *maxOutputBytes = (6144/8)*numChannels;

#ifdef DRM
    *maxOutputBytes += 1; /* for CRC */
#endif

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
    hEncoder->config.aacObjectType = LOW;
    hEncoder->config.allowMidside = 1;
    hEncoder->config.useLfe = 1;
    hEncoder->config.useTns = 0;
    hEncoder->config.bitRate = 0; /* default bitrate / channel */
    hEncoder->config.bandWidth = bwfac * hEncoder->sampleRate;
    hEncoder->config.quantqual = DEFQUAL;
    hEncoder->config.psymodellist = (psymodellist_t *)psymodellist;
    hEncoder->config.psymodelidx = 0;
    hEncoder->psymodel =
      (psymodel_t *)hEncoder->config.psymodellist[hEncoder->config.psymodelidx].ptr;
    hEncoder->config.shortctl = SHORTCTL_NORMAL;

	/* default channel map is straight-through */
	for( channel = 0; channel < MAX_CHANNELS; channel++ )
		hEncoder->config.channel_map[channel] = channel;
	
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

    for (channel = 0; channel < numChannels; channel++) 
	{
        hEncoder->coderInfo[channel].prev_window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].window_shape = SINE_WINDOW;
        hEncoder->coderInfo[channel].block_type = ONLY_LONG_WINDOW;
        hEncoder->coderInfo[channel].num_window_groups = 1;
        hEncoder->coderInfo[channel].window_group_length[0] = 1;

        hEncoder->sampleBuff[channel] = NULL;
        hEncoder->nextSampleBuff[channel] = NULL;
        hEncoder->next2SampleBuff[channel] = NULL;
    }

    /* Initialize coder functions */
	fft_initialize( &hEncoder->fft_tables );
    
	hEncoder->psymodel->PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
        hEncoder->sampleRate, hEncoder->srInfo->cb_width_long,
        hEncoder->srInfo->num_cb_long, hEncoder->srInfo->cb_width_short,
        hEncoder->srInfo->num_cb_short);

    FilterBankInit(hEncoder);

    TnsInit(hEncoder);

    AACQuantizeInit(hEncoder->coderInfo, hEncoder->numChannels,
		    &(hEncoder->aacquantCfg));

	

    HuffmanInit(hEncoder->coderInfo, hEncoder->numChannels);

    /* Return handle */
    return hEncoder;
}

int FAACAPI faacEncClose(faacEncHandle hpEncoder)
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel;

    /* Deinitialize coder functions */
    hEncoder->psymodel->PsyEnd(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels);

    FilterBankEnd(hEncoder);

    AACQuantizeEnd(hEncoder->coderInfo, hEncoder->numChannels,
			&(hEncoder->aacquantCfg));

    HuffmanEnd(hEncoder->coderInfo, hEncoder->numChannels);

	fft_terminate( &hEncoder->fft_tables );

    /* Free remaining buffer memory */
    for (channel = 0; channel < hEncoder->numChannels; channel++) 
	{
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
    if (hEncoder) 
		FreeMemory(hEncoder);

    return 0;
}

int FAACAPI faacEncEncode(faacEncHandle hpEncoder,
                          int32_t *inputBuffer,
                          unsigned int samplesInput,
                          unsigned char *outputBuffer,
                          unsigned int bufferSize
                          )
{
    faacEncStruct* hEncoder = (faacEncStruct*)hpEncoder;
    unsigned int channel, i;
    int sb, frameBytes;
    unsigned int offset;
    BitStream *bitStream; /* bitstream used for writing the frame to */
#ifdef DRM
    int desbits, diff;
    double fix;
#endif

    /* local copy's of parameters */
    ChannelInfo *channelInfo = hEncoder->channelInfo;
    CoderInfo *coderInfo = hEncoder->coderInfo;
    unsigned int numChannels = hEncoder->numChannels;
    unsigned int useLfe = hEncoder->config.useLfe;
    unsigned int useTns = hEncoder->config.useTns;
    unsigned int allowMidside = hEncoder->config.allowMidside;
    unsigned int bandWidth = hEncoder->config.bandWidth;
    unsigned int shortctl = hEncoder->config.shortctl;
    int maxqual = hEncoder->config.outputFormat ? MAXQUALADTS : MAXQUAL;

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
    for (channel = 0; channel < numChannels; channel++) 
	{
		double *tmp;


		if (!hEncoder->sampleBuff[channel])
			hEncoder->sampleBuff[channel] = (double*)AllocMemory(FRAME_LEN*sizeof(double));
		
		tmp = hEncoder->sampleBuff[channel];

        hEncoder->sampleBuff[channel]		= hEncoder->nextSampleBuff[channel];
        hEncoder->nextSampleBuff[channel]	= hEncoder->next2SampleBuff[channel];
        hEncoder->next2SampleBuff[channel]	= hEncoder->next3SampleBuff[channel];
		hEncoder->next3SampleBuff[channel]	= tmp;

        if (samplesInput == 0)
        {
            /* start flushing*/
            for (i = 0; i < FRAME_LEN; i++)
                hEncoder->next3SampleBuff[channel][i] = 0.0;
        }
        else
        {
			int samples_per_channel = samplesInput/numChannels;

            /* handle the various input formats and channel remapping */
            switch( hEncoder->config.inputFormat )
			{
                case FAAC_INPUT_16BIT:
					{
						short *input_channel = (short*)inputBuffer + hEncoder->config.channel_map[channel];

						for (i = 0; i < samples_per_channel; i++)
						{
							hEncoder->next3SampleBuff[channel][i] = (double)*input_channel;
							input_channel += numChannels;
						}
					}
                    break;

                case FAAC_INPUT_32BIT:
					{
						int32_t *input_channel = (int32_t*)inputBuffer + hEncoder->config.channel_map[channel];
						
						for (i = 0; i < samples_per_channel; i++)
						{
							hEncoder->next3SampleBuff[channel][i] = (1.0/256) * (double)*input_channel;
							input_channel += numChannels;
						}
					}
                    break;

                case FAAC_INPUT_FLOAT:
					{
						float *input_channel = (float*)inputBuffer + hEncoder->config.channel_map[channel];

						for (i = 0; i < samples_per_channel; i++)
						{
							hEncoder->next3SampleBuff[channel][i] = (double)*input_channel;
							input_channel += numChannels;
						}
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
			hEncoder->psymodel->PsyBufferUpdate( 
					&hEncoder->fft_tables, 
					&hEncoder->gpsyInfo, 
					&hEncoder->psyInfo[channel],
					hEncoder->next3SampleBuff[channel], 
					bandWidth,
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
        hEncoder->srInfo->num_cb_short, numChannels, (double)hEncoder->aacquantCfg.quality / DEFQUAL);

    hEncoder->psymodel->BlockSwitch(coderInfo, hEncoder->psyInfo, numChannels);

    /* force block type */
    if (shortctl == SHORTCTL_NOSHORT)
    {
		for (channel = 0; channel < numChannels; channel++)
		{
			coderInfo[channel].block_type = ONLY_LONG_WINDOW;
		}
    }
    if (shortctl == SHORTCTL_NOLONG)
    {
		for (channel = 0; channel < numChannels; channel++)
		{
			coderInfo[channel].block_type = ONLY_SHORT_WINDOW;
		}
    }

    /* AAC Filterbank, MDCT with overlap and add */
    for (channel = 0; channel < numChannels; channel++) {
        FilterBank(hEncoder,
            &coderInfo[channel],
            hEncoder->sampleBuff[channel],
            hEncoder->freqBuff[channel],
            hEncoder->overlapBuff[channel],
            MOVERLAPPED);
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

    for (channel = 0; channel < numChannels; channel++) {
		if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
			SortForGrouping(&coderInfo[channel],
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

    for (channel = 0; channel < numChannels; channel++)
    {
        CalcAvgEnrg(&coderInfo[channel], hEncoder->freqBuff[channel]);
    }

#ifdef DRM
    /* loop the quantization until the desired bit-rate is reached */
    diff = 1; /* to enter while loop */
    hEncoder->aacquantCfg.quality = 120; /* init quality setting */
    while (diff > 0) { /* if too many bits, do it again */
#endif
    /* Quantize and code the signal */
    for (channel = 0; channel < numChannels; channel++) {
        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            AACQuantize(&coderInfo[channel],
                        hEncoder->srInfo->cb_width_short,
                        hEncoder->srInfo->num_cb_short, hEncoder->freqBuff[channel],
                        &(hEncoder->aacquantCfg));
        } else {
            AACQuantize(&coderInfo[channel],
                        hEncoder->srInfo->cb_width_long,
                        hEncoder->srInfo->num_cb_long, hEncoder->freqBuff[channel],
                        &(hEncoder->aacquantCfg));
        }
    }

#ifdef DRM
    /* Write the AAC bitstream */
    bitStream = OpenBitStream(bufferSize, outputBuffer);
    WriteBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannels);

    /* Close the bitstream and return the number of bytes written */
    frameBytes = CloseBitStream(bitStream);

    /* now calculate desired bits and compare with actual encoded bits */
    desbits = (int) ((double) numChannels * (hEncoder->config.bitRate * FRAME_LEN)
            / hEncoder->sampleRate);

    diff = ((frameBytes - 1 /* CRC */) * 8) - desbits;

    /* do linear correction according to relative difference */
    fix = (double) desbits / ((frameBytes - 1 /* CRC */) * 8);

    /* speed up convergence. A value of 0.92 gives approx up to 10 iterations */
    if (fix > 0.92)
        fix = 0.92;

    hEncoder->aacquantCfg.quality *= fix;

    /* quality should not go lower than 1, set diff to exit loop */
    if (hEncoder->aacquantCfg.quality <= 1)
        diff = -1;
    }
#endif

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
#ifndef DRM
    /* Write the AAC bitstream */
    bitStream = OpenBitStream(bufferSize, outputBuffer);

    WriteBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannels);

    /* Close the bitstream and return the number of bytes written */
    frameBytes = CloseBitStream(bitStream);

    /* Adjust quality to get correct average bitrate */
    if (hEncoder->config.bitRate)
	{
		double fix;
		int desbits = numChannels * (hEncoder->config.bitRate * FRAME_LEN)
				/ hEncoder->sampleRate;
		int diff = (frameBytes * 8) - desbits;

		hEncoder->bitDiff += diff;
		fix = (double)hEncoder->bitDiff / desbits;
		fix *= 0.01;
		fix = max(fix, -0.2);
		fix = min(fix, 0.2);

		if (((diff > 0) && (fix > 0.0)) || ((diff < 0) && (fix < 0.0)))
		{
			hEncoder->aacquantCfg.quality *= (1.0 - fix);
                        if (hEncoder->aacquantCfg.quality > maxqual)
                            hEncoder->aacquantCfg.quality = maxqual;
            if (hEncoder->aacquantCfg.quality < 50)
                hEncoder->aacquantCfg.quality = 50;
		}
    }
#endif

    return frameBytes;
}


#ifdef DRM
/* Scalefactorband data table for 960 transform length */
/* all parameters which are different from the 1024 transform length table are
   marked with an "x" */
static SR_INFO srInfo[12+1] =
{
    { 96000, 40/*x*/, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28,
            36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 0/*x*/
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 28/*x*/
        }
    }, { 88200, 40/*x*/, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 8, 12, 12, 12, 12, 12, 16, 16, 24, 28,
            36, 44, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 0/*x*/
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 28/*x*/
        }
    }, { 64000, 45/*x*/, 12,
        {
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            8, 8, 8, 8, 12, 12, 12, 16, 16, 16, 20, 24, 24, 28,
            36, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
            40, 40, 40, 16/*x*/, 0/*x*/
        },{
            4, 4, 4, 4, 4, 4, 8, 8, 8, 16, 28, 28/*x*/
        }
    }, { 48000, 49, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32/*x*/
        }, {
            4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 8/*x*/
        }
    }, { 44100, 49, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28, 28, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32/*x*/
        }, {
            4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 8/*x*/
        }
    }, { 32000, 49/*x*/, 14,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 20, 20, 24, 24, 28,
            28, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 0/*x*/, 0/*x*/
        },{
            4,  4,  4,  4,  4,  8,  8,  8,  12, 12, 12, 16, 16, 16
        }
    }, { 24000, 46/*x*/, 15,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
            36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 0/*x*/
        }, {
            4,  4,  4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 12/*x*/
        }
    }, { 22050, 46/*x*/, 15,
        {
            4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  8,  8,  8,  8,  8,  8,  8,
            8,  8,  8,  12, 12, 12, 12, 16, 16, 16, 20, 20, 24, 24, 28, 28, 32,
            36, 36, 40, 44, 48, 52, 52, 64, 64, 64, 64, 0/*x*/
        }, {
            4,  4,  4,  4,  4,  4,  4,  8,  8,  8, 12, 12, 16, 16, 12/*x*/
        }
    }, { 16000, 42/*x*/, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 0/*x*/
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 12/*x*/
        }
    }, { 12000, 42/*x*/, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 0/*x*/
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 12/*x*/
        }
    }, { 11025, 42/*x*/, 15,
        {
            8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 16, 16, 16, 16, 20, 20, 20, 24,
            24, 28, 28, 32, 36, 40, 40, 44, 48, 52, 56, 60, 64, 64, 0/*x*/
        }, {
            4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 12, 16, 20, 12/*x*/
        }
    }, { 8000, 40, 15,
        {
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 16,
            16, 16, 16, 16, 16, 16, 20, 20, 20, 20, 24, 24, 24, 28,
            28, 32, 36, 36, 40, 44, 48, 52, 56, 60, 64, 16/*x*/
        }, {
            4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 12, 16, 20, 12/*x*/
        }
    },
    { -1 }
};
#else
/* Scalefactorband data table for 1024 transform length */
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
#endif
