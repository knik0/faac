/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2026 Nils Schimmelmann
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

#ifndef RATECONTROL_H
#define RATECONTROL_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Rate control: steers the quantizer's masking-target multiplier toward a
 * target bitrate.
 *
 * This is NOT an AAC bit reservoir. Nothing here moves a bit between frames
 * and every raw_data_block is self-contained; the ADTS buffer_fullness field
 * stays the 0x7FF sentinel. The ACCOUNT (balance) is a clamped integral of the
 * per-frame bit error: underspend banks, overspend owes, nothing is forgiven,
 * and `lend` is the share of the balance offered to a frame. The actuator is
 * quality, not bits. */
typedef struct {
    int frameBudget;   /* bits per frame at the target rate; 0 = VBR */

    int balance;       /* signed: banked bits positive, owed bits negative */
    int sbrBitsAcc;    /* EWMA of the SBR payload, scaled by 1<<RC_SBR_EWMA_SHIFT */
} RateControl;

/* Opens the account square at zero for the resolved rate. */
void RateControlReset(RateControl *rc, unsigned int numChannels,
                      unsigned long bitRate, unsigned long sampleRate);

/* Settles a coded frame of payloadBits (no ADTS header), of which sbrBits were
   SBR payload, against the account and returns the quality for the next frame.
   SBR's share is charged separately so the controller does not starve the core
   to pay for it. */
float RateControlUpdate(RateControl *rc, int payloadBits, int sbrBits,
                        float quality, float maxqual);

#ifdef FAAC_STATS
void RateControlStatsInit(void);
void RateControlStatsPrint(FILE *out);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RATECONTROL_H */
