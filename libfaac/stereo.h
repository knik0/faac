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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "channels.h"
#include "util.h"

void AACstereo(CoderInfo *coder,
               AACElement *elements,
               int numElements,
               float *s[MAX_CHANNELS],
               float quality,
               int mode,
               int sampleRate
              );

#endif
