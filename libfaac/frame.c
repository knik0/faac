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
 * $Id: frame.c,v 1.10 2001/02/28 18:39:34 menno Exp $
 */

/*
 * CHANGES:
 *  2001/01/17: menno: Added frequency cut off filter.
 *  2001/02/28: menno: Added Temporal Noise Shaping.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

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
	hEncoder->config.aacProfile = config->aacProfile;

	 /* No SSR supported yet */
	if ((hEncoder->config.aacProfile != MAIN)&&(hEncoder->config.aacProfile != LOW))
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
								  unsigned int numChannels)
{
	unsigned int channel;
	faacEncHandle hEncoder;

	hEncoder = (faacEncStruct*)malloc(sizeof(faacEncStruct));
	memset(hEncoder, 0, sizeof(faacEncStruct));

	hEncoder->numChannels = numChannels;
	hEncoder->sampleRate = sampleRate;
	hEncoder->sampleRateIdx = GetSRIndex(sampleRate);

	/* Initialize variables to default values */
	hEncoder->frameNum = 0;
	hEncoder->flushFrame = 0;

	/* Default configuration */
	hEncoder->config.aacProfile = MAIN;
	hEncoder->config.allowMidside = 1;
	hEncoder->config.useLfe = 0;
	hEncoder->config.useTns = 0;
	hEncoder->config.bitRate = 64000; /* default bitrate / channel */
	hEncoder->config.bandWidth = 18000; /* default bandwidth */

	/* find correct sampling rate depending parameters */
	hEncoder->srInfo = &srInfo[hEncoder->sampleRateIdx];

	for (channel = 0; channel < numChannels; channel++) {
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
	PsyInit(&hEncoder->gpsyInfo, hEncoder->psyInfo, hEncoder->numChannels,
		hEncoder->sampleRate, hEncoder->sampleRateIdx);

	FilterBankInit(hEncoder);

    TnsInit(hEncoder);

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

	AACQuantizeEnd();

	HuffmanEnd(hEncoder->coderInfo, hEncoder->numChannels);

	/* Free remaining buffer memory */
	for (channel = 0; channel < hEncoder->numChannels; channel++) {
		if (hEncoder->sampleBuff[channel]) free(hEncoder->sampleBuff[channel]);
		if (hEncoder->nextSampleBuff[channel]) free(hEncoder->nextSampleBuff[channel]);
	}

	/* Free handle */
	if (hEncoder) free(hEncoder);

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

	/* local copy's of parameters */
	ChannelInfo *channelInfo = hEncoder->channelInfo;
	CoderInfo *coderInfo = hEncoder->coderInfo;
	unsigned int numChannels = hEncoder->numChannels;
	unsigned int sampleRate = hEncoder->sampleRate;
	unsigned int aacProfile = hEncoder->config.aacProfile;
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
	if (hEncoder->flushFrame == 2)
		return 0;

	/* Determine the channel configuration */
	GetChannelInfo(channelInfo, numChannels, useLfe);

	/* Update current sample buffers */
	for (channel = 0; channel < numChannels; channel++) {
		if (hEncoder->sampleBuff[channel])
			free(hEncoder->sampleBuff[channel]);
		hEncoder->sampleBuff[channel] = hEncoder->nextSampleBuff[channel];
		hEncoder->nextSampleBuff[channel] = (double*)malloc(FRAME_LEN*sizeof(double));

		if (samplesInput == 0) { /* start flushing*/
			for (i = 0; i < FRAME_LEN; i++)
				hEncoder->nextSampleBuff[channel][i] = 0.0;
		} else {
			for (i = 0; i < (int)(samplesInput/numChannels); i++)
				hEncoder->nextSampleBuff[channel][i] = 
					(double)inputBuffer[(i*numChannels)+channel];
			for (i = (int)(samplesInput/numChannels); i < FRAME_LEN; i++)
				hEncoder->nextSampleBuff[channel][i] = 0.0;
		}

		/* Psychoacoustics */
		/* Update buffers and run FFT on new samples */
		PsyBufferUpdate(&hEncoder->gpsyInfo, &hEncoder->psyInfo[channel],
			hEncoder->nextSampleBuff[channel]);
	}

	if (hEncoder->frameNum <= 1) /* Still filling up the buffers */
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
			hEncoder->overlapBuff[channel]);

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

	MSEncode(coderInfo, channelInfo, hEncoder->freqBuff, numChannels, allowMidside);

	/* Quantize and code the signal */
	bitsToUse = (int)(bitRate*FRAME_LEN/sampleRate+0.5);
	for (channel = 0; channel < numChannels; channel++) {
		if (coderInfo[channel].block_type == ONLY_SHORT_WINDOW) {
			AACQuantize(&coderInfo[channel], &hEncoder->psyInfo[channel],
				&channelInfo[channel], hEncoder->srInfo->cb_width_short,
				hEncoder->srInfo->num_cb_short,	hEncoder->freqBuff[channel], bitsToUse);
		} else {
			AACQuantize(&coderInfo[channel], &hEncoder->psyInfo[channel],
				&channelInfo[channel], hEncoder->srInfo->cb_width_long,
				hEncoder->srInfo->num_cb_long, hEncoder->freqBuff[channel], bitsToUse);
		}
	}

	/* Write the AAC bitstream */
	bitStream = OpenBitStream(bufferSize, outputBuffer);

	WriteBitstream(hEncoder, coderInfo, channelInfo, bitStream, numChannels);

	/* Close the bitstream and return the number of bytes written */
	frameBytes = CloseBitStream(bitStream);

#ifdef _DEBUG
	printf("%4d %4d\n", hEncoder->frameNum-1, frameBytes);
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
			4,	4,	4,	4,	4,	4,	4,	4,	4,	4,	8,	8,	8,	8,	
			8,	8,	8,	12,	12,	12,	12,	16,	16,	20,	20,	24,	24,	28,	
			28,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,	32,
			32,	32,	32,	32,	32,	32,	32,	32,	32
		},{
			4,	4,	4,	4,	4,	8,	8,	8,	12,	12,	12,	16,	16,	16
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
			28, 32, 32, 36, 40, 44, 48, 52, 56, 60, 64, 80
		}, {
			4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 12, 16, 20, 20
		}
	},
	{ -1 }
};
