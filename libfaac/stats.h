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

    double totalReservoirRatio;
    float minReservoirRatio;
    float maxReservoirRatio;
    unsigned int reservoirFrames;
} faacEncStats;

extern faacEncStats g_faacStats;

#ifdef __cplusplus
}
#endif

#endif /* FAAC_STATS */

#endif /* FAAC_STATS_H */
