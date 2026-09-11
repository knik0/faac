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

#ifndef FAAC_STATS_H
#define FAAC_STATS_H

#ifdef FAAC_STATS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faacEncStats {
    unsigned int totalFrames;
    unsigned int transientFrames;
    unsigned int shortChannels;
    unsigned int shortSplitChannels;
    unsigned long shortGroupSum;
    double totalQuality;

    unsigned long totalBands;
    unsigned long msBands;
    unsigned long isBands;
    unsigned long pnsBands;

    unsigned int longBlocks;
    unsigned int longBlocksTNS;

    unsigned int sbrFrames;
    unsigned int sbrTransientFrames;
    unsigned long sbrInvfSum;
    unsigned long sbrInvfCount;

    double totalAttack;
    float maxAttack;
    unsigned long attackCount;

    unsigned int peakRetryFrames;

    /* Bit reservoir (CBR). An underflow means the declared fullness is false;
       stuffing bits are the mode's cost. */
    float minResFill;
    unsigned int resFrames;
    unsigned int resBoundRetryFrames;
    unsigned int resUnderflowFrames;
    double resStuffBits;
    double resTotalBits;

    double totalBalanceRatio;
    float minBalanceRatio;
    float maxBalanceRatio;
    unsigned int balanceFrames;

    /* Rate-control quality clamp: is the loop's authority exhausted? A frame
       that overshoots its bit target while quality already sits on MINQUAL is
       floor-limited -- the controller has nothing left to give and no amount of
       loop tuning can recover the bits. One that overshoots off the floor is
       loop-limited, and the gain/damping are then fair game. */
    unsigned int minqualFrames;
    unsigned int maxqualFrames;
    unsigned int overshootFrames;
    unsigned int minqualOvershootFrames;

    /* Signed frame bit error (totalBits - desbits), split by sign, so the mean
       miss on each side is separable from how often each side occurs. */
    double sumOverBits;
    double sumUnderBits;
    double sumDesBits;
    unsigned int underFrames;
} faacEncStats;

extern faacEncStats g_faacStats;

#ifdef __cplusplus
}
#endif

#endif /* FAAC_STATS */

#endif /* FAAC_STATS_H */
