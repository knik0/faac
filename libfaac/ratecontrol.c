/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2003-2017 Krzysztof Nikiel
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

#include <math.h>

#include "ratecontrol.h"
#include "coder.h"
#include "quantize.h"
#include "stats.h"

#define RC_DAMPING_FACTOR      0.6f   /* Control loop damping */

/* AIM banks against slightly under the nominal frame budget, so the average
 * comes out a ceiling rather than a midpoint. AMORT spreads the balance over
 * that many frames; it trades bitrate accuracy against coding efficiency and is
 * the dominant lever here. Bitrate error alone cannot see efficiency, and ranks
 * this arm behind alternatives that simply spend more bits. */
#define RC_BALANCE_AIM         0.99f
#define RC_BALANCE_AMORT       32

/* How far the account may run in either direction, in frames of budget.
 * Symmetric on purpose: capacity is only two frames, so bounding one side
 * tighter leaves the arm biased toward the side it still remembers. */
#define RC_BALANCE_FRAMES      4

/* Time constant of the SBR payload average, as a right shift: 1/16 per frame,
   so ~16 frames (0.37 s at 48 kHz). Long enough to average out the payload's
   frame-to-frame swing, short enough to follow a real change in content. */
#define RC_SBR_EWMA_SHIFT      4

void RateControlReset(RateControl *rc, unsigned int numChannels,
                      unsigned long bitRate, unsigned long sampleRate)
{
    rc->frameBudget = numChannels * (bitRate * FRAME_LEN) / sampleRate;

    /* A signed account opens square at zero: any nonzero start is credit the
       stream never earned, which `lend` then spends. */
    rc->balance = 0;
    /* Negative means "no frame seen yet"; the first frame seeds the average
       rather than dragging it up from zero over the whole time constant. */
    rc->sbrBitsAcc = -1;
}

float RateControlUpdate(RateControl *rc, int payloadBits, int sbrBits,
                        float quality, float maxqual)
{
    int desbits = rc->frameBudget;
    int totalBits = payloadBits;
    int sbrCharge;
    float fix;

    /* SBR payload is not fixed overhead: at 48 kHz it runs 135-255 bits against
       a 2048-bit budget. Charging the frame's own `sbrBits` makes the
       controller answer for a quantity it does not control -- a payload spike
       enters the ledger as core debt, and `lend` squeezes core quality to
       repay it. Charging the running average lets the spike reach the output
       rate instead. On LC `sbrBits` is 0. */
    {
        int prev = rc->sbrBitsAcc;
        rc->sbrBitsAcc = (prev < 0)
            ? (sbrBits << RC_SBR_EWMA_SHIFT)
            : (prev + sbrBits - (prev >> RC_SBR_EWMA_SHIFT));
    }
    sbrCharge = rc->sbrBitsAcc >> RC_SBR_EWMA_SHIFT;

    /* Settle the frame against the account. The balance is signed and both
       directions are kept: forgiving credit but not debt biases the loop by
       however much each clip drew. One CORE setpoint serves the account and
       `fix` alike -- the frame's budget less what SBR was charged, against
       what the core actually spent. */
    int coreTarget = (int)(desbits * RC_BALANCE_AIM) - sbrCharge;
    int coreBits = totalBits - sbrBits;
    int lend;
    int bound = RC_BALANCE_FRAMES * desbits;
    int diff = coreTarget - coreBits;
    rc->balance += diff;
    if (rc->balance > bound)
        rc->balance = bound;
    else if (rc->balance < -bound)
        rc->balance = -bound;

    /* What this frame may lean on, signed and amortized: a credit lets a
       complex frame overspend, a debt holds the next frames under budget
       until it is repaid. Amortizing is what keeps a deep balance from being
       worked off in one lurch. */
    lend = rc->balance / RC_BALANCE_AMORT;

    if (coreBits > 0)
        fix = (float)(coreTarget + lend) / (float)coreBits;
    else
        fix = 1.0f;

    /* Stiffer damping when the account is far off centre. Removing this
       and the deadband below costs 16 kHz speech MOS under ABR. */
    float damping = RC_DAMPING_FACTOR;
    float fillRatio = 0.5f + 0.5f * (float)rc->balance / (float)bound;
    if (fillRatio < 0.25f || fillRatio > 0.75f)
        damping = 0.85f;
#ifdef FAAC_STATS
    {
        float fillPct = fillRatio * 100.0f;
        g_faacStats.totalBalanceRatio += fillPct;
        if (fillPct < g_faacStats.minBalanceRatio) g_faacStats.minBalanceRatio = fillPct;
        if (fillPct > g_faacStats.maxBalanceRatio) g_faacStats.maxBalanceRatio = fillPct;
        g_faacStats.balanceFrames++;
    }
#endif

    fix = (fix - 1.0f) * damping + 1.0f;

    /* Skip small adjustments (< 0.5%) to keep quality steady */
    if (fabsf(fix - 1.0f) > 0.005f) {
        fix = (fix < 0.80f) ? 0.80f : ((fix > 1.20f) ? 1.20f : fix);
        quality *= fix;
    }

    if (quality > maxqual)
        quality = maxqual;
    if (quality < MINQUAL)
        quality = MINQUAL;

#ifdef FAAC_STATS
    {
        int overshot = (totalBits > desbits);
        g_faacStats.sumDesBits += desbits;
        if (overshot) g_faacStats.sumOverBits += (totalBits - desbits);
        else { g_faacStats.underFrames++; g_faacStats.sumUnderBits += (desbits - totalBits); }
        int atFloor = (quality <= MINQUAL);
        if (atFloor) g_faacStats.minqualFrames++;
        if (quality >= maxqual) g_faacStats.maxqualFrames++;
        if (overshot) g_faacStats.overshootFrames++;
        if (overshot && atFloor) g_faacStats.minqualOvershootFrames++;
    }
#endif

    return quality;
}

#ifdef FAAC_STATS
void RateControlStatsInit(void)
{
    g_faacStats.minBalanceRatio = 100.0f;
}

void RateControlStatsPrint(FILE *out)
{
    if (g_faacStats.balanceFrames > 0)
    {
        fprintf(out, " Rate Control & Cap  : Bit Balance = %5.1f%% (min %5.1f%%, max %5.1f%%) | Peak Retries = %5.1f%% (%u/%u)\n",
                g_faacStats.totalBalanceRatio / g_faacStats.balanceFrames,
                g_faacStats.minBalanceRatio, g_faacStats.maxBalanceRatio,
                100.0 * g_faacStats.peakRetryFrames / g_faacStats.totalFrames,
                g_faacStats.peakRetryFrames, g_faacStats.totalFrames);
    }
    if (g_faacStats.balanceFrames > 0)
    {
        unsigned int rcf = g_faacStats.balanceFrames;
        fprintf(out, " Bit Error           : mean over = %7.1f bits (%u fr) | mean under = %7.1f bits (%u fr) | net = %+6.2f%% of target\n",
                g_faacStats.overshootFrames ? g_faacStats.sumOverBits / g_faacStats.overshootFrames : 0.0,
                g_faacStats.overshootFrames,
                g_faacStats.underFrames ? g_faacStats.sumUnderBits / g_faacStats.underFrames : 0.0,
                g_faacStats.underFrames,
                g_faacStats.sumDesBits > 0 ? 100.0 * (g_faacStats.sumOverBits - g_faacStats.sumUnderBits) / g_faacStats.sumDesBits : 0.0);
        fprintf(out, " Quality Clamp       : Overshoot = %5.1f%% (%u/%u) | at MINQUAL = %5.1f%% | overshoot AND floored = %5.1f%% | at MAXQUAL = %5.1f%%\n",
                100.0 * g_faacStats.overshootFrames / rcf, g_faacStats.overshootFrames, rcf,
                100.0 * g_faacStats.minqualFrames / rcf,
                100.0 * g_faacStats.minqualOvershootFrames / rcf,
                100.0 * g_faacStats.maxqualFrames / rcf);
    }
}
#endif
