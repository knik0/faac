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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Private encoder core. What was historically the public faacEnc* C API is now
 * an internal implementation detail behind the sole public API, faac_* in
 * <faac.h>. These declarations are not installed and, lacking FAACAPI, not
 * exported -- the facade in faac_encoder.c is their only caller.
 */

#ifndef FAAC_INTERNAL_H
#define FAAC_INTERNAL_H

#include <stdint.h>

#define FAAC_CFG_VERSION 106

/* MPEG version */
enum { MPEG4 = 0, MPEG2 = 1 };

/* AAC object types this build implements, numbered per the MPEG-4 AOT
 * registry (mirrors the public FAAC_OBJ_* enum in <faac.h>). AUTO defers the
 * choice between LOW and HE_V1 to faacEncApplyConfig. */
enum { AUTO = 0, LOW = 2, HE_V1 = 5 };

/* PCM input sample format. Named distinctly from the public faac_input_format
 * enumerators (<faac.h>) so the facade can include both headers; faac.c
 * _Static_asserts the two enumerations agree value-for-value. */
enum { INPUT_NULL = 0, INPUT_16BIT, INPUT_24BIT, INPUT_32BIT, INPUT_FLOAT };

/* Block-type control */
enum { SHORTCTL_NORMAL = 0, SHORTCTL_NOSHORT = 1, SHORTCTL_NOLONG = 2 };

enum stream_format { RAW_STREAM = 0, ADTS_STREAM = 1 };

enum { JOINT_NONE = 0, JOINT_MS, JOINT_IS, JOINT_MIXED };

typedef struct faacEncConfiguration
{
    unsigned int mpegVersion;
    unsigned int aacObjectType;
    unsigned int jointmode;
    unsigned int useLfe;
    unsigned int useTns;
    unsigned long bitRate;           /* per channel */
    unsigned int bandWidth;
    unsigned long quantqual;
    unsigned int outputFormat;       /* 0 = raw, 1 = ADTS */
    unsigned int inputFormat;
    int shortctl;
    int channel_map[64];             /* MAX_CHANNELS entries; identity by default */
    int pnslevel;
} faacEncConfiguration, *faacEncConfigurationPtr;

typedef void *faacEncHandle;

int  faacEncGetVersion(char **id, char **copyright);
int  faacEncGetDecoderSpecificInfo(faacEncHandle hEncoder, unsigned char **ppBuffer,
                                   unsigned long *pSizeOfDecoderSpecificInfo);
faacEncHandle faacEncOpen(unsigned long sampleRate, unsigned int numChannels,
                          unsigned long *inputSamples, unsigned long *maxOutputBytes);
int  faacEncEncode(faacEncHandle hEncoder, int32_t *inputBuffer, unsigned int samplesInput,
                   unsigned char *outputBuffer, unsigned int bufferSize);
int  faacEncClose(faacEncHandle hEncoder);

#endif /* FAAC_INTERNAL_H */
