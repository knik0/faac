/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 * Copyright (C) 2002-2017 Krzysztof Nikiel
 * Copyright (C) 2004 Dan Villiom P. Christiansen
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <locale.h>
#endif

#if defined(__APPLE__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__bsdi__)
#define __unix__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#else
# include "getopt.h"
# include "getopt.c"
#endif

#include <faac.h>
#include "output.h"
#include "charset.h"
#include "encode_engine.h"

#ifdef _WIN32
# undef stderr
# define stderr stdout
#endif

#define MAX_COVER_ART_SIZE ((size_t)32 * 1024 * 1024)

enum flags
{
    SHORTCTL_FLAG = 300,
    MPEGVERS_FLAG,
    ARTIST_FLAG,
    ARTIST_SORT_FLAG,
    TITLE_FLAG,
    GENRE_FLAG,
    ALBUM_FLAG,
    ALBUM_SORT_FLAG,
    ALBUM_ARTIST_FLAG,
    ALBUM_ARTIST_SORT_FLAG,
    TRACK_FLAG,
    DISC_FLAG,
    YEAR_FLAG,
    COVER_ART_FLAG,
    COMMENT_FLAG,
    WRITER_FLAG,
    WRITER_SORT_FLAG,
    TAG_FLAG,
    CREATION_TIME_FLAG,
    HELP_QUAL,
    HELP_IO,
    HELP_MP4,
    HELP_ADVANCED,
    OPT_JOINT,
    OPT_PNS,
    OBJTYPE_FLAG,
    CAP_RATE_FLAG,
    OPT_TNS_ENABLE,
    OPT_TNS_DISABLE,
    OPT_OVERWRITE,
    OPT_COMPILATION,
    OPT_IGNORE_LENGTH,
    LANG_FLAG
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
    {"--cap-rate <bitrate>\tCap any single frame at x kbps.\n",
    "\t\tFor packet-oriented transports that cannot fragment a frame, where\n"
    "\t\tan oversized frame is dropped rather than split. Must be >= the -b\n"
    "\t\tbitrate. Best-effort: quality is backed off until the frame fits,\n"
    "\t\tso pathological input can still exceed the cap.\n"},
    {NULL, NULL}
};

static help_t help_io[] = {
    {"-o <filename>\tSet output file to X (only for one input file)\n",
    "\t\tFormat is auto-detected from extension (.aac/.adts -> ADTS, .m4a/.mp4/.m4b -> MP4; default: MP4).\n"},
    {"-a\t\tUse ADTS stream output format.\n",
    "\t\tGenerate ADTS transport stream output.\n"},
    {"-\t\tUse stdin/stdout\n",
    "\t\tIf you simply use a hyphen/minus sign instead\n"
    "\t\tof a filename, FAAC can encode directly from stdin,\n"
    "\t\tthus enabling piping from other applications and utilities. The\n"
    "\t\tsame works for stdout as well, so FAAC can pipe its output to\n"
    "\t\tother apps such as a server.\n"},
    {"-v <verbose>\t\tverbosity level (-v0 is quiet mode)\n", NULL},
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
    "\t\tRaw PCM input channels (default: 2, max. 8).\n"},
    {"-X\t\tRaw PCM swap input bytes\n",
    "\t\tRaw PCM swap input bytes (default: bigendian).\n"},
    {"-I <C[,LFE]>\tInput channel config, default is 3,4 (Center third, LF fourth)\n",
    "\t\tInput multichannel configuration (default: 3,4 which means\n"
    "\t\tCenter is third and LFE is fourth like in 5.1 WAV, so you only\n"
    "\t\thave to specify a different position of these two mono channels\n"
    "\t\tin your multichannel input files if they haven't been reordered\n"
    "\t\talready).\n"},
    {"--ignorelength\tIgnore wav length from header (useful with files over 4 GB)\n", NULL},
    {"--overwrite\t\tOverwrite existing output file", NULL},
    {NULL, NULL}
};

static help_t help_mp4[] = {
    {"--tag <tagname,tagvalue> Add named tag (iTunes '----')\n", NULL},
    {"--artist <name>\tSet artist name\n", NULL},
    {"--artistsort <name>\tSet artist sort order\n", NULL},
    {"--composer <name>\tSet composer name\n", NULL},
    {"--composersort <name>\tSet composer sort order\n", NULL},
    {"--title <name>\tSet title/track name\n", NULL},
    {"--genre <number>\tSet genre number\n", NULL},
    {"--album <name>\tSet album/performer\n", NULL},
    {"--albumartist <name>\tSet album artist\n", NULL},
    {"--albumartistsort <name>\tSet album artist sort order\n", NULL},
    {"--albumsort <name>\tSet album sort order\n", NULL},
    {"--compilation\tMark as compilation\n", NULL},
    {"--track <number/total>\tSet track number\n", NULL},
    {"--disc <number/total>\tSet disc number\n", NULL},
    {"--year <number>\tSet year\n", NULL},
    {"--cover-art <filename>\tRead cover art from file X\n",
    "\t\tSupported image formats are GIF, JPEG, and PNG.\n"},
    {"--comment <string>\tSet comment\n", NULL},
    {"--lang <code3>\tSet ISO 639-2/T 3-letter language code (e.g. eng, ger)\n", NULL},
    {"--creation-time <value>\tSet creation/modification time (auto, now, or timestamp)\n", NULL},
    {NULL, NULL}
};

static help_t help_advanced[] = {
    {"--tns  \tEnable coding of TNS, temporal noise shaping.\n", NULL},
    {"--no-tns\tDisable coding of TNS, temporal noise shaping.\n", NULL},
    {"--joint 0\tDisable joint stereo coding.\n", NULL},
    {"--joint 1\tUse Mid/Side coding.\n", NULL},
    {"--joint 2\tUse Intensity Stereo coding.\n", NULL},
    {"--joint 3\tUse Mixed Mode (dynamic M/S and IS) coding (default).\n", NULL},
    {"--pns <0 .. 10>\tPNS level; 0=disabled.\n", NULL},
    {"--mpeg-vers X\tForce AAC MPEG version, X can be 2 or 4\n", NULL},
    {"--object-type X\tForce AAC object type: lc, he-aac-v1, or auto (default)\n", NULL},
    {"--shortctl X\tEnforce block type (0 = both (default); 1 = no short; 2 = no\n"
    "\t\tlong).\n", NULL},
    {NULL, NULL}
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
    "FAAC is free software, licensed under the GNU Lesser General Public\n"
    "License (LGPL), version 2.1 or later:\n"
    "\n"
    "FAAC - Freeware Advanced Audio Coder\n"
    "Copyright (C) 1999-2001, Menno Bakker\n"
    "Copyright (C) 2002-2017, Krzysztof Nikiel\n"
    "Copyright (C) 2004, Dan Villiom P. Christiansen\n"
    "Copyright (C) 2005-2026, Fabian Greffrath\n"
    "Copyright (C) 2026, Nils Schimmelmann\n"
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
    "\n";

#ifndef _WIN32
volatile int running = 1;
static void signal_handler(int signal)
{
    (void)signal;
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


static bool cli_progress_callback(const progress_info_t *info, void *user_data)
{
    encode_options_t *opts = (encode_options_t *)user_data;
    if (opts && opts->verbose == 0)
    {
#ifndef _WIN32
        return running != 0;
#else
        return true;
#endif
    }

    /* Cancellation (running != 0) is checked below on every call regardless
       of this throttle: the caller invokes progress_cb every frame, but
       redrawing the status line that often just spams the terminal. */
    static progress_throttle_t s_throttle = { .last_fired_sec = -1.0 };
    if (info->is_final || progress_throttle_tick(&s_throttle, info, 0.033))
    {
        if (info->total_frames > 0)
        {
            int percent = (int)(info->current_frame * 100 / info->total_frames);
            double played_sec = (double)info->current_input_samples / (double)(info->sample_rate ? info->sample_rate : 1);
            double bitrate_kbps = played_sec > 0.0 ? ((double)info->total_bytes_written * 8.0 / 1000.0) / played_sec : 0.0;
            fprintf(stderr, "\r%7u/%-7u (%3d%%) |  %5.1f  | %6.1f/%-6.1f | %7.2fx | %.1f ",
                    info->current_frame, info->total_frames, percent,
                    bitrate_kbps,
                    info->time_elapsed_sec, info->time_elapsed_sec + info->eta_sec,
                    info->speed_factor, info->eta_sec);
        }
        else
        {
            fprintf(stderr, "\r %7u | %7.1f | %7.2fx ",
                    info->current_frame, info->time_elapsed_sec, info->speed_factor);
        }
        fflush(stderr);
    }
#ifndef _WIN32
    return running != 0;
#else
    return true;
#endif
}

static void cli_log_callback(int level, const char *message, void *user_data)
{
    encode_options_t *opts = (encode_options_t *)user_data;
    if (opts && (int)opts->verbose >= level)
    {
        fprintf(stderr, "%s", message);
    }
}

static void cli_session_start_callback(const encode_session_info_t *info, void *user_data)
{
    encode_options_t *opts = (encode_options_t *)user_data;
    if (!opts || opts->verbose < 1)
        return;

    if (info->bit_rate)
    {
        fprintf(stderr, "Initial quantization quality: %u\n", info->quant_quality);
        fprintf(stderr, "Average bitrate: %u kbps/channel\n", (info->bit_rate + 500) / 1000);
    }
    else
    {
        fprintf(stderr, "Quantization quality: %u\n", info->quant_quality);
    }
    fprintf(stderr, "Bandwidth: %u Hz\n", info->bandwidth);
    if (info->pns_level > 0)
        fprintf(stderr, "PNS level: %d\n", info->pns_level);

    const char *jm_str = "";
    switch (info->joint_mode)
    {
    case FAAC_JOINT_MS: jm_str = " + M/S"; break;
    case FAAC_JOINT_IS: jm_str = " + IS"; break;
    case FAAC_JOINT_MIXED: jm_str = " + Mixed"; break;
    default: break;
    }

    fprintf(stderr, "Object type: %s (MPEG-%d)%s%s%s\n",
            (info->object_type == FAAC_OBJ_HE_AAC_V1) ? "HE-AAC v1" : "Low Complexity",
            (info->mpeg_version == FAAC_MPEG4) ? 4 : 2,
            info->use_tns ? " + TNS" : "",
            jm_str,
            (info->pns_level > 0) ? " + PNS" : "");

    const char *fmt_str = "Unknown";
    if (info->container_mp4)
    {
        fmt_str = "MPEG-4 File Format (MP4)";
    }
    else
    {
        switch (info->stream_format)
        {
        case FAAC_STREAM_RAW: fmt_str = "Headerless AAC (RAW)"; break;
        case FAAC_STREAM_ADTS: fmt_str = "Transport Stream (ADTS)"; break;
        default: break;
        }
    }
    fprintf(stderr, "Container format: %s\n", fmt_str);

    fprintf(stderr, "Encoding %s to %s\n", info->input_filename, info->output_filename);
    if (info->total_input_samples != 0)
    {
        fprintf(stderr, "         frame         | bitrate | elapsed/estim | play/CPU | ETA\n");
    }
    else
    {
        fprintf(stderr, "  frame  | elapsed | play/CPU\n");
    }
}

static void cli_summary_callback(const encode_summary_t *summary, void *user_data)
{
    encode_options_t *opts = (encode_options_t *)user_data;
    if (!opts || opts->verbose < 2)
        return;

    fprintf(stderr, "\n");
    fprintf(stderr, "%u frames\n", summary->frame_count);
    fprintf(stderr, "%" PRIu64 " output samples\n", summary->sample_count);
    if (summary->is_mp4)
    {
        fprintf(stderr, "max bitrate: %u\n", summary->max_bitrate);
        fprintf(stderr, "avg bitrate: %u\n", summary->avg_bitrate);
        fprintf(stderr, "max frame size: %u\n", summary->max_frame_size);
    }
    else
    {
        fprintf(stderr, "avg bitrate: %u kbps\n", summary->avg_bitrate);
        fprintf(stderr, "max frame size: %u bytes\n", summary->max_frame_size);
    }
}

int main(int argc, char *argv[])
{
    encode_options_t opts;
    init_encode_options(&opts);

    char *aacFileName = NULL;
    bool aacFileNameGiven = false;
    bool stream_flag_given = false;
    bool has_custom_tags = false;
    const char *dieMessage = NULL;
    int ret = 0;

#ifndef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    /* So charset.c's utf8_ensure() can read the real locale codeset via
       nl_langinfo() instead of always seeing the default "C" locale. */
    setlocale(LC_CTYPE, "");
#endif

    faac_library_info libinfo = { .struct_size = sizeof(libinfo) };
    if (faac_get_library_info(&libinfo) != FAAC_OK)
    {
        fprintf(stderr, "Wrong libfaac version!\n");
        return 1;
    }

#ifdef _WIN32
    int wargc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    char **allocated_argv = NULL;
    if (wargv && wargc > 0)
    {
        allocated_argv = calloc((size_t)wargc, sizeof(char *));
        if (allocated_argv)
        {
            bool conv_ok = true;
            for (int i = 0; i < wargc; i++)
            {
                char *utf8_arg = win32_utf16_to_utf8(wargv[i]);
                if (!utf8_arg)
                {
                    conv_ok = false;
                    break;
                }
                allocated_argv[i] = utf8_arg;
            }

            if (conv_ok)
            {
                argc = wargc;
                argv = allocated_argv;
            }
            else
            {
                for (int i = 0; i < wargc; i++)
                {
                    if (allocated_argv[i])
                        free(allocated_argv[i]);
                }
                free(allocated_argv);
                allocated_argv = NULL;
            }
        }
        LocalFree(wargv);
    }
#endif

    if (argc < 2)
    {
        help('?');
        ret = 1;
        goto cleanup;
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
            {"tns", 0, 0, OPT_TNS_ENABLE},
            {"no-tns", 0, 0, OPT_TNS_DISABLE},
            {"mpeg-version", 1, 0, MPEGVERS_FLAG},
            {"object-type", 1, 0, OBJTYPE_FLAG},
            {"license", 0, 0, 'L'},
            {"adts", 0, 0, 'a'},
            {"artist", 1, 0, ARTIST_FLAG},
            {"artistsort", 1, 0, ARTIST_SORT_FLAG},
            {"title", 1, 0, TITLE_FLAG},
            {"album", 1, 0, ALBUM_FLAG},
            {"albumartist", 1, 0, ALBUM_ARTIST_FLAG},
            {"albumartistsort", 1, 0, ALBUM_ARTIST_SORT_FLAG},
            {"albumsort", 1, 0, ALBUM_SORT_FLAG},
            {"track", 1, 0, TRACK_FLAG},
            {"disc", 1, 0, DISC_FLAG},
            {"genre", 1, 0, GENRE_FLAG},
            {"year", 1, 0, YEAR_FLAG},
            {"cover-art", 1, 0, COVER_ART_FLAG},
            {"comment", 1, 0, COMMENT_FLAG},
            {"composer", 1, 0, WRITER_FLAG},
            {"composersort", 1, 0, WRITER_SORT_FLAG},
            {"compilation", 0, 0, OPT_COMPILATION},
            {"pcmswapbytes", 0, 0, 'X'},
            {"ignorelength", 0, 0, OPT_IGNORE_LENGTH},
            {"tag", 1, 0, TAG_FLAG},
            {"overwrite", 0, 0, OPT_OVERWRITE},
            {"creation-time", 1, 0, CREATION_TIME_FLAG},
            {"lang", 1, 0, LANG_FLAG},
            {"language", 1, 0, LANG_FLAG},
            {"cap-rate", 1, 0, CAP_RATE_FLAG},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "Hhb:m:o:rnc:q:PR:B:C:I:Xv:La",
                            long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
        case OPT_TNS_ENABLE: opts.use_tns = true; break;
        case OPT_TNS_DISABLE: opts.use_tns = false; break;
        case OPT_OVERWRITE: opts.overwrite = true; break;
        case OPT_COMPILATION: opts.metadata.compilation = true; break;
        case OPT_IGNORE_LENGTH: opts.ignore_wav_length = true; break;
        case 'L':
            if (libinfo.copyright)
                fprintf(stderr, "%s", libinfo.copyright);
            fprintf(stderr, "%s", license);
            ret = 0;
            goto cleanup;
        case 'X':
            opts.raw_endian = false;
            break;
        case 'a':
            opts.container_mp4 = false;
            opts.stream_format = FAAC_STREAM_ADTS;
            stream_flag_given = true;
            break;
        case 'o':
            aacFileName = strdup(optarg);
            aacFileNameGiven = true;
            break;
        case 'r':
            opts.container_mp4 = false;
            opts.stream_format = FAAC_STREAM_RAW;
            stream_flag_given = true;
            break;
        case 'c':
            opts.bandwidth = atoi(optarg);
            break;
        case 'b':
            parse_quality_or_bitrate(optarg, true, &opts);
            break;
        case 'q':
            parse_quality_or_bitrate(optarg, false, &opts);
            break;
        case 'I':
            if (sscanf(optarg, "%hu,%hu", &opts.center_channel, &opts.lfe_channel) < 1)
                dieMessage = "Wrong channel config.\n";
            break;
        case 'P':
            opts.raw_pcm_input = true;
            break;
        case 'R':
            opts.raw_rate = atoi(optarg);
            opts.raw_pcm_input = true;
            break;
        case 'B':
            {
                int bits = atoi(optarg);
                if (bits > 32)
                    bits = 32;
                if (bits < 8)
                    bits = 8;
                opts.raw_bits = (uint8_t)bits;
            }
            opts.raw_pcm_input = true;
            break;
        case 'C':
            opts.raw_channels = (uint16_t)atoi(optarg);
            opts.raw_pcm_input = true;
            break;
        case ARTIST_FLAG:
            opts.metadata.artist = optarg;
            break;
        case ARTIST_SORT_FLAG:
            opts.metadata.artist_sort = optarg;
            break;
        case WRITER_FLAG:
            opts.metadata.composer = optarg;
            break;
        case WRITER_SORT_FLAG:
            opts.metadata.composer_sort = optarg;
            break;
        case TITLE_FLAG:
            opts.metadata.title = optarg;
            break;
        case ALBUM_FLAG:
            opts.metadata.album = optarg;
            break;
        case ALBUM_ARTIST_FLAG:
            opts.metadata.album_artist = optarg;
            break;
        case ALBUM_ARTIST_SORT_FLAG:
            opts.metadata.album_artist_sort = optarg;
            break;
        case ALBUM_SORT_FLAG:
            opts.metadata.album_sort = optarg;
            break;
        case TRACK_FLAG:
            if (sscanf(optarg, "%hu/%hu", &opts.metadata.track, &opts.metadata.ntracks) < 1)
                dieMessage = "Wrong track number.\n";
            break;
        case DISC_FLAG:
            if (sscanf(optarg, "%hu/%hu", &opts.metadata.disc, &opts.metadata.ndiscs) < 1)
                dieMessage = "Wrong disc number.\n";
            break;
        case GENRE_FLAG:
            {
                int g = atoi(optarg);
                if (g < 0 || g > 255)
                    dieMessage = "Genre number out of range.\n";
                else
                    opts.metadata.genre_id = (uint16_t)(g + 1);
            }
            break;
        case YEAR_FLAG:
            opts.metadata.year = optarg;
            break;
        case COMMENT_FLAG:
            opts.metadata.comment = optarg;
            break;
        case MPEGVERS_FLAG:
            switch (atoi(optarg))
            {
            case 2: opts.mpeg_version = FAAC_MPEG2; break;
            case 4: opts.mpeg_version = FAAC_MPEG4; break;
            default: dieMessage = "Unrecognised MPEG version!\n"; break;
            }
            break;
        case OBJTYPE_FLAG:
            if (!strcmp(optarg, "lc"))
                opts.object_type = FAAC_OBJ_LOW;
            else if (!strcmp(optarg, "he-aac-v1"))
                opts.object_type = FAAC_OBJ_HE_AAC_V1;
            else if (!strcmp(optarg, "auto"))
                opts.object_type = FAAC_OBJ_AUTO;
            else
                dieMessage = "Unrecognised object type (use lc, he-aac-v1, or auto)!\n";
            break;
        case SHORTCTL_FLAG:
            opts.shortctl = (enum faac_shortctl_mode)atoi(optarg);
            break;
        case OPT_JOINT:
            opts.joint_mode = (enum faac_joint_mode)atoi(optarg);
            break;
        case OPT_PNS:
            opts.pns_level = (int8_t)atoi(optarg);
            break;
        case TAG_FLAG:
            {
                char *tagname = optarg;
                char *tagval = strchr(optarg, ',');
                if (!tagval)
                {
                    dieMessage = "Missing tag value.\n";
                }
                else
                {
                    *tagval++ = '\0';
                    if (*tagval == '\0')
                        dieMessage = "Tag value cannot be empty.\n";
                }
                if (!dieMessage)
                {
                    if (!add_custom_tag_to_options(&opts, tagname, tagval))
                        dieMessage = "Couldn't add tag (out of memory).\n";
                }
                has_custom_tags = true;
            }
            break;
        case COVER_ART_FLAG:
            {
#ifdef _WIN32
                FILE *f = win32_fopen_utf8(optarg, "rb");
#else
                FILE *f = fopen(optarg, "rb");
#endif
                if (f)
                {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    clearerr(f);

                    if (sz <= 0 || (size_t)sz > MAX_COVER_ART_SIZE)
                    {
                        dieMessage = "Invalid cover art file size!\n";
                    }
                    else
                    {
                        opts.art_size = (uint64_t)sz;
                        opts.art_data = malloc((size_t)opts.art_size);
                        if (opts.art_data)
                        {
                            if (fread((void *)opts.art_data, 1, (size_t)opts.art_size, f) != (size_t)opts.art_size)
                            {
                                dieMessage = "Error reading cover art file!\n";
                                free((void *)opts.art_data);
                                opts.art_data = NULL;
                                opts.art_size = 0;
                            }
                            else if (opts.art_size < 12 || !check_image_header((const char *)opts.art_data))
                            {
                                dieMessage = "Unsupported cover image file format!\n";
                                free((void *)opts.art_data);
                                opts.art_data = NULL;
                                opts.art_size = 0;
                            }
                        }
                        else
                        {
                            dieMessage = "Out of memory reading cover art file!\n";
                        }
                    }
                    fclose(f);
                }
                else
                {
                    dieMessage = "Error opening cover art file!\n";
                }
            }
            break;
        case CREATION_TIME_FLAG:
            opts.creation_time_str = optarg;
            break;
        case LANG_FLAG:
            opts.metadata.language = optarg;
            break;
        case CAP_RATE_FLAG:
            opts.max_bit_rate = atoi(optarg) * 1000;
            break;
        case 'v':
            opts.verbose = (uint8_t)atoi(optarg);
            break;
        case HELP_QUAL:
        case HELP_IO:
        case HELP_MP4:
        case HELP_ADVANCED:
        case 'H':
        case 'h':
            help(c);
            ret = 1;
            goto cleanup;
        case '?':
        default:
            help('?');
            ret = 1;
            goto cleanup;
        }
    }

    if (optind < argc)
    {
        opts.input_filename = argv[optind];
        if ((argc - optind) > 1 && aacFileNameGiven)
            dieMessage = "Cannot encode several input files to one output file.\n";
    }
    else
    {
        dieMessage = "No input file specified.\n";
    }

    if (dieMessage)
    {
        fprintf(stderr, "%s", dieMessage);
        ret = 1;
        goto cleanup;
    }

    if (!aacFileNameGiven)
    {
        aacFileName = get_output_filename(opts.input_filename, opts.container_mp4);
    }
    else if (!stream_flag_given)
    {
        opts.container_mp4 = detect_container_mp4(aacFileName);
        opts.stream_format = opts.container_mp4 ? FAAC_STREAM_RAW : FAAC_STREAM_ADTS;
    }

    if (opts.container_mp4)
    {
        opts.mpeg_version = FAAC_MPEG4;
    }

    bool has_metadata = opts.metadata.artist || opts.metadata.artist_sort ||
                        opts.metadata.title || opts.metadata.album ||
                        opts.metadata.album_sort || opts.metadata.album_artist ||
                        opts.metadata.album_artist_sort || opts.metadata.composer ||
                        opts.metadata.composer_sort || opts.metadata.year ||
                        opts.metadata.comment || opts.metadata.genre_id ||
                        opts.metadata.track || opts.metadata.disc ||
                        opts.metadata.compilation || opts.metadata.language ||
                        opts.art_data || opts.custom_tag_count > 0 || has_custom_tags;

    if (!opts.container_mp4 && has_metadata)
    {
        fprintf(stderr, "Metadata requires MP4 output!\n");
        ret = 1;
        goto cleanup;
    }

    if (opts.verbose > 0 && libinfo.version)
    {
        fprintf(stderr, "Freeware Advanced Audio Coder\nFAAC %s\n\n", libinfo.version);
    }

    opts.output_filename = aacFileName;

    encode_callbacks_t cbs = {
        .progress_cb = cli_progress_callback,
        .session_start_cb = cli_session_start_callback,
        .summary_cb = cli_summary_callback,
        .log_cb = cli_log_callback,
        .user_data = &opts
    };

    ret = run_encoding_session_ext(&opts, &cbs);
    if (opts.verbose && opts.verbose < 2)
        fprintf(stderr, "\n");

cleanup:
    if (aacFileName) free(aacFileName);
    free_encode_options(&opts);

#ifdef _WIN32
    if (allocated_argv)
    {
        for (int i = 0; i < argc; i++)
        {
            if (allocated_argv[i])
                free(allocated_argv[i]);
        }
        free(allocated_argv);
    }
#endif

    return ret;
}
