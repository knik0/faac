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
 */

#ifndef BLOCKSWITCH_H
#define BLOCKSWITCH_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "coder.h"
#include "channels.h"

typedef struct {
	int size;
	int sizeS;

	int block_type;

        void *data;
} PsyInfo;

typedef struct {
	float sampleRate;
	/* Transient rule: see PSY_LEVEL_RATIO_LC in blockswitch.c. */
	float levelRatio;
	float levelSmooth;

	/* shared work buffers */
	float *sharedWorkBuffLong;  /* Used for 2048-sample windows (filtbank, psy, mdct) */
} GlobalPsyInfo;

void PsyInit (GlobalPsyInfo *gpsyInfo, PsyInfo *psyInfo,
		unsigned int numChannels, unsigned int sampleRate, bool heCore);
void PsyEnd (PsyInfo *psyInfo, unsigned int numChannels);
float PsyGetAttack (PsyInfo *psyInfo);
void PsyCalculate (PsyInfo *psyInfo, const bool *isLfeChannel,
		unsigned int numChannels);
void PsyBufferUpdate (GlobalPsyInfo * gpsyInfo, PsyInfo * psyInfo,
		float * restrict p_lookahead1,
		float * restrict p_lookahead2);
void BlockSwitch (CoderInfo *coderInfo, PsyInfo *psyInfo,
		unsigned int numChannels);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BLOCKSWITCH_H */
