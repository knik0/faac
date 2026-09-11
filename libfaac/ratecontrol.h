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
 * target bitrate, and in CBR models the decoder buffer as a bit reservoir.
 *
 * Two integrators, each with a reason. The ACCOUNT (balance) is a clamped
 * integral of the per-frame bit error, in every rate mode: underspend banks,
 * overspend owes, `lend` is the share of the balance offered to a frame. The
 * RESERVOIR (res*) is the AAC decoder buffer, CBR only: a hard per-frame bound
 * on what may be sent, refilled at the mean rate. `lend` reads the account,
 * not the reservoir fill: steering from the fill measured worse. */
typedef struct {
    int frameBudget;   /* bits per frame at the target rate; 0 = VBR */

    int balance;       /* signed: banked bits positive, owed bits negative */
    int sbrBitsAcc;    /* EWMA of the SBR payload, scaled by 1<<RC_SBR_EWMA_SHIFT */

    /* Bit reservoir, AAC_MAX_BITS_PER_CH per channel. resMean == 0 outside
       CBR switches cap, floor and header declaration off together. */
    int resMean;       /* bits arriving per frame */
    int resCap;        /* capacity: 6144 * channels - resMean */
    int resFill;       /* fill after the last frame, [0, resCap] */
    int resMinBits;    /* floor BuildFrame stuffs this frame up to; 0 = none */
    int stuffedBits;   /* what BuildFrame stuffed into the last frame */
    int sbrBits;       /* SBR payload BuildFrame wrote into the last frame */
    int prevWant;      /* previous frame's core bits if it was stuffed, else 0 */
} RateControl;

/* Opens the account and, for CBR, sizes the reservoir from the resolved rate. */
void RateControlReset(RateControl *rc, unsigned int numChannels,
                      unsigned long bitRate, unsigned long sampleRate, int cbr);

/* Fill after a raw_data_block of payloadBits (no ADTS header). Used by both the
   ADTS header and the controller so the two cannot disagree. */
int RateControlReservoirAfter(const RateControl *rc, int payloadBits);

/* CBR: the most the next frame may send (mean + fill) and, as a side effect,
   the floor BuildFrame stuffs it up to. 0 when there is no reservoir. */
int RateControlFrameCap(RateControl *rc);

/* Settles a coded frame of payloadBits against the account and reservoir and
   returns the quality for the next frame. SBR's share is charged separately
   so the controller does not starve the core to pay for it. */
float RateControlUpdate(RateControl *rc, int payloadBits,
                        float quality, float maxqual);

#ifdef FAAC_STATS
void RateControlStatsInit(void);
void RateControlStatsPrint(FILE *out);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RATECONTROL_H */
