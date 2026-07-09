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

#ifndef QUANTIZE_H
#define QUANTIZE_H

#include "coder.h"

typedef struct
{
    float quality;
    int max_cbl;
    int max_cbs;
    int max_l;
    int pnslevel;
} AACQuantCfg;

/* Rounding bias for the x^(3/4) quantization: 0.4054f minimizes average
 * quantization error for a uniform distribution (ISO 14496-3 §8.3.5). */
#define MAGIC_NUMBER 0.4054f

enum {
    DEFQUAL = 100,
    MAXQUAL = 5000,
    MAXQUALADTS = MAXQUAL,
    MINQUAL = 10,
};

void ResetCoderSections(CoderInfo *coderInfo);
int BlocQuant(CoderInfo *coderInfo, float *xr, AACQuantCfg *aacquantCfg);
void CalcBW(unsigned *bw, int rate, SR_INFO *sr, AACQuantCfg *aacquantCfg);
void BlocGroup(float *xr, CoderInfo *coderInfo, AACQuantCfg *aacquantCfg);
void QuantizeInit(void);

#endif
