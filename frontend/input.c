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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: input.c,v 1.1 2002/11/23 17:39:14 knik Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include <fcntl.h>
#endif

#include "input.h"

typedef unsigned long u_long;
typedef unsigned short u_short;

typedef struct
{
  u_long label;			/* 'RIFF' */
  u_long length;		/* Length of rest of file */
  u_long chunk_type;		/* 'WAVE' */
}
riff_t;

typedef struct
{
  u_long label;
  u_long len;
}
riffsub_t;

#define WAVE_FORMAT_PCM 1
typedef struct
{
  u_short wFormatTag;
  u_short nChannels;
  u_long nSamplesPerSec;
  u_long nAvgBytesPerSec;
  u_short nBlockAlign;
  u_short wBitsPerSample;
  u_short cbSize;
}
WAVEFORMATEX;

pcmfile_t *wav_open_read(const char *name)
{
  int i;
  int skip;
  FILE *wave_f;
  riff_t riff;
  riffsub_t riffsub;
  WAVEFORMATEX wave;
  char *riffl = "RIFF";
  char *wavel = "WAVE";
  char *fmtl = "fmt ";
  char *datal = "data";
  int fmtsize;
  pcmfile_t *sndf;

  if (!strcmp(name, "-"))
  {
#ifdef WIN32
    setmode(fileno(stdin), O_BINARY);
#endif
    wave_f = stdin;
  }
  else if (!(wave_f = fopen(name, "rb")))
  {
    perror(name);
    return NULL;
  }

  if (fread(&riff, 1, sizeof(riff), wave_f) != sizeof(riff))
    return NULL;
  if (memcmp(&(riff.label), riffl, 4))
    return NULL;
  if (memcmp(&(riff.chunk_type), wavel, 4))
    return NULL;

  if (fread(&riffsub, 1, sizeof(riffsub), wave_f) != sizeof(riffsub))
    return NULL;
  if (memcmp(&(riffsub.label), fmtl, 4))
    return NULL;
  memset(&wave, 0, sizeof(wave));
  fmtsize = (riffsub.len < sizeof(wave)) ? riffsub.len : sizeof(wave);
  if (fread(&wave, 1, fmtsize, wave_f) != fmtsize)
    return NULL;

  for (skip = riffsub.len - fmtsize; skip > 0; skip--)
    fgetc(wave_f);

  for (i = 0;; i++)
  {
    if (fread(&riffsub, 1, sizeof(riffsub), wave_f) != sizeof(riffsub))
      return NULL;
    if (!memcmp(&(riffsub.label), datal, 4))
      break;
    if (i > 10)
      return NULL;

    for (skip = riffsub.len; skip > 0; skip--)
      fgetc(wave_f);
  }
  if (wave.wFormatTag != WAVE_FORMAT_PCM)
    return NULL;

  sndf = malloc(sizeof(*sndf));
  sndf->f = wave_f;
  sndf->channels = wave.nChannels;
  sndf->samplebits = wave.wBitsPerSample;
  sndf->samplerate = wave.nSamplesPerSec;
  sndf->samples = riffsub.len /
    (((wave.wBitsPerSample > 8) ? 2 : 1) * wave.nChannels);
  return sndf;
}

size_t wav_read_short(pcmfile_t *sndf, short *buf, size_t num)
{
  int size;
  int i;

  if (sndf->samplebits > 8)
    return fread(buf, 2, num, sndf->f);

  // convert to 16 bit
  size = fread(buf, 1, num, sndf->f);
  for (i = size - 1; i >= 0; i--)
    buf[i] = (((char *)buf)[i] - 128) * 256;
  return size;
}

int wav_close(pcmfile_t *sndf)
{
  int i = fclose(sndf->f);
  free(sndf);
  return i;
}
