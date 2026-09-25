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

#ifndef STEREO_H
#define STEREO_H

#include "channels.h"
#include "faac_internal.h"
#include "util.h"

/* Joint-stereo policy resolved once per configuration: per window type, the
 * bands that may take M/S and the first intensity-coded band. */
typedef struct {
    JointMode mode;
    int isStart[2];       /* indexed by window type: 0 long, 1 short */
    int msEnd[2];         /* bands below may take M/S; 0 = none */
} StereoConfig;

void StereoConfigure(StereoConfig *cfg, JointMode mode, int sampleRate, unsigned int bandWidth,
                     unsigned long bitRatePerCh, int sbr, const int *sfbOffset[2], const int sfbn[2]);

void AACstereo(CoderInfo *coder,
               AACElement *elements,
               int numElements,
               float *s[MAX_CHANNELS],
               float quality,
               const StereoConfig *cfg);

#endif
