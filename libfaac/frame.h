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
 * $Id: frame.h,v 1.9 2001/04/11 13:50:31 menno Exp $
 */

#ifndef FRAME_H
#define FRAME_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "coder.h"
#include "channels.h"
#include "psych.h"

#ifdef WIN32
  #pragma pack(push, 8)
  #ifndef FAACAPI
    #define FAACAPI __stdcall
  #endif
#else
  #ifndef FAACAPI
    #define FAACAPI
  #endif
#endif


static SR_INFO srInfo[12+1];

typedef struct faacEncConfiguration
{
	/* AAC object type */
	unsigned int aacObjectType;

	/* Allow mid/side coding */
	unsigned int allowMidside;

	/* Use one of the channels as LFE channel */
	unsigned int useLfe;

	/* Use Temporal Noise Shaping */
	unsigned int useTns;

	/* Use Long Term Prediction */
	unsigned int useLtp;

	/* bitrate / channel of AAC file */
	unsigned long bitRate;

	/* AAC file frequency bandwidth */
	unsigned int bandWidth;

} faacEncConfiguration, *faacEncConfigurationPtr;

typedef struct {
	/* number of channels in AAC file */
	unsigned int numChannels;

	/* samplerate of AAC file */
	unsigned long sampleRate;
	unsigned int sampleRateIdx;

	unsigned int usedBytes;

	/* frame number */
	unsigned int frameNum;
	unsigned int flushFrame;

	/* Scalefactorband data */
	SR_INFO *srInfo;

	/* sample buffers of current next and next next frame*/
	double *sampleBuff[MAX_CHANNELS];
	double *nextSampleBuff[MAX_CHANNELS];
	double *next2SampleBuff[MAX_CHANNELS];
	double *ltpTimeBuff[MAX_CHANNELS];

	/* Filterbank buffers */
	double *sin_window_long;
	double *sin_window_short;
	double *kbd_window_long;
	double *kbd_window_short;
	double *freqBuff[MAX_CHANNELS];
	double *overlapBuff[MAX_CHANNELS];

	double *msSpectrum[MAX_CHANNELS];

	/* Channel and Coder data for all channels */
	CoderInfo coderInfo[MAX_CHANNELS];
	ChannelInfo channelInfo[MAX_CHANNELS];

	/* Psychoacoustics data */
	PsyInfo psyInfo[MAX_CHANNELS];
	GlobalPsyInfo gpsyInfo;

	/* Configuration data */
	faacEncConfiguration config;

} faacEncStruct, *faacEncHandle;

faacEncConfigurationPtr FAACAPI faacEncGetCurrentConfiguration(faacEncHandle hEncoder);
int FAACAPI faacEncSetConfiguration (faacEncHandle hEncoder, faacEncConfigurationPtr config);

faacEncHandle FAACAPI faacEncOpen(unsigned long sampleRate,
								  unsigned int numChannels,
								  unsigned long *inputSamples,
								  unsigned long *maxOutputBytes);

int FAACAPI faacEncEncode(faacEncHandle hEncoder,
						  short *inputBuffer,
						  unsigned int samplesInput,
						  unsigned char *outputBuffer,
						  unsigned int bufferSize
						  );

int FAACAPI faacEncClose(faacEncHandle hEncoder);


#ifdef WIN32
  #pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FRAME_H */
