/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2002 Krzysztof Nikiel
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

#ifndef _INPUT_H
#define _INPUT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
  FILE *f;
  uint16_t channels;
  uint8_t samplebytes;
  uint32_t samplerate;
  int64_t samples;
  bool bigendian;
  bool swap;
  bool isfloat;
} pcmfile_t;

pcmfile_t *wav_open_read(const char *path, bool rawchans);
size_t wav_read_float32(pcmfile_t *sndf, float *buf, size_t num, int *map);
int wav_close(pcmfile_t *file);

/* Create channel remapping array for multi-channel input, consumed by
   wav_read_float32()'s internal chan_remap(). */
int *mk_chan_map(uint16_t channels, uint16_t center, uint16_t lf);

#ifdef __cplusplus
}
#endif
#endif /* _INPUT_H */
