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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef FAAC_REAL_H
#define FAAC_REAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#ifdef FAAC_PRECISION_SINGLE
typedef float faac_real;
#define FAAC_SIN sinf
#define FAAC_COS cosf
#define FAAC_SQRT sqrtf
#define FAAC_FABS fabsf
#define FAAC_LOG10 log10f
#define FAAC_POW powf
#define FAAC_ASIN asinf
#define FAAC_LRINT lrintf
#define FAAC_FLOOR floorf
#else
typedef double faac_real;
#define FAAC_SIN sin
#define FAAC_COS cos
#define FAAC_SQRT sqrt
#define FAAC_FABS fabs
#define FAAC_LOG10 log10
#define FAAC_POW pow
#define FAAC_ASIN asin
#define FAAC_LRINT lrint
#define FAAC_FLOOR floor
#endif

#endif /* FAAC_REAL_H */
