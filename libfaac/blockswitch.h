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
 */

#ifndef BLOCKSWITCH_H
#define BLOCKSWITCH_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "coder.h"
#include "channels.h"

struct faacEncStruct;

typedef struct {
	int size;
	int sizeS;

	int block_type;

        void *data;
} PsyInfo;

typedef struct {
	float sampleRate;

	/* shared work buffers */
	float *sharedWorkBuffLong;  /* Used for 2048-sample windows (filtbank, psy, tns, mdct) */
} GlobalPsyInfo;

void PsyInit (GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
		unsigned int numChannels, unsigned int sampleRate);
void PsyEnd (PsyInfo *psyInfo, unsigned int numChannels);
void PsyCalculate (ChannelInfo *channelInfo, PsyInfo *psyInfo,
		unsigned int numChannels);
void PsyBufferUpdate (GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo,
		float * restrict p_lookahead1,
		float * restrict p_lookahead2);
void BlockSwitch (struct faacEncStruct *hEncoder, CoderInfo *coderInfo, PsyInfo *psyInfo,
		unsigned int numChannels);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BLOCKSWITCH_H */
