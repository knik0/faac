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
 * $Id: input.c,v 1.6 2003/06/21 08:58:27 knik Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef HAVE_U_INT32_T
typedef unsigned int u_int32_t;
#endif
#ifndef HAVE_U_INT16_T
typedef unsigned short u_int16_t;
#endif

#ifdef WIN32
#include <fcntl.h>
#endif

#include "input.h"

#ifdef WORDS_BIGENDIAN
# define UINT32(x) (((x & 0xff) << 24) | ((x & 0xff00) << 8) \
	| ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24))
# define UINT16(x) (((x & 0xff) << 8) | ((x & 0xff00) >> 8))
#else
# define UINT32(x) (x)
# define UINT16(x) (x)
#endif

typedef struct
{
  u_int32_t label;           /* 'RIFF' */
  u_int32_t length;        /* Length of rest of file */
  u_int32_t chunk_type;      /* 'WAVE' */
}
riff_t;

typedef struct
{
  u_int32_t label;
  u_int32_t len;
}
riffsub_t;

#define WAVE_FORMAT_PCM 1
typedef struct
{
  u_int16_t wFormatTag;
  u_int16_t nChannels;
  u_int32_t nSamplesPerSec;
  u_int32_t nAvgBytesPerSec;
  u_int16_t nBlockAlign;
  u_int16_t wBitsPerSample;
  u_int16_t cbSize;
}
WAVEFORMATEX;

pcmfile_t *wav_open_read(const char *name,
			 int rawchans, int rawbits, int rawrate)
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
  int dostdin = 0;

  if (!strcmp(name, "-"))
  {
#ifdef WIN32
    setmode(fileno(stdin), O_BINARY);
#endif
    wave_f = stdin;
    dostdin = 1;
  }
  else if (!(wave_f = fopen(name, "rb")))
  {
    perror(name);
    return NULL;
  }

  if (rawchans < 1) // header input
  {
  if (fread(&riff, 1, sizeof(riff), wave_f) != sizeof(riff))
    return NULL;
  if (memcmp(&(riff.label), riffl, 4))
    return NULL;
  if (memcmp(&(riff.chunk_type), wavel, 4))
    return NULL;

  if (fread(&riffsub, 1, sizeof(riffsub), wave_f) != sizeof(riffsub))
    return NULL;
  riffsub.len = UINT32(riffsub.len);
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
    riffsub.len = UINT32(riffsub.len);
    if (!memcmp(&(riffsub.label), datal, 4))
      break;
    if (i > 10)
      return NULL;

    for (skip = riffsub.len; skip > 0; skip--)
      fgetc(wave_f);
  }
  if (UINT16(wave.wFormatTag) != WAVE_FORMAT_PCM)
    return NULL;
  }

  sndf = malloc(sizeof(*sndf));
  sndf->f = wave_f;
  if (rawchans > 0) // raw input
  {
    sndf->bigendian = 1;
    sndf->channels = rawchans;
    sndf->samplebits = rawbits;
    sndf->samplerate = rawrate;
    if (dostdin)
      sndf->samples = 0;
    else
    {
      fseek(sndf->f, 0 , SEEK_END);
      sndf->samples = ftell(sndf->f) /
	(((sndf->samplebits > 8) ? 2 : 1) * sndf->channels);
      rewind(sndf->f);
    }
  }
  else
  {
    sndf->bigendian = 0;
  sndf->channels = UINT16(wave.nChannels);
  sndf->samplebits = UINT16(wave.wBitsPerSample);
  sndf->samplerate = UINT32(wave.nSamplesPerSec);
  sndf->samples = riffsub.len /
    (((UINT16(wave.wBitsPerSample) > 8) ? 2 : 1) * sndf->channels);
  }
  return sndf;
}

size_t wav_read_short(pcmfile_t *sndf, short *buf, size_t num)
{
  int size;
  int i;

  if (sndf->samplebits > 8) {
    size = fread(buf, 2, num, sndf->f);
    // fix endianness
#ifdef WORDS_BIGENDIAN
    if (!sndf->bigendian)
#else
    if (sndf->bigendian)
#endif
      // swap bytes
      for (i = 0; i < size; i++)
      {
	int s = buf[i];
	buf[i] = ((s & 0xff) << 8) | ((s & 0xff00) >> 8);
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
