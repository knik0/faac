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
#include <string.h>

#include "frame.h"
#include "coder.h"
#include "channels.h"
#include "bitstream.h"
#include "filtbank.h"
#include "util.h"
#include "tns.h"
#include "stereo.h"

static char *libfaacName = PACKAGE_VERSION;
static char *libCopyright =
  "FAAC - Freeware Advanced Audio Coder (http://faac.sourceforge.net/)\n"
  " Copyright (C) 1999,2000,2001  Menno Bakker\n"
  " Copyright (C) 2002,2003,2017  Krzysztof Nikiel\n"
  "This software is based on the ISO MPEG-4 reference source code.\n";

static const psymodellist_t psymodellist[] = {
  {&psymodel2, "knipsycho psychoacoustic"},
  {NULL}
};

static SR_INFO srInfo[12+1];

// default bandwidth/samplerate ratio
static const struct {
    double fac;
    double freq;
} g_bw = {0.42, 18000};

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

    hEncoder->config.jointmode = config->jointmode;
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

#ifdef DRM
    config->pnslevel = 0;
#endif

    /* Re-init TNS for new profile */
    TnsInit(hEncoder);

    /* Check for correct bitrate */
    if (config->bitRate > (MaxBitrate(hEncoder->sampleRate) / hEncoder->numChannels))
        config->bitRate = MaxBitrate(hEncoder->sampleRate) / hEncoder->numChannels;
#if 0
    if (config->bitRate < MinBitrate())
        return 0;
#endif

    if (config->bitRate && !config->bandWidth)
    {
        config->bandWidth = (double)config->bitRate * hEncoder->sampleRate * g_bw.fac / 50000.0;
        if (config->bandWidth > g_bw.freq)
            config->bandWidth = g_bw.freq;

        if (!config->quantqual)
        {
            config->quantqual = (double)config->bitRate * hEncoder->numChannels / 1280;
            if (config->quantqual > 100)
                config->quantqual = (config->quantqual - 100) * 3.0 + 100;
        }
    }

    if (!config->quantqual)
        config->quantqual = DEFQUAL;

    hEncoder->config.bitRate = config->bitRate;

    if (!config->bandWidth)
    {
        config->bandWidth = g_bw.fac * hEncoder->sampleRate;
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

    if (config->jointmode == JOINT_MS)
        config->pnslevel = 0;
    if (config->pnslevel < 0)
        config->pnslevel = 0;
    if (config->pnslevel > 10)
        config->pnslevel = 10;
    hEncoder->aacquantCfg.pnslevel = config->pnslevel;
    /* set quantization quality */
    hEncoder->aacquantCfg.quality = config->quantqual;
    CalcBW(&hEncoder->config.bandWidth,
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
    *maxOutputBytes = ADTS_FRAMESIZE;

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
    hEncoder->config.jointmode = JOINT_IS;
    hEncoder->config.pnslevel = 4;
    hEncoder->config.useLfe = 1;
    hEncoder->config.useTns = 0;
    hEncoder->config.bitRate = 64000;
    hEncoder->config.bandWidth = g_bw.fac * hEncoder->sampleRate;
    hEncoder->config.quantqual = 0;
    hEncoder->config.psymodellist = (psymodellist_t *)psymodellist;
    hEncoder->config.psymodelidx = 0;
    hEncoder->psymodel =
      (psymodel_t *)hEncoder->config.psymodellist[hEncoder->config.psymodelidx].ptr;
    hEncoder->config.shortctl = SHORTCTL_NORMAL;

	/* default channel map is straight-through */
	for( channel = 0; channel < MAX_CHANNELS; channel++ )
		hEncoder->config.channel_map[channel] = channel;

    hEncoder->config.outputFormat = ADTS_STREAM;

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
        hEncoder->coderInfo[channel].groups.n = 1;
        hEncoder->coderInfo[channel].groups.len[0] = 1;

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

    fft_terminate(&hEncoder->fft_tables);

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

    BlocStat();

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
    unsigned int jointmode = hEncoder->config.jointmode;
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
    else if ((hEncoder->frameNum <= 4) || (shortctl == SHORTCTL_NOLONG))
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

    for (channel = 0; channel < numChannels; channel++) {
        channelInfo[channel].msInfo.is_present = 0;

        if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
            coderInfo[channel].sfbn = hEncoder->aacquantCfg.max_cbs;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].sfbn; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_short[sb];
            }
            coderInfo[channel].sfb_offset[sb] = offset;
            BlocGroup(hEncoder->freqBuff[channel], coderInfo + channel, &hEncoder->aacquantCfg);
        } else {
            coderInfo[channel].sfbn = hEncoder->aacquantCfg.max_cbl;

            coderInfo[channel].groups.n = 1;
            coderInfo[channel].groups.len[0] = 1;

            offset = 0;
            for (sb = 0; sb < coderInfo[channel].sfbn; sb++) {
                coderInfo[channel].sfb_offset[sb] = offset;
                offset += hEncoder->srInfo->cb_width_long[sb];
            }
            coderInfo[channel].sfb_offset[sb] = offset;
        }
    }

    /* Perform TNS analysis and filtering */
    for (channel = 0; channel < numChannels; channel++) {
        if ((!channelInfo[channel].lfe) && (useTns)) {
            TnsEncode(&(coderInfo[channel].tnsInfo),
                      coderInfo[channel].sfbn,
                      coderInfo[channel].sfbn,
                      coderInfo[channel].block_type,
                      coderInfo[channel].sfb_offset,
                      hEncoder->freqBuff[channel]);
        } else {
            coderInfo[channel].tnsInfo.tnsDataPresent = 0;      /* TNS not used for LFE */
        }
    }

    for (channel = 0; channel < numChannels; channel++) {
      // reduce LFE bandwidth
		if (!channelInfo[channel].cpe && channelInfo[channel].lfe)
		{
                    coderInfo[channel].sfbn = 3;
		}
	}

    AACstereo(coderInfo, channelInfo, hEncoder->freqBuff, numChannels,
              (double)hEncoder->aacquantCfg.quality/DEFQUAL, jointmode);

#ifdef DRM
    /* loop the quantization until the desired bit-rate is reached */
    diff = 1; /* to enter while loop */
    hEncoder->aacquantCfg.quality = 120; /* init quality setting */
    while (diff > 0) { /* if too many bits, do it again */
#endif
    for (channel = 0; channel < numChannels; channel++) {
        BlocQuant(&coderInfo[channel], hEncoder->freqBuff[channel],
                  &(hEncoder->aacquantCfg));
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

                        cil->sfbn = cir->sfbn = max(cil->sfbn, cir->sfbn);
		}
    }
#ifndef DRM
    /* Write the AAC bitstream */
    bitStream = OpenBitStream(bufferSize, outputBuffer);

    if (WriteBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannels) < 0)
        return -1;

    /* Close the bitstream and return the number of bytes written */
    frameBytes = CloseBitStream(bitStream);

    /* Adjust quality to get correct average bitrate */
    if (hEncoder->config.bitRate)
    {
        int desbits = numChannels * (hEncoder->config.bitRate * FRAME_LEN)
            / hEncoder->sampleRate;
        double fix = (double)desbits / (double)(frameBytes * 8);

        if (fix < 0.9)
            fix += 0.1;
        else if (fix > 1.1)
            fix -= 0.1;
        else
            fix = 1.0;

        fix = (fix - 1.0) * 0.5 + 1.0;
        // printf("q: %.1f(f:%.4f)\n", hEncoder->aacquantCfg.quality, fix);

        hEncoder->aacquantCfg.quality *= fix;

        if (hEncoder->aacquantCfg.quality > maxqual)
            hEncoder->aacquantCfg.quality = maxqual;
        if (hEncoder->aacquantCfg.quality < 10)
            hEncoder->aacquantCfg.quality = 10;
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
