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
 * $Id: input.c,v 1.2 2002/12/15 15:16:55 menno Exp $
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
  u_int_32 label;           /* 'RIFF' */
  u_int_32 length;        /* Length of rest of file */
  u_int_32 chunk_type;      /* 'WAVE' */
}
riff_t;

typedef struct
{
  u_int_32 label;
  u_int_32 len;
}
riffsub_t;

#define WAVE_FORMAT_PCM 1
typedef struct
{
  u_int_16 wFormatTag;
  u_int_16 nChannels;
  u_int_32 nSamplesPerSec;
  u_int_32 nAvgBytesPerSec;
  u_int_16 nBlockAlign;
  u_int_16 wBitsPerSample;
  u_int_16 cbSize;
}
WAVEFORMATEX;

u_int32_t little32(u_int32_t l32)
{
  unsigned char *s;
  u_int32_t u32;

  s = (unsigned char*)&l32;
  u32 = s[3];
  u32 <<= 8;
  u32 |= s[2];
  u32 <<= 8;
  u32 |= s[1];
  u32 <<= 8;
  u32 |= s[0];
  return u32;
}

u_int16_t little16(u_int16_t l16)
{
  unsigned char *s;
  u_int16_t u16;

  s = (unsigned char*)&l16;
  u16 = s[1];
  u16 <<= 8;
  u16 |= s[0];
  return u16;
}

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
  riffsub.len = little32(riffsub.len);
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
    riffsub.len = little32(riffsub.len);
    if (!memcmp(&(riffsub.label), datal, 4))
      break;
    if (i > 10)
      return NULL;

    for (skip = riffsub.len; skip > 0; skip--)
      fgetc(wave_f);
  }
  if (little16(wave.wFormatTag) != WAVE_FORMAT_PCM)
    return NULL;

  sndf = malloc(sizeof(*sndf));
  sndf->f = wave_f;
  sndf->channels = little16(wave.nChannels);
  sndf->samplebits = little16(wave.wBitsPerSample);
  sndf->samplerate = little32(wave.nSamplesPerSec);
  sndf->samples = riffsub.len /
    (((little16(wave.wBitsPerSample) > 8) ? 2 : 1) * sndf->channels);
  return sndf;
}

size_t wav_read_short(pcmfile_t *sndf, short *buf, size_t num)
{
  int size;
  int i;

  if (sndf->samplebits > 8) {
    size = fread(buf, 2, num, sndf->f);
    for (i = 0; i < size; i++) {
      /* change endianess for big endian (ppc, sparc) machines */
      buf[i] = little16(buf[i]);
    }
    return size;
  }

  /* this is endian clean */
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
