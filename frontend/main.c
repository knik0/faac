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
 * $Id: main.c,v 1.26 2002/08/10 16:07:23 knik Exp $
 */

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __unix__
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <sndfile.h>
#include <getopt.h>

#include <faac.h>


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
    SNDFILE *infile;
    SF_INFO sfinfo;

    unsigned int sr, chan;
    unsigned long samplesInput, maxBytesOutput;

    faacEncConfigurationPtr myFormat;
    unsigned int mpegVersion = MPEG2;
    unsigned int objectType = LOW;
    unsigned int useMidSide = 1;
    unsigned int useTns = 0;
    unsigned int useAdts = 1;
    unsigned int cutOff = 0;
    unsigned long bitRate = 64000;
    int psymodelidx = -1;

    char *audioFileName;
    char *aacFileName;
    char percent[200];

    short *pcmbuf;

    unsigned char *bitbuf;
    int bytesInput = 0;

    FILE *outfile;

#ifdef __unix__
    struct rusage usage;
#endif
#ifdef _WIN32
    long begin, end;
    int nTotSecs, nSecs;
    int nMins;
#else
    float totalSecs;
    int mins;
#endif

    fprintf(stderr, "FAAC - command line demo of %s\n", __DATE__);
    fprintf(stderr, "Uses FAACLIB version: " FAACENC_VERSION "\n\n");

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
            { "bitrate", 1, 0, 'b' }
        };

	c = getopt_long(argc, argv, "m:o:rntc:b:p:",
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
            if (sscanf(optarg, "%u", &i) < 1) {
                cutOff = 18000;
            } else {
                cutOff = i;
            }
            break;
        }
	case 'b':
	  {
            unsigned int i;
                bitRate = 64000;
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
        case '?':
            break;
        default:
            fprintf(stderr, "%s: unknown option specified, ignoring: %c\n",
                progName, c);
        }
    }

    /* check that we have at least two non-option arguments */
    if ((argc - optind) < 2)
    {
      int i;

      // get available psymodels
      hEncoder = faacEncOpen(44100, 2, &samplesInput, &maxBytesOutput);
      myFormat = faacEncGetCurrentConfiguration(hEncoder);

        fprintf(stderr, "USAGE: %s -options infile outfile\n", progName);
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
    fprintf(stderr, "  -b X   Set the bitrate per channel, X in kbps.\n\n");

    faacEncClose(hEncoder);

        return 1;
    }

    /* point to the specified file names */
    audioFileName = argv[optind++];
    aacFileName = argv[optind++];

    /* open the audio input file */
    infile = sf_open_read(audioFileName, &sfinfo);
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
    sr = sfinfo.samplerate;
    chan = sfinfo.channels;

    /* open the encoder library */
    hEncoder = faacEncOpen(sr, chan, &samplesInput, &maxBytesOutput);

    pcmbuf = (short*)malloc(samplesInput*sizeof(short));
    bitbuf = (unsigned char*)malloc(maxBytesOutput*sizeof(unsigned char));

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
#ifdef _WIN32
        begin = GetTickCount();
#endif
        frames = (int)(sfinfo.samples/1024+0.5);
        currentFrame = 0;

        /* encoding loop */
        for ( ;; )
        {
            int bytesWritten;

            currentFrame++;

            bytesInput = sf_read_short(infile, pcmbuf, samplesInput) * sizeof(short);

            /* call the actual encoding routine */
            bytesWritten = faacEncEncode(hEncoder,
                pcmbuf,
                bytesInput/2,
                bitbuf,
                maxBytesOutput);

      if (!(currentFrame & 63))
      {
#ifndef _DEBUG
            sprintf(percent, "%.2f encoding %s.",
                min((double)(currentFrame*100)/frames,100), audioFileName);
            fprintf(stderr, "%s\r", percent);
#endif
#ifdef _WIN32
            SetConsoleTitle(percent);
#endif
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

        /* clean up */
        fclose(outfile);

#ifdef _WIN32
        end = GetTickCount();
        nTotSecs = (end-begin)/1000;
        nMins = nTotSecs / 60;
        nSecs = nTotSecs - (60*nMins);
	fprintf(stderr, "Encoding %s took:\t%.2fsec\n", audioFileName,
		(double)(end - begin) / 1000);
#else
#ifdef __unix__
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            totalSecs=usage.ru_utime.tv_sec;
            mins = totalSecs/60;
            fprintf(stderr, "Encoding %s took: %i min, %.2f sec. of cpu-time\n",
                audioFileName, mins, totalSecs - (60 * mins));
        }
#else
        totalSecs = (float)(clock())/(float)CLOCKS_PER_SEC;
        mins = totalSecs/60;
        fprintf(stderr, "Encoding %s took: %i min, %.2f sec.\n",
            audioFileName, mins, totalSecs - (60 * mins));
#endif
#endif
    }

    faacEncClose(hEncoder);

    sf_close(infile);

    if (pcmbuf) free(pcmbuf);
    if (bitbuf) free(bitbuf);

    return 0;
}
