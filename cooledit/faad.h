/*
 * FAAD - Freeware Advanced Audio Decoder
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
 * $Id: faad.h,v 1.1 2001/07/02 07:26:01 menno Exp $
 */

#ifndef _AACDEC_H
#define _AACDEC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef WIN32
  #pragma pack(push, 8)
  #ifndef FAADAPI
    #define FAADAPI __stdcall
  #endif
#else
  #ifndef FAADAPI
    #define FAADAPI
  #endif
#endif

#define FAAD_OK 0
#define FAAD_OK_CHUPDATE 1
#define FAAD_ERROR 2
#define FAAD_FATAL_ERROR 3

typedef void *faacDecHandle;
typedef void *faacProgConfig;

typedef struct faacDecConfiguration
{
	unsigned int dummy;
} faacDecConfiguration, *faacDecConfigurationPtr;


faacDecHandle FAADAPI faacDecOpen();

faacDecConfigurationPtr FAADAPI faacDecGetCurrentConfiguration(faacDecHandle hDecoder);

int FAADAPI faacDecSetConfiguration(faacDecHandle hDecoder,
									faacDecConfigurationPtr config);

int FAADAPI faacDecInit(faacDecHandle hDecoder,
						unsigned char *buffer,
						unsigned long *samplerate,
						unsigned long *channels);

int FAADAPI faacDecGetProgConfig(faacDecHandle hDecoder,
								 faacProgConfig *progConfig);

int FAADAPI faacDecDecode(faacDecHandle hDecoder,
						  unsigned char *buffer,
						  unsigned long *bytesconsumed,
						  short *sample_buffer);

void FAADAPI faacDecClose(faacDecHandle hDecoder);

#endif

#ifdef WIN32
  #pragma pack(pop)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
