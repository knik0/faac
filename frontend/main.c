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
 * $Id: main.c,v 1.31 2002/12/23 19:02:43 knik Exp $
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

#include <faac.h>

#include "input.h"

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/* globals */
char* progName;

int StringCompI(char const *str1, char const *str2, unsigned long len)
{
    signed int c1 = 0, c2 = 0;

    while (len--) {
        c1 = tolower(*str1++);
        c2 = tolower(*str2++);

        if (c1 == 0 || c1 != c2)
            break;
    }

    return c1 - c2;
}

int main(int argc, char *argv[])
{
    int frames, currentFrame;
    faacEncHandle hEncoder;
    pcmfile_t *infile = NULL;

    unsigned int sr, chan;
    unsigned long samplesInput, maxBytesOutput;

    faacEncConfigurationPtr myFormat;
    unsigned int mpegVersion = MPEG2;
    unsigned int objectType = LOW;
    unsigned int useMidSide = 1;
    unsigned int useTns = 0;
    unsigned int useAdts = 1;
    int cutOff = -1;
    unsigned long bitRate = 0;
    int psymodelidx = -1;
    const double bwbrfac = 1.3;
    const int bwdefault = 16000;

    char *audioFileName;
    char *aacFileName;

    short *pcmbuf;

    unsigned char *bitbuf;
    int bytesInput = 0;
    int rawInput = 0;
    int dieUsage = 0;

    FILE *outfile;

    fprintf(stderr, "FAAC version " FAACENC_VERSION " (" __DATE__ ")\n");

    /* begin process command line */
    progName = argv[0];
    while (1) {
        int c = -1;
        int option_index = 0;
        static struct option long_options[] = {
            { "mpeg", 0, 0, 'm' },
            { "objecttype", 0, 0, 'o' },
            { "raw", 0, 0, 'r' },
            { "nomidside", 0, 0, 'n' },
            { "usetns", 0, 0, 't' },
            { "cutoff", 1, 0, 'c' },
            { "bitrate", 1, 0, 'b' },
            { "acousticmodel", 1, 0, 'p'},
            { "pcmraw", 0, 0, 'P'},
            { "pcmsamplerate", 1, 0, 'R'},
            { "pcmsamplebits", 1, 0, 'B'},
            { "pcmchannels", 1, 0, 'C'}
        };

    c = getopt_long(argc, argv, "m:o:rntc:b:p:PR:B:C:",
            long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case 'm': {
            unsigned int i;
            if (optarg) {
                if (sscanf(optarg, "%u", &i) < 1) {
                    mpegVersion = MPEG2;
                } else {
                    if (i == 2)
                        mpegVersion = MPEG2;
                    else
                        mpegVersion = MPEG4;
                }
            } else {
                mpegVersion = MPEG2;
            }
            break;
        }
        case 'o': {
            unsigned char i[10];
            if (optarg) {
                if (sscanf(optarg, "%s", i) < 1) {
                    objectType = LOW;
                } else {
                    if (StringCompI(i, "MAIN", 4) == 0)
                        objectType = MAIN;
                    else if (StringCompI(i, "LTP", 3) == 0)
                        objectType = LTP;
                    else
                        objectType = LOW;
                }
            } else {
                objectType = LOW;
            }
            break;
        }
        case 'r': {
            useAdts = 0;
            break;
        }
        case 'n': {
            useMidSide = 0;
            break;
        }
        case 't': {
            useTns = 1;
            break;
        }
        case 'c': {
            unsigned int i;
        if (sscanf(optarg, "%u", &i) > 0) {
                cutOff = i;
            }
            break;
        }
    case 'b':
      {
            unsigned int i;
        if (sscanf(optarg, "%u", &i) > 0)
        {
          if (i > 0 && i < 1000)
        bitRate = i * 1000;
            }
            break;
        }
    case 'p':
      {
        unsigned int i;
        if (sscanf(optarg, "%u", &i) > 0)
          psymodelidx = i;
      break;
      }
   case 'P':
     {
       rawInput = 1;
       infile = malloc(sizeof(*infile));
       if (infile == NULL) {
              fprintf(stderr, "%s: unable to allocate memory\n", progName);
         return 1;
       }
       infile->f = NULL;
       infile->channels = 2;
       infile->samplerate = 44100;
       infile->samplebits = 16;
       infile->samples = 0;
       break;
       }
   case 'R':
     {
       unsigned int i;
       if (rawInput != 1) {
         fprintf(stderr, "%s: for raw pcm input -P needs to be specified first\n",
         progName);
         dieUsage = 1;
         break;
      }
       if (sscanf(optarg, "%u", &i) > 0)
         infile->samplerate = i;
       break;
       }
   case 'B':
     {
       unsigned int i;
       if (rawInput != 1) {
         fprintf(stderr, "%s: for raw pcm input -P needs to be specified first\n",
         progName);
         dieUsage = 1;
         break;
       }
       if (sscanf(optarg, "%u", &i) > 0)
         infile->samplebits = i;
       break;
       }
   case 'C':
     {
       unsigned int i;
       if (rawInput != 1) {
         fprintf(stderr, "%s: for raw pcm input -P needs to be specified first\n",
         progName);
         dieUsage = 1;
         break;
       }
       if (sscanf(optarg, "%u", &i) > 0)
         infile->channels = i;
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
      int i;

      // get available psymodels
      hEncoder = faacEncOpen(44100, 2, &samplesInput, &maxBytesOutput);
      myFormat = faacEncGetCurrentConfiguration(hEncoder);

        fprintf(stderr, "\nUsage: %s -options infile outfile\n", progName);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -m X   AAC MPEG version, X can be 2 or 4.\n");
        fprintf(stderr, "  -o X   AAC object type, X can be LC, MAIN or LTP.\n");
    for (i = 0; myFormat->psymodellist[i].ptr; i++)
    {
      fprintf(stderr, "  -p %d   Use %s.%s\n", i,
          myFormat->psymodellist[i].name,
          (i == myFormat->psymodelidx) ? " (default)" : "");
    }
        fprintf(stderr, "  -n     Don\'t use mid/side coding.\n");
        fprintf(stderr, "  -r     RAW AAC output file.\n");
        fprintf(stderr, "  -t     Use TNS coding.\n");
        fprintf(stderr,
            "  -c X   Set the bandwidth, X in Hz. (default=automatic)\n");
        fprintf(stderr, "  -b X   Set the bitrate per channel, X in kbps."
            " (default is auto)\n");
        fprintf(stderr, "  -P     Raw PCM input mode (default 44100Hz 16bit stereo).\n");
        fprintf(stderr, "  -R     Raw PCM input rate.\n");
        fprintf(stderr, "  -B     Raw PCM input sample size (16 default or 8bits).\n");
        fprintf(stderr, "  -C     Raw PCM input channels.\n\n");

        faacEncClose(hEncoder);

        return 1;
    }

    /* point to the specified file names */
    audioFileName = argv[optind++];
    aacFileName = argv[optind++];

    /* open the audio input file */
    if (rawInput == 1) {
      if (!strcmp(audioFileName, "-")) {
#ifdef WIN32
   setmode(fileno(stdin), O_BINARY);
#endif
   infile->f = stdin;
   infile->samples = 0;
      }
      else {
   if (!(infile->f = fopen(audioFileName, "rb")))
        {
          fprintf(stderr, "Couldn't open input file %s\n", audioFileName);
     perror("Reason");
     return 1;
        }
   fseek(infile->f, 0 , SEEK_END);
   infile->samples = ftell(infile->f) /
     (((infile->samplebits > 8) ? 2 : 1) * infile->channels);
   rewind(infile->f);
      }
    } else {
      infile = wav_open_read(audioFileName);
    }
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

    /* determine input file parameters */
    sr = infile->samplerate;
    chan = infile->channels;

    /* open the encoder library */
    hEncoder = faacEncOpen(sr, chan, &samplesInput, &maxBytesOutput);

    pcmbuf = (short*)malloc(samplesInput*sizeof(short));
    bitbuf = (unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char));

    if (!bitRate)
    {
      bitRate = pow((double)(sr / 2) / bwdefault, 1.0 / bwbrfac) * 64000;
      bitRate = ((bitRate + 500) / 500) * 500;
      if (bitRate > 64000)
    bitRate = 64000;
    }
    if (cutOff <= 0)
    {
      if (cutOff < 0) // default
    cutOff = pow((double)bitRate / 64000, bwbrfac) * bwdefault;
      else // disabled
    cutOff = sr / 2;
    }
    if (cutOff > (sr / 2))
      cutOff = sr / 2;
    fprintf(stderr, "Bit rate: %ld bps per channel\n", bitRate);
    fprintf(stderr, "Cutoff frequency is ");
    if (cutOff == sr / 2)
      fprintf(stderr, "disabled\n");
    else
      fprintf(stderr, "%d Hz\n", cutOff);

    /* put the options in the configuration struct */
    myFormat = faacEncGetCurrentConfiguration(hEncoder);
    myFormat->aacObjectType = objectType;
    myFormat->mpegVersion = mpegVersion;
    myFormat->useTns = useTns;
    myFormat->allowMidside = useMidSide;
    myFormat->bitRate = bitRate;
    myFormat->bandWidth = cutOff;
    myFormat->outputFormat = useAdts;
    if (psymodelidx >= 0)
      myFormat->psymodelidx = psymodelidx;
    if (!faacEncSetConfiguration(hEncoder, myFormat)) {
        fprintf(stderr, "Unsupported output format!\n");
        return 1;
    }

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

    fprintf(stderr, "Encoding %s\n", audioFileName);
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

            bytesInput = wav_read_short(infile, pcmbuf, samplesInput) * sizeof(short);

            /* call the actual encoding routine */
            bytesWritten = faacEncEncode(hEncoder,
                pcmbuf,
                bytesInput/2,
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
            (1024.0 * currentFrame / sr) / timeused,
            timeused  * (frames - currentFrame) / currentFrame);
       else
         fprintf(stderr,
           "\r %5d |  %6.1f | %8.3f ",
           currentFrame,
           timeused,
           (1024.0 * currentFrame / sr) / timeused);
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
            if (!bytesInput && !bytesWritten)
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
