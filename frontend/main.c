/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
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
 * $Id: main.c,v 1.39 2003/07/21 16:33:49 knik Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <windows.h>
#include <fcntl.h>
#endif

#ifdef __unix__
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# include "getopt.h"
# include "getopt.c"
#endif

#include "input.h"

#include <faac.h>

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/* globals */
char* progName;


static int *mkChanMap(int channels, int center, int lf)
{
  int *map;
  int inpos;
  int outpos;

  if (!center && !lf)
    return NULL;

  if (channels < 3)
    return NULL;

  if (lf > 0)
    lf--;
  else
    lf = channels - 1; // default AAC position

  if (center > 0)
    center--;
  else
    center = 0; // default AAC position

  map = malloc(channels * sizeof(map[0]));
  memset(map, 0, channels * sizeof(map[0]));

  outpos = 0;
  if ((center >= 0) && (center < channels))
    map[outpos++] = center;

  inpos = 0;
  for (; outpos < (channels - 1); inpos++)
  {
    if (inpos == center)
      continue;
    if (inpos == lf)
      continue;

    map[outpos++] = inpos;
  }
  if (outpos < channels)
  {
    if ((lf >= 0) && (lf < channels))
      map[outpos] = lf;
    else
      map[outpos] = inpos;
  }

  return map;
}

int main(int argc, char *argv[])
{
    int frames, currentFrame;
    faacEncHandle hEncoder;
    pcmfile_t *infile = NULL;

    unsigned long samplesInput, maxBytesOutput;

    faacEncConfigurationPtr myFormat;
    unsigned int mpegVersion = MPEG2;
    unsigned int objectType = LOW;
    unsigned int useMidSide = 1;
    static unsigned int useTns = 0;
    unsigned int useAdts = 1;
    int cutOff = -1;
    int bitRate = 0;
    unsigned long quantqual = 0;
    int chanC = 3;
    int chanLF = 4;

    char *audioFileName;
    char *aacFileName;

    int32_t *pcmbuf;
    int *chanmap = NULL;

    unsigned char *bitbuf;
    int samplesRead = 0;
    int dieUsage = 0;

    int rawChans = 0; // disabled by default
    int rawBits = 16;
    int rawRate = 44100;

    FILE *outfile;

    // get faac version
    hEncoder = faacEncOpen(44100, 2, &samplesInput, &maxBytesOutput);
    myFormat = faacEncGetCurrentConfiguration(hEncoder);
    if (myFormat->version == FAAC_CFG_VERSION)
    {
      fprintf(stderr, "%s(see the faac.html file for more details)\n\n",
	     myFormat->copyright);
      fprintf(stderr, "libfaac version %s\n", myFormat->name);
      faacEncClose(hEncoder);
    }
    else
    {
      fprintf(stderr, __FILE__ "(%d): wrong libfaac version\n", __LINE__);
      faacEncClose(hEncoder);
      return 1;
    }

    /* begin process command line */
    progName = argv[0];
    while (1) {
        static struct option long_options[] = {
            { "mpeg", 0, 0, 'm' },
            { "objecttype", 0, 0, 'o' },
            { "raw", 0, 0, 'r' },
            { "nomidside", 0, 0, 'n' },
            { "tns", 0, &useTns, 1 },
            { "notns", 0, &useTns, 0 },
            { "cutoff", 1, 0, 'c' },
            { "quality", 1, 0, 'q' },
            { "pcmraw", 0, 0, 'P'},
            { "pcmsamplerate", 1, 0, 'R'},
            { "pcmsamplebits", 1, 0, 'B'},
            { "pcmchannels", 1, 0, 'C'},
            { 0, 0, 0, 0}
        };
      int c = -1;
      int option_index = 0;

      c = getopt_long(argc, argv, "a:m:o:rnc:q:PR:B:C:I:",
            long_options, &option_index);

        if (c == -1)
            break;

	if (!c)
	  continue;

        switch (c) {
	case 'm':
	  mpegVersion = atoi(optarg);
	  switch(mpegVersion)
	  {
	  case 2:
	    mpegVersion = MPEG2;
	    break;
	  case 4:
	    mpegVersion = MPEG4;
	    break;
	  default:
	    mpegVersion = MPEG4;
	  }
	  break;
        case 'o':
	  objectType = atoi(optarg);
	  switch (objectType)
	  {
	  case 1:
	    objectType = MAIN;
	    break;
	  case 2:
	    objectType = LTP;
	    break;
	  default:
	    objectType = LOW;
	    break;
	  }
	  break;
        case 'r': {
            useAdts = 0;
            break;
        }
        case 'n': {
            useMidSide = 0;
            break;
        }
        case 'c': {
            unsigned int i;
        if (sscanf(optarg, "%u", &i) > 0) {
                cutOff = i;
            }
            break;
        }
        case 'a': {
	  unsigned int i;
	  if (sscanf(optarg, "%u", &i) > 0)
	  {
	    bitRate = 1000 * i;
	  }
	  break;
        }
    case 'q':
      {
            unsigned int i;
        if (sscanf(optarg, "%u", &i) > 0)
        {
          if (i > 0 && i < 1000)
	    quantqual = i;
            }
            break;
        }
    case 'I':
      sscanf(optarg, "%d,%d", &chanC, &chanLF);
      break;
   case 'P':
	  rawChans = 2; // enable raw input
       break;
   case 'R':
     {
       unsigned int i;
       if (sscanf(optarg, "%u", &i) > 0)
	    {
	      rawRate = i;
	      rawChans = (rawChans > 0) ? rawChans : 2;
	    }
       break;
       }
   case 'B':
     {
       unsigned int i;
       if (sscanf(optarg, "%u", &i) > 0)
	    {
	      if (i > 32)
                i = 32;
	      if (i < 8)
		i = 8;
	      rawBits = i;
	      rawChans = (rawChans > 0) ? rawChans : 2;
	    }
       break;
       }
   case 'C':
     {
       unsigned int i;
       if (sscanf(optarg, "%u", &i) > 0)
	      rawChans = i;
       break;
       }
        case '?':
            break;
        default:
            fprintf(stderr, "%s: unknown option specified, ignoring: %c\n",
                progName, c);
        }
    }

    /* check that we have at least two non-option arguments */
    if ((argc - optind) < 2 || dieUsage == 1)
    {
	printf("\nUsage: %s -options infile outfile\n", progName);
	printf("Options:\n");
	printf("  -a <x>\tSet average bitrate to approximately x kbps/channel.\n");
	printf("  -c <bandwidth>\tSet the bandwidth in Hz. (default=automatic)\n");
	printf("  -q <quality>\tSet quantizer quality.\n");
	printf("  --tns  \tEnable TNS coding.\n");
	printf("  --notns\tDisable TNS coding.\n");
	printf("  -n     Don\'t use mid/side coding.\n");
	printf("  -m X   AAC MPEG version, X can be 2 or 4.\n");
	printf("  -o X   AAC object type. (0=Low Complexity (default), 1=Main, 2=LTP)\n");
	printf("  -r     RAW AAC output file.\n");
	printf("  -P     Raw PCM input mode (default 44100Hz 16bit stereo).\n");
	printf("  -R     Raw PCM input rate.\n");
	printf("  -B     Raw PCM input sample size (16 default or 8bits).\n");
	printf("  -C     Raw PCM input channels.\n");
	printf("  -I <C,LF> Input channel config, default is 3,4 (Center third, LF fourth)\n");
	printf("More details on FAAC usage can be found in the faac.html file.\n");

        return 1;
    }

    /* point to the specified file names */
    audioFileName = argv[optind++];
    aacFileName = argv[optind++];

    /* open the audio input file */
    if (rawChans > 0) // use raw input
    {
      infile = wav_open_read(audioFileName, 1);
      if (infile)
      {
	infile->channels = rawChans;
	infile->samplebytes = rawBits / 8;
	infile->samplerate = rawRate;
      }
    }
    else // header input
      infile = wav_open_read(audioFileName, 0);

    if (infile == NULL)
    {
        fprintf(stderr, "Couldn't open input file %s\n", audioFileName);
        return 1;
    }

    /* open the aac output file */
    outfile = fopen(aacFileName, "wb");
    if (!outfile)
    {
        fprintf(stderr, "Couldn't create output file %s\n", aacFileName);
        return 1;
    }

    /* open the encoder library */
    hEncoder = faacEncOpen(infile->samplerate, infile->channels,
			   &samplesInput, &maxBytesOutput);

    pcmbuf = (int32_t *)malloc(samplesInput*sizeof(int32_t));
    bitbuf = (unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char));
    chanmap = mkChanMap(infile->channels, chanC, chanLF);
    if (chanmap)
    {
      fprintf(stderr, "Remapping input channels: Center=%d, LFE=%d\n",
	      chanC, chanLF);
    }

    if (cutOff <= 0)
    {
      if (cutOff < 0) // default
	cutOff = 0;
      else // disabled
    cutOff = infile->samplerate / 2;
    }
    if (cutOff > (infile->samplerate / 2))
      cutOff = infile->samplerate / 2;

    /* put the options in the configuration struct */
    myFormat = faacEncGetCurrentConfiguration(hEncoder);
    myFormat->aacObjectType = objectType;
    myFormat->mpegVersion = mpegVersion;
    myFormat->useTns = useTns;
    myFormat->allowMidside = useMidSide;
    if (bitRate)
      myFormat->bitRate = bitRate;
    myFormat->bandWidth = cutOff;
    if (quantqual > 0)
      myFormat->quantqual = quantqual;
    myFormat->outputFormat = useAdts;
    if (!faacEncSetConfiguration(hEncoder, myFormat)) {
        fprintf(stderr, "Unsupported output format!\n");
        return 1;
    }

    cutOff = myFormat->bandWidth;
    quantqual = myFormat->quantqual;
    bitRate = myFormat->bitRate;
    if (bitRate)
      fprintf(stderr, "Approximate ABR: %d kbps/channel\n", bitRate/1000);
    fprintf(stderr, "Quantization quality: %ld\n", quantqual);
    fprintf(stderr, "Bandwidth: %d Hz\n", cutOff);
    fprintf(stderr, "Object type: ");
    switch(objectType)
    {
    case LOW:
      fprintf(stderr, "Low Complexity");
      break;
    case MAIN:
      fprintf(stderr, "Main");
      break;
    case LTP:
      fprintf(stderr, "LTP");
      break;
    }
    fprintf(stderr, "(MPEG-%d)", (mpegVersion == MPEG4) ? 4 : 2);
    if (myFormat->useTns)
      fprintf(stderr, " + TNS");
    if (myFormat->allowMidside)
      fprintf(stderr, " + M/S");
    fprintf(stderr, "\n");

    if (outfile)
    {
    int showcnt = 0;
#ifdef _WIN32
    long begin = GetTickCount();
#endif
   if (infile->samples)
     frames = (int)infile->samples / 1024 + 2;
   else
     frames = 0;
        currentFrame = 0;

    fprintf(stderr, "Encoding %s to %s\n", audioFileName, aacFileName);
   if (frames != 0)
     fprintf(stderr,
        "   frame          | elapsed/estim | play/CPU | ETA\n");
   else
     fprintf(stderr,
           " frame | elapsed | play/CPU\n");
        /* encoding loop */
        for ( ;; )
        {
            int bytesWritten;

	    samplesRead = wav_read_int24(infile, pcmbuf, samplesInput, chanmap);

            /* call the actual encoding routine */
            bytesWritten = faacEncEncode(hEncoder,
                pcmbuf,
                samplesRead,
                bitbuf,
                maxBytesOutput);

        if (bytesWritten)
      {
          currentFrame++;
          showcnt--;
        }

        if ((showcnt <= 0) || !bytesWritten)
        {
          double timeused;
#ifdef __unix__
          struct rusage usage;
#endif
#ifdef _WIN32
          char percent[50];
          timeused = (GetTickCount() - begin) * 1e-3;
#else
#ifdef __unix__
          if (getrusage(RUSAGE_SELF, &usage) == 0) {
        timeused = (double)usage.ru_utime.tv_sec +
          (double)usage.ru_utime.tv_usec * 1e-6;
          }
          else
                timeused = 0;
#else
          timeused = (double)clock() * (1.0 / CLOCKS_PER_SEC);
#endif
#endif
          if (currentFrame && (timeused > 0.1))
          {

        showcnt += 50;

       if (frames != 0)
         fprintf(stderr,
            "\r%5d/%-5d (%3d%%)| %6.1f/%-6.1f | %8.3f | %.1f ",
            currentFrame, frames, currentFrame*100/frames,
            timeused,
            timeused * frames / currentFrame,
            (1024.0 * currentFrame / infile->samplerate) / timeused,
            timeused  * (frames - currentFrame) / currentFrame);
       else
         fprintf(stderr,
           "\r %5d |  %6.1f | %8.3f ",
           currentFrame,
           timeused,
           (1024.0 * currentFrame / infile->samplerate) / timeused);
        fflush(stderr);
#ifdef _WIN32
       if (frames != 0)
       {
         sprintf(percent, "%.2f%% encoding %s",
            100.0 * currentFrame / frames, audioFileName);
            SetConsoleTitle(percent);
       }
#endif
          }
      }

            /* all done, bail out */
            if (!samplesRead && !bytesWritten)
                break ;

            if (bytesWritten < 0)
            {
                fprintf(stderr, "faacEncEncode() failed\n");
                break ;
            }

            /* write bitstream to aac file */
            fwrite(bitbuf, 1, bytesWritten, outfile);
        }
    fprintf(stderr, "\n\n");

        /* clean up */
        fclose(outfile);
    }

    faacEncClose(hEncoder);

    wav_close(infile);

    if (pcmbuf) free(pcmbuf);
    if (bitbuf) free(bitbuf);

    return 0;
}

/*
$Log: main.c,v $
Revision 1.39  2003/07/21 16:33:49  knik
Fixed LFE channel mapping.

Revision 1.38  2003/07/13 08:34:43  knik
Fixed -o, -m and -I option.
Print object type setting.

Revision 1.37  2003/07/10 19:19:32  knik
Input channel remapping and 24-bit support.

Revision 1.36  2003/06/26 19:40:53  knik
TNS disabled by default.
Copyright info moved to library.
Print help to standard output.

Revision 1.35  2003/06/21 08:59:31  knik
raw input support moved to input.c

Revision 1.34  2003/05/10 09:40:35  knik
added approximate ABR option

Revision 1.33  2003/04/13 08:39:28  knik
removed psymodel setting

Revision 1.32  2003/03/27 17:11:06  knik
updated library interface
-b bitrate option replaced with -q quality option
TNS enabled by default

Revision 1.31  2002/12/23 19:02:43  knik
added some headers

Revision 1.30  2002/12/15 15:16:55  menno
Some portability changes

Revision 1.29  2002/11/23 17:34:59  knik
replaced libsndfile with input.c
improved bandwidth/bitrate calculation formula

Revision 1.28  2002/08/30 16:20:45  knik
misplaced #endif

Revision 1.27  2002/08/19 16:33:54  knik
automatic bitrate setting
more advanced status line

*/
