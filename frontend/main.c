/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 * Copyright (C) 2002-2017 Krzysztof Nikiel
 * Copyright (C) 2004 Dan Villiom P. Christiansen
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
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#else
#include <signal.h>
#endif

/* the BSD derivatives don't define __unix__ */
#if defined(__APPLE__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
#define __unix__
#endif

#ifdef __unix__
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# include "getopt.h"
# include "getopt.c"
#endif

#include "mp4write.h"

#if !defined(HAVE_STRCASECMP) && !defined(_WIN32)
# define strcasecmp strcmp
#endif

#ifdef _WIN32
# undef stderr
# define stderr stdout
#endif

#include "input.h"

#include <faac.h>

#define FALSE 0
#define TRUE 1

enum flags
{
    SHORTCTL_FLAG = 300,
    MPEGVERS_FLAG,
    ARTIST_FLAG,
    TITLE_FLAG,
    GENRE_FLAG,
    ALBUM_FLAG,
    TRACK_FLAG,
    DISC_FLAG,
    YEAR_FLAG,
    COVER_ART_FLAG,
    COMMENT_FLAG,
    WRITER_FLAG,
    TAG_FLAG,
    HELP_QUAL,
    HELP_IO,
    HELP_MP4,
    HELP_ADVANCED,
    OPT_JOINT,
    OPT_PNS
};

typedef struct {
    char *shorthelp;
    char *longhelp;
} help_t;

const char *usage =
    "Usage: %s [options] infile\n\n";

static help_t help_qual[] = {
    {"-q <quality>\tSet encoding quality.\n",
    "\t\tSet default variable bitrate (VBR) quantizer quality in percent.\n"
    "\t\tmax. 5000, min. 10.\n"
    "\t\tdefault: 100, averages at approx. 120 kbps VBR for a normal\n"
    "\t\tstereo input file with 16 bit and 44.1 kHz sample rate\n"
    },
    {"-b <bitrate>\tSet average bitrate to x kbps. (ABR)\n",
    "\t\tSet average bitrate (ABR) to approximately <bitrate> kbps.\n"
    "\t\tmax. ~500 (stereo)\n"},
    {"-c <freq>\tSet the bandwidth in Hz.\n",
    "\t\tThe actual frequency is adjusted to maximize upper spectral band\n"
    "\t\tusage.\n"},
    {0}
};

static help_t help_io[] = {
    {"-o <filename>\tSet output file to X (only for one input file)\n",
    "\t\tonly for one input file; you can use *.aac, *.mp4, *.m4a or\n"
    "\t\t*.m4b as file extension, and the file format will be set\n"
    "\t\tautomatically to ADTS or MP4).\n"},
    {"-\t\tUse stdin/stdout\n",
    "\t\tIf you simply use a hyphen/minus sign instead\n"
    "\t\tof a filename, FAAC can encode directly from stdin,\n"
    "\t\tthus enabling piping from other applications and utilities. The\n"
    "\t\tsame works for stdout as well, so FAAC can pipe its output to\n"
    "\t\tother apps such as a server.\n"},
    {"-v <verbose>\t\tverbosity level (-v0 is quiet mode)\n"},
    {"-r\t\tUse RAW AAC output file.\n",
    "\t\tGenerate raw AAC bitstream (i.e. without any headers).\n"
    "\t\tNot advised!!!, RAW AAC files are practically useless!!!\n"},
    {"-P\t\tRaw PCM input mode (default 44100Hz 16bit stereo).\n",
    "\t\tRaw PCM input mode (default: off, i.e. expecting a WAV header;\n"
    "\t\tnecessary for input files or bitstreams without a header; using\n"
    "\t\tonly -P assumes the default values for -R, -B and -C in the\n"
    "\t\tinput file).\n"},
    {"-R <samplerate>\tRaw PCM input rate.\n",
    "\t\tRaw PCM input sample rate in Hz (default: 44100 Hz, max. 96 kHz)\n"},
    {"-B <samplebits>\tRaw PCM input sample size (8, 16 (default), 24 or 32bits).\n",
    "\t\tRaw PCM input sample size (default: 16, also possible 8, 24, 32\n"
    "\t\tbit fixed or float input).\n"},
    {"-C <channels>\tRaw PCM input channels.\n",
    "\t\tRaw PCM input channels (default: 2, max. 33 + 1 LFE).\n"},
    {"-X\t\tRaw PCM swap input bytes\n",
    "\t\tRaw PCM swap input bytes (default: bigendian).\n"},
    {"-I <C[,LFE]>\tInput channel config, default is 3,4 (Center third, LF fourth)\n",
    "\t\tInput multichannel configuration (default: 3,4 which means\n"
    "\t\tCenter is third and LFE is fourth like in 5.1 WAV, so you only\n"
    "\t\thave to specify a different position of these two mono channels\n"
    "\t\tin your multichannel input files if they haven't been reordered\n"
    "\t\talready).\n"},
    {"--ignorelength\tIgnore wav length from header (useful with files over 4 GB)\n"},
    {"--overwrite\t\tOverwrite existing output file"},
    {0}
};

static help_t help_mp4[] = {
    {"-w\t\tWrap AAC data in MP4 container. (default for *.mp4 and *.m4a)\n",
    "\t\tWrap AAC data in MP4 container. (default for *.mp4, *.m4a and\n"
    "\t\t*.m4b)\n"},
    {"--tag <tagname,tagvalue> Add named tag (iTunes '----')\n"},
    {"--artist <name>\tSet artist name\n"},
    {"--composer <name>\tSet composer name\n"},
    {"--title <name>\tSet title/track name\n"},
    {"--genre <number>\tSet genre number\n"},
    {"--album <name>\tSet album/performer\n"},
    {"--compilation\tMark as compilation\n"},
    {"--track <number/total>\tSet track number\n"},
    {"--disc <number/total>\tSet disc number\n"},
    {"--year <number>\tSet year\n"},
    {"--cover-art <filename>\tRead cover art from file X\n",
    "\t\tSupported image formats are GIF, JPEG, and PNG.\n"},
    {"--comment <string>\tSet comment\n"},
    {0}
};

static help_t help_advanced[] = {
    {"--tns  \tEnable coding of TNS, temporal noise shaping.\n"},
    {"--no-tns\tDisable coding of TNS, temporal noise shaping.\n"},
    {"--joint 0\tDisable joint stereo coding.\n"},
    {"--joint 1\tUse Mid/Side coding.\n"},
    {"--joint 2\tUse Intensity Stereo coding.\n"},
    {"--pns <0 .. 10>\tPNS level; 0=disabled.\n"},
    {"--mpeg-vers X\tForce AAC MPEG version, X can be 2 or 4\n"},
    {"--shortctl X\tEnforce block type (0 = both (default); 1 = no short; 2 = no\n"
    "\t\tlong).\n"},
    {0}
};

static struct {
    int id;
    char *name;
    char *option;
    help_t *help;
} g_help[] = {
    {HELP_QUAL, "Quality-related options", "--help-qual", help_qual},
    {HELP_IO, "Input/output options", "--help-io", help_io},
    {HELP_MP4, "MP4 specific options", "--help-mp4", help_mp4},
    {HELP_ADVANCED, "Advanced options, only for testing purposes", "--help-advanced", help_advanced},
    {0}
};

char *license =
    "\nPlease note that the use of this software may require the payment of patent\n"
    "royalties. You need to consider this issue before you start building derivative\n"
    "works. We are not warranting or indemnifying you in any way for patent\n"
    "royalities! YOU ARE SOLELY RESPONSIBLE FOR YOUR OWN ACTIONS!\n"
    "\n"
    "FAAC is based on the ISO MPEG-4 reference code. For this code base the\n"
    "following license applies:\n"
    "\n"
    "This software module was originally developed by\n"
    "\n"
    "FirstName LastName (CompanyName)\n"
    "\n"
    "and edited by\n"
    "\n"
    "FirstName LastName (CompanyName)\n"
    "FirstName LastName (CompanyName)\n"
    "\n"
    "in the course of development of the MPEG-2 NBC/MPEG-4 Audio standard\n"
    "ISO/IEC 13818-7, 14496-1,2 and 3. This software module is an\n"
    "implementation of a part of one or more MPEG-2 NBC/MPEG-4 Audio tools\n"
    "as specified by the MPEG-2 NBC/MPEG-4 Audio standard. ISO/IEC gives\n"
    "users of the MPEG-2 NBC/MPEG-4 Audio standards free license to this\n"
    "software module or modifications thereof for use in hardware or\n"
    "software products claiming conformance to the MPEG-2 NBC/ MPEG-4 Audio\n"
    "standards. Those intending to use this software module in hardware or\n"
    "software products are advised that this use may infringe existing\n"
    "patents. The original developer of this software module and his/her\n"
    "company, the subsequent editors and their companies, and ISO/IEC have\n"
    "no liability for use of this software module or modifications thereof\n"
    "in an implementation. Copyright is not released for non MPEG-2\n"
    "NBC/MPEG-4 Audio conforming products. The original developer retains\n"
    "full right to use the code for his/her own purpose, assign or donate\n"
    "the code to a third party and to inhibit third party from using the\n"
    "code for non MPEG-2 NBC/MPEG-4 Audio conforming products. This\n"
    "copyright notice must be included in all copies or derivative works.\n"
    "\n"
    "Copyright (c) 1997.\n"
    "\n"
    "For the changes made for the FAAC project the GNU Lesser General Public\n"
    "License (LGPL), version 2 1991 applies:\n"
    "\n"
    "FAAC - Freeware Advanced Audio Coder\n"
    "Copyright (C) 2001-2004 The individual contributors\n"
    "\n"
    "This library is free software; you can redistribute it and/or\n"
    "modify it under the terms of the GNU Lesser General Public\n"
    "License as published by the Free Software Foundation; either\n"
    "version 2.1 of the License, or (at your option) any later version.\n"
    "\n"
    "This library is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
    "Lesser General Public License for more details.\n"
    "\n"
    "You should have received a copy of the GNU Lesser General Public\n"
    "License along with this library; if not, write to the Free Software\n"
    "Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
    "\n";

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

/* globals */
char *progName;
#ifndef _WIN32
volatile int running = 1;
#endif

enum container_format
{
    NO_CONTAINER,
    MP4_CONTAINER,
};

#ifndef _WIN32
void signal_handler(int signal)
{
    running = 0;
}
#endif

static void help0(help_t *h, int l)
{
    int cnt;

    for (cnt = 0; h[cnt].shorthelp; cnt++)
    {
        printf("    %s", h[cnt].shorthelp);
        if (l && h[cnt].longhelp)
            printf("%s", h[cnt].longhelp);
    }
    printf("\n\n");
}

static void help(int mode)
{
    int cnt;
    static const char *name = "faac";

    printf(usage, name);
    switch (mode)
    {
    case '?':
    case 'h':
    case 'H':
        printf("Help options:\n"
                "\t-h\t\tShort help on using FAAC\n"
                "\t-H\t\tDescription of all options for FAAC.\n"
                "\t--license\tLicense terms for FAAC.\n");
        for (cnt = 0; g_help[cnt].id; cnt++)
            printf("\t%s\t%s\n", g_help[cnt].option, g_help[cnt].name);
        if (mode == 'h')
        {
        for (cnt = 0; cnt < 2; cnt++)
        {
            printf("%s:\n", g_help[cnt].name);
            help0(g_help[cnt].help, 0);
        }
        }
        if (mode == 'H')
        {
        for (cnt = 0; cnt < g_help[cnt].id; cnt++)
        {
            printf("%s:\n", g_help[cnt].name);
            help0(g_help[cnt].help, 1);
        }
        }
        break;
    default:
        for (cnt = 0; g_help[cnt].id; cnt++)
            if (g_help[cnt].id == mode)
            {
                printf("%s:\n", g_help[cnt].name);
                help0(g_help[cnt].help, 1);
                break;
            }
        break;
    }
}

static int check_image_header(const char *buf)
{
    if (!strncmp(buf, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8))
        return 1;               /* PNG */
    else if (!strncmp(buf, "\xFF\xD8\xFF\xE0", 4) ||
             !strncmp(buf, "\xFF\xD8\xFF\xE1", 4))
        return 1;               /* JPEG */
    else if (!strncmp(buf, "GIF87a", 6) || !strncmp(buf, "GIF89a", 6))
        return 1;               /* GIF */
    else
        return 0;
}

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
        lf = channels - 1;      // default AAC position

    if (center > 0)
        center--;
    else
        center = 0;             // default AAC position

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

#define fprintf if(verbose)fprintf

int main(int argc, char *argv[])
{
    int frames, currentFrame;
    faacEncHandle hEncoder;
    pcmfile_t *infile = NULL;

    unsigned long samplesInput, maxBytesOutput, totalBytesWritten = 0;

    faacEncConfigurationPtr myFormat;
    unsigned int mpegVersion = MPEG2;
    unsigned int objectType = LOW;
    int jointmode = -1;
    int pnslevel = -1;
    static int useTns = 0;
    enum container_format container = NO_CONTAINER;
    enum stream_format stream = ADTS_STREAM;
    int cutOff = -1;
    int bitRate = 0;
    unsigned long quantqual = 0;
    int chanC = 3;
    int chanLF = 4;

    char *audioFileName = NULL;
    char *aacFileName = NULL;
    char *aacFileExt = NULL;
    int aacFileNameGiven = 0;

    float *pcmbuf;
    int *chanmap = NULL;

    unsigned char *bitbuf;
    int samplesRead = 0;
    const char *dieMessage = NULL;

    int rawChans = 0;           // disabled by default
    int rawBits = 16;
    int rawRate = 44100;
    int rawEndian = 1;

    int shortctl = SHORTCTL_NORMAL;

    FILE *outfile = NULL;

    unsigned int ntracks = 0, trackno = 0;
    unsigned int ndiscs = 0, discno = 0;
    static int compilation = 0;
    const char *artist = NULL, *title = NULL, *album = NULL, *year = NULL,
        *comment = NULL, *composer = NULL, *tagname = 0, *tagval = 0;
    int genre = 0;
    uint8_t *artData = NULL;
    uint64_t artSize = 0;
    uint64_t encoded_samples = 0;
    unsigned int delay_samples;
    unsigned int frameSize;
    uint64_t input_samples = 0;
    char *faac_id_string;
    char *faac_copyright_string;
    static int ignorelen = 0;
    int verbose = 1;
    static int overwrite = 0;

#ifndef _WIN32
    // install signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif

    // get faac version
    if (faacEncGetVersion(&faac_id_string, &faac_copyright_string) ==
        FAAC_CFG_VERSION)
    {
        fprintf(stderr, "Freeware Advanced Audio Coder\nFAAC %s\n\n",
                faac_id_string);
    }
    else
    {
        fprintf(stderr, __FILE__ "(%d): wrong libfaac version\n", __LINE__);
        return 1;
    }

    /* begin process command line */
    progName = argv[0];
    if (argc < 2)
    {
        help('?');
        return 1;
    }
    while (1)
    {
        static struct option long_options[] = {
            {"help", 0, 0, 'h'},
            {"help-qual", 0, 0, HELP_QUAL},
            {"help-io", 0, 0, HELP_IO},
            {"help-mp4", 0, 0, HELP_MP4},
            {"help-advanced", 0, 0, HELP_ADVANCED},
            {"raw", 0, 0, 'r'},
            {"joint", required_argument, 0, OPT_JOINT},
            {"pns", required_argument, 0, OPT_PNS},
            {"cutoff", 1, 0, 'c'},
            {"quality", 1, 0, 'q'},
            {"pcmraw", 0, 0, 'P'},
            {"pcmsamplerate", 1, 0, 'R'},
            {"pcmsamplebits", 1, 0, 'B'},
            {"pcmchannels", 1, 0, 'C'},
            {"shortctl", 1, 0, SHORTCTL_FLAG},
            {"tns", 0, &useTns, 1},
            {"no-tns", 0, &useTns, 0},
            {"mpeg-version", 1, 0, MPEGVERS_FLAG},
            {"license", 0, 0, 'L'},
            {"createmp4", 0, 0, 'w'},
            {"artist", 1, 0, ARTIST_FLAG},
            {"title", 1, 0, TITLE_FLAG},
            {"album", 1, 0, ALBUM_FLAG},
            {"track", 1, 0, TRACK_FLAG},
            {"disc", 1, 0, DISC_FLAG},
            {"genre", 1, 0, GENRE_FLAG},
            {"year", 1, 0, YEAR_FLAG},
            {"cover-art", 1, 0, COVER_ART_FLAG},
            {"comment", 1, 0, COMMENT_FLAG},
            {"composer", 1, 0, WRITER_FLAG},
            {"compilation", 0, &compilation, 1},
            {"pcmswapbytes", 0, 0, 'X'},
            {"ignorelength", 0, &ignorelen, 1},
            {"tag", 1, 0, TAG_FLAG},
            {"overwrite", 0, &overwrite, 1},
            {0, 0, 0, 0}
        };
        int c = -1;
        int option_index = 0;

        c = getopt_long(argc, argv, "Hhb:m:o:rnc:q:PR:B:C:I:Xwv:",
                        long_options, &option_index);

        if (c == -1)
            break;

        if (!c)
            continue;

        switch (c)
        {
        case 'o':
            {
                int l = strlen(optarg);
                aacFileName = malloc(l + 1);
                memcpy(aacFileName, optarg, l);
                aacFileName[l] = '\0';
                aacFileNameGiven = 1;
            }
            break;
        case 'r':
            {
                stream = RAW_STREAM;
                break;
            }
        case 'c':
            {
                unsigned int i;
                if (sscanf(optarg, "%u", &i) > 0)
                {
                    cutOff = i;
                }
                break;
            }
        case 'b':
            {
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
                    if (i > 0)
                        quantqual = i;
                }
                break;
            }
        case 'I':
            sscanf(optarg, "%d,%d", &chanC, &chanLF);
            break;
        case 'P':
            rawChans = 2;       // enable raw input
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
        case 'w':
            container = MP4_CONTAINER;
            break;
        case ARTIST_FLAG:
            artist = optarg;
            break;
        case WRITER_FLAG:
            composer = optarg;
            break;
        case TITLE_FLAG:
            title = optarg;
            break;
        case ALBUM_FLAG:
            album = optarg;
            break;
        case TRACK_FLAG:
            if (sscanf(optarg, "%d/%d", &trackno, &ntracks) < 1)
                dieMessage = "Wrong track number.\n";
            break;
        case DISC_FLAG:
            if (sscanf(optarg, "%d/%d", &discno, &ndiscs) < 1)
                dieMessage = "Wrong disc number.\n";
            break;
        case GENRE_FLAG:
            genre = atoi(optarg);
            if ((genre < 0) || (genre > 146))
                dieMessage = "Genre number out of range.\n";
            genre++;
            break;
        case YEAR_FLAG:
            year = optarg;
            break;
        case COMMENT_FLAG:
            comment = optarg;
            break;
        case TAG_FLAG:
            tagname = optarg;
            if (!(tagval = strchr(optarg, ',')))
                dieMessage = "Missing tag value.\n";
            else
                *(char *)tagval++ = 0;
            mp4tag_add(tagname, tagval);
            break;
        case COVER_ART_FLAG:
            {
                FILE *artFile = fopen(optarg, "rb");

                if (artFile)
                {
                    uint64_t r;

                    fseek(artFile, 0, SEEK_END);
                    artSize = ftell(artFile);

                    artData = malloc(artSize);

                    fseek(artFile, 0, SEEK_SET);
                    clearerr(artFile);

                    r = fread(artData, artSize, 1, artFile);

                    if (r != 1)
                    {
                        dieMessage = "Error reading cover art file!\n";
                        free(artData);
                        artData = NULL;
                    }
                    else if (artSize < 12
                             || !check_image_header((const char *) artData))
                    {
                        /* the above expression checks the image signature */
                        dieMessage = "Unsupported cover image file format!\n";
                        free(artData);
                        artData = NULL;
                    }

                    fclose(artFile);
                }
                else
                {
                    dieMessage = "Error opening cover art file!\n";
                }

                break;
            }
        case SHORTCTL_FLAG:
            shortctl = atoi(optarg);
            break;
        case MPEGVERS_FLAG:
            mpegVersion = atoi(optarg);
            switch (mpegVersion)
            {
            case 2:
                mpegVersion = MPEG2;
                break;
            case 4:
                mpegVersion = MPEG4;
                break;
            default:
                dieMessage = "Unrecognised MPEG version!\n";
            }
            break;
        case 'L':
            fprintf(stderr, "%s", faac_copyright_string);
            dieMessage = license;
            break;
        case 'X':
            rawEndian = 0;
            break;
        case 'v':
            verbose = atoi(optarg);
            break;
        case HELP_QUAL:
        case HELP_IO:
        case HELP_MP4:
        case HELP_ADVANCED:
        case 'H':
        case 'h':
            help(c);
            return 1;
            break;
        case OPT_JOINT:
            jointmode = atoi(optarg);
            break;
        case OPT_PNS:
            pnslevel = atoi(optarg);
            break;
        case '?':
        default:
            help('?');
            return 1;
            break;
        }
    }

    /* check that we have at least one non-option arguments */
    if (!dieMessage && (argc - optind) > 1 && aacFileNameGiven)
        dieMessage =
            "Cannot encode several input files to one output file.\n";

    if (argc - optind < 1 || dieMessage)
    {
        fprintf(stderr, dieMessage, progName, progName, progName, progName);
        return 1;
    }

    while (argc - optind > 0)
    {
        /* get the input file name */
        audioFileName = argv[optind++];
    }

    /* generate the output file name, if necessary */
    if (!aacFileNameGiven)
    {
        char *t = strrchr(audioFileName, '.');
        int l = t ? strlen(audioFileName) - strlen(t) : strlen(audioFileName);

        aacFileExt = container == MP4_CONTAINER ? ".m4a" : ".aac";

        aacFileName = malloc(l + 1 + 4);
        memcpy(aacFileName, audioFileName, l);
        memcpy(aacFileName + l, aacFileExt, 4);
        aacFileName[l + 4] = '\0';
    }
    else
    {
        aacFileExt = strrchr(aacFileName, '.');

        if (aacFileExt
            && (!strcmp(".m4a", aacFileExt) || !strcmp(".m4b", aacFileExt)
                || !strcmp(".mp4", aacFileExt)))
            container = MP4_CONTAINER;
    }

    /* open the audio input file */
    if (rawChans > 0)           // use raw input
    {
        infile = wav_open_read(audioFileName, 1);
        if (infile)
        {
            infile->bigendian = rawEndian;
            infile->channels = rawChans;
            infile->samplebytes = rawBits / 8;
            infile->samplerate = rawRate;
            infile->samples /= (infile->channels * infile->samplebytes);
        }
    }
    else                        // header input
        infile = wav_open_read(audioFileName, 0);

    if (infile == NULL)
    {
        fprintf(stderr, "Couldn't open input file %s\n", audioFileName);
        return 1;
    }

    /* open the encoder library */
    hEncoder = faacEncOpen(infile->samplerate, infile->channels,
                           &samplesInput, &maxBytesOutput);

    if (hEncoder == NULL)
    {
        fprintf(stderr, "Couldn't open encoder instance for input file %s\n",
                audioFileName);
        wav_close(infile);
        return 1;
    }

    if (container != MP4_CONTAINER && (ntracks || trackno || artist ||
                                       title || album || year || artData ||
                                       genre || comment || discno || ndiscs ||
                                       composer || compilation))
    {
        fprintf(stderr, "Metadata requires MP4 output!\n");
        return 1;
    }

    if (container == MP4_CONTAINER)
    {
        mpegVersion = MPEG4;
        stream = RAW_STREAM;
    }

    frameSize = samplesInput / infile->channels;
    delay_samples = frameSize;  // encoder delay 1024 samples
    pcmbuf = (float *) malloc(samplesInput * sizeof(float));
    bitbuf = (unsigned char *) malloc(maxBytesOutput * sizeof(unsigned char));
    chanmap = mkChanMap(infile->channels, chanC, chanLF);
    if (chanmap)
    {
        fprintf(stderr, "Remapping input channels: Center=%d, LFE=%d\n",
                chanC, chanLF);
    }

    if (cutOff <= 0)
    {
        if (cutOff < 0)         // default
            cutOff = 0;
        else                    // disabled
            cutOff = infile->samplerate / 2;
    }
    if (cutOff > (infile->samplerate / 2))
        cutOff = infile->samplerate / 2;

    /* put the options in the configuration struct */
    myFormat = faacEncGetCurrentConfiguration(hEncoder);
    myFormat->aacObjectType = objectType;
    myFormat->mpegVersion = mpegVersion;
    myFormat->useTns = useTns;
    switch (shortctl)
    {
    case SHORTCTL_NOSHORT:
        fprintf(stderr, "disabling short blocks\n");
        myFormat->shortctl = shortctl;
        break;
    case SHORTCTL_NOLONG:
        fprintf(stderr, "disabling long blocks\n");
        myFormat->shortctl = shortctl;
        break;
    }
    if (infile->channels >= 6)
        myFormat->useLfe = 1;
    if (jointmode >= 0)
        myFormat->jointmode = jointmode;
    if (pnslevel >= 0)
        myFormat->pnslevel = pnslevel;
    if (quantqual > 0)
    {
        myFormat->quantqual = quantqual;
        myFormat->bitRate = 0;
    }
    if (bitRate)
        myFormat->bitRate = bitRate / infile->channels;
    myFormat->bandWidth = cutOff;
    myFormat->outputFormat = stream;
    myFormat->inputFormat = FAAC_INPUT_FLOAT;
    if (!faacEncSetConfiguration(hEncoder, myFormat))
    {
        fprintf(stderr, "Unsupported output format!\n");
        return 1;
    }

    /* initialize MP4 creation */
    if (container == MP4_CONTAINER)
    {
        if (!strcmp(aacFileName, "-"))
        {
            fprintf(stderr, "cannot encode MP4 to stdout\n");
            return 1;
        }

        if (mp4atom_open(aacFileName, overwrite))
        {
            fprintf(stderr, "Couldn't create output file %s\n", aacFileName);
            return 1;
        }
        mp4atom_head();

        mp4config.samplerate = infile->samplerate;
        mp4config.channels = infile->channels;
        mp4config.bits = infile->samplebytes * 8;
    }
    else
    {
        /* open the aac output file */
        if (!strcmp(aacFileName, "-"))
        {
            outfile = stdout;
        }
        else
        {
            outfile = fopen(aacFileName, "wb");
        }
        if (!outfile)
        {
            fprintf(stderr, "Couldn't create output file %s\n", aacFileName);
            return 1;
        }
    }

    cutOff = myFormat->bandWidth;
    quantqual = myFormat->quantqual;
    bitRate = myFormat->bitRate;
    if (bitRate)
    {
        fprintf(stderr, "Initial quantization quality: %ld\n", quantqual);
        fprintf(stderr, "Average bitrate: %d kbps/channel\n",
                (bitRate + 500) / 1000);
    }
    else
        fprintf(stderr, "Quantization quality: %ld\n", quantqual);
    fprintf(stderr, "Bandwidth: %d Hz\n", cutOff);
    if (myFormat->pnslevel > 0)
        fprintf(stderr, "PNS level: %d\n", myFormat->pnslevel);
    fprintf(stderr, "Object type: ");
    switch (objectType)
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

    switch(myFormat->jointmode) {
    case JOINT_MS:
        fprintf(stderr, " + M/S");
        break;
    case JOINT_IS:
        fprintf(stderr, " + IS");
        break;
    }
    if (myFormat->pnslevel > 0)
        fprintf(stderr, " + PNS");
    fprintf(stderr, "\n");

    fprintf(stderr, "Container format: ");
    switch (container)
    {
    case NO_CONTAINER:
        switch (stream)
        {
        case RAW_STREAM:
            fprintf(stderr, "Headerless AAC (RAW)\n");
            break;
        case ADTS_STREAM:
            fprintf(stderr, "Transport Stream (ADTS)\n");
            break;
        }
        break;
    case MP4_CONTAINER:
        fprintf(stderr, "MPEG-4 File Format (MP4)\n");
        break;
    }

    int showcnt = 0;
#ifdef _WIN32
    long begin = GetTickCount();
#endif
    if (infile->samples)
        frames = ((infile->samples + 1023) / 1024) + 1;
    else
        frames = 0;
    currentFrame = 0;

    fprintf(stderr, "Encoding %s to %s\n", audioFileName, aacFileName);
    if (frames != 0)
    {
        fprintf(stderr, "   frame          | bitrate | elapsed/estim | "
                "play/CPU | ETA\n");
    }
    else
    {
        fprintf(stderr, " frame | elapsed | play/CPU\n");
    }

    /* encoding loop */
#ifdef _WIN32
    for (;;)
#else
    while (running)
#endif
    {
        int bytesWritten;

        if (!ignorelen)
        {
            if (input_samples < infile->samples || infile->samples == 0)
                samplesRead =
                    wav_read_float32(infile, pcmbuf, samplesInput, chanmap);
            else
                samplesRead = 0;

            if (input_samples + (samplesRead / infile->channels) >
                infile->samples && infile->samples != 0)
                samplesRead =
                    (infile->samples - input_samples) * infile->channels;
        }
        else
            samplesRead =
                wav_read_float32(infile, pcmbuf, samplesInput, chanmap);

        input_samples += samplesRead / infile->channels;

        /* call the actual encoding routine */
        bytesWritten = faacEncEncode(hEncoder,
                                     (int32_t *) pcmbuf,
                                     samplesRead, bitbuf, maxBytesOutput);

        if (bytesWritten)
        {
            currentFrame++;
            showcnt--;
            totalBytesWritten += bytesWritten;
        }

        if ((showcnt <= 0) || !bytesWritten)
        {
            double timeused;
#ifdef __unix__
            struct rusage usage;
#endif
#ifdef _WIN32
            char percent[MAX_PATH + 20];
            timeused = (GetTickCount() - begin) * 1e-3;
#else
#ifdef __unix__
            if (getrusage(RUSAGE_SELF, &usage) == 0)
            {
                timeused = (double) usage.ru_utime.tv_sec +
                    (double) usage.ru_utime.tv_usec * 1e-6;
            }
            else
                timeused = 0;
#else
            timeused = (double) clock() * (1.0 / CLOCKS_PER_SEC);
#endif
#endif
            if (currentFrame && (timeused > 0.1))
            {
                showcnt += 50;

                if (frames != 0)
                {
                    fprintf(stderr,
                            "\r%5d/%-5d (%3d%%)|  %5.1f  | %6.1f/%-6.1f | %7.2fx | %.1f ",
                            currentFrame, frames, currentFrame * 100 / frames,
                            ((double) totalBytesWritten * 8.0 / 1000.0) /
                            ((double) infile->samples / infile->samplerate *
                             currentFrame / frames), timeused,
                            timeused * frames / currentFrame,
                            (1024.0 * currentFrame / infile->samplerate) /
                            timeused,
                            timeused * (frames -
                                        currentFrame) / currentFrame);
                }
                else
                {
                    fprintf(stderr,
                            "\r %5d |  %6.1f | %7.2fx ",
                            currentFrame,
                            timeused,
                            (1024.0 * currentFrame / infile->samplerate) /
                            timeused);
                }

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
            break;

        if (bytesWritten < 0)
        {
            fprintf(stderr, "faacEncEncode() failed\n");
            break;
        }

        if (bytesWritten > 0)
        {
            uint64_t frame_samples = input_samples - encoded_samples;
            if (frame_samples > delay_samples)
                frame_samples = delay_samples;

            if (container == MP4_CONTAINER)
                mp4atom_frame(bitbuf, bytesWritten, frame_samples);
            else
                fwrite(bitbuf, 1, bytesWritten, outfile);

            encoded_samples += frame_samples;
        }
    }
    fprintf(stderr, "\n");

    if (container == MP4_CONTAINER)
    {
        char *version_string = malloc(strlen(faac_id_string) + 6);

        faacEncGetDecoderSpecificInfo(hEncoder,
                                      &mp4config.asc.data,
                                      &mp4config.asc.size);
        strcpy(version_string, "FAAC ");
        strcpy(version_string + 5, faac_id_string);

        mp4config.tag.encoder = version_string;

#define SETTAG(x) if(x)mp4config.tag.x=x
        SETTAG(artist);
        SETTAG(composer);
        SETTAG(title);
        SETTAG(album);
        SETTAG(trackno);
        SETTAG(ntracks);
        SETTAG(discno);
        SETTAG(ndiscs);
        SETTAG(compilation);
        SETTAG(year);
        SETTAG(genre);
        SETTAG(comment);
        if (artData && artSize)
        {
            mp4config.tag.cover.data = artData;
            mp4config.tag.cover.size = artSize;
        }

        mp4atom_tail();
        mp4atom_close();

        free(version_string);

        if (verbose >= 2)
        {
            fprintf(stderr, "%u frames\n", mp4config.frame.ents);
            fprintf(stderr, "%u output samples\n", mp4config.samples);
            fprintf(stderr, "max bitrate: %u\n", mp4config.bitrate.max);
            fprintf(stderr, "avg bitrate: %u\n", mp4config.bitrate.avg);
            fprintf(stderr, "max frame size: %u\n", mp4config.buffersize);
        }
    }
    else
    {
        fclose(outfile);
    }

    faacEncClose(hEncoder);

    wav_close(infile);

    if (artData)
        free(artData);
    if (pcmbuf)
        free(pcmbuf);
    if (bitbuf)
        free(bitbuf);
    if (aacFileNameGiven)
        free(aacFileName);

    return 0;
}
