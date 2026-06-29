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
 */

#ifndef FRAME_H
#define FRAME_H

/* Input sample FIFO slots, each one frame (FRAME_LEN samples) wide, relative to
   the frame currently being coded (FIFO_CURR): one frame behind (FIFO_PAST,
   reused as the MDCT overlap) and two frames ahead. The two ahead slots are
   needed because the block-switch energy analysis works on 2-frame-wide windows
   and keeps one window of lookahead, whose far edge reaches two frames ahead. */
#define LOOKAHEAD_DEPTH 2
#define FIFO_PAST       0
#define FIFO_CURR       1
#define FIFO_AHEAD1     2
#define FIFO_AHEAD2     3

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <faac.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "coder.h"
#include "channels.h"
#include "blockswitch.h"
#include "fft.h"
#include "quantize.h"

#include <faaccfg.h>

typedef struct {
    /* number of channels in AAC file */
    unsigned int numChannels;

    /* samplerate of AAC file */
    unsigned long sampleRate;
    unsigned int sampleRateIdx;

    unsigned int usedBytes;

    /* frame number */
    unsigned int frameNum;
    unsigned int flushFrame;

    /* Scalefactorband data */
    SR_INFO *srInfo;

    /* sample buffers: FIFO_PAST (MDCT overlap), FIFO_CURR, FIFO_AHEAD1, FIFO_AHEAD2 */
    faac_real *audioFIFO[MAX_CHANNELS][4];

    /* Filterbank buffers */
    faac_real *sin_window_long;
    faac_real *sin_window_short;
    faac_real *kbd_window_long;
    faac_real *kbd_window_short;
    faac_real *freqBuff[MAX_CHANNELS];

    /* Channel and Coder data for all channels */
    CoderInfo coderInfo[MAX_CHANNELS];
    ChannelInfo channelInfo[MAX_CHANNELS];

    /* Psychoacoustics data */
    PsyInfo psyInfo[MAX_CHANNELS];
    GlobalPsyInfo gpsyInfo;

    /* Configuration data */
    faacEncConfiguration config;

    psymodel_t *psymodel;

    /* quantizer specific config */
    AACQuantCfg aacquantCfg;

    /* FFT Tables */
    FFT_Tables	fft_tables;

    /* Input FIFO: decouples the caller's per-call chunk size from the encoder
     * frame size. faacEncEncode appends whatever it is handed (any count) and
     * emits one frame once a full frame (FRAME_LEN samples/ch) has accumulated.
     * Stores format-converted faac_real. */
    faac_real    *inputFifo[MAX_CHANNELS];
    unsigned int  inputFifoFill;     /* samples per channel currently buffered */
    unsigned int  inputFifoCap;      /* per-channel capacity in samples */
} faacEncStruct;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FRAME_H */
