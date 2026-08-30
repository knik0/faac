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

struct faacEncStruct;

/* Scaling factor to normalize subblock high-pass energy sums e_w = sum(d[n]^2) relative to full-scale PCM power.
 * With PCM float range [-32768, 32767] and 256-sample subblocks, peak subblock energy e_max = 256 * 65536^2 = 1.1e12.
 * Scaling by 1.0e-8f normalizes subblock energies so stream PE (totalPE) maps cleanly to the PE_THRESH_PER_CH complexity threshold. */
#define PE_ENERGY_SCALE      (1.0e-8f)

/* Per-channel Perceptual Entropy complexity threshold: streams exceeding 10.0f/ch PE are classified as high-complexity/transient */
#define PE_THRESH_PER_CH     (10.0f)

typedef struct {
	int size;
	int sizeS;

	int block_type;
	float pe;

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
float PsyGetAttack (PsyInfo *psyInfo);
void PsyCalculate (AACElement *elements, int numElements, PsyInfo *psyInfo,
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
