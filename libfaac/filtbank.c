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
 * $Id: filtbank.c,v 1.8 2001/06/08 18:01:09 menno Exp $
 */

/*
 * CHANGES:
 *  2001/01/17: menno: Added frequency cut off filter.
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "coder.h"
#include "filtbank.h"
#include "frame.h"
#include "fft.h"
#include "util.h"

#define  TWOPI       2*M_PI


void FilterBankInit(faacEncHandle hEncoder)
{
    unsigned int i, channel;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        hEncoder->freqBuff[channel] = (double*)AllocMemory(2*FRAME_LEN*sizeof(double));
        hEncoder->overlapBuff[channel] = (double*)AllocMemory(FRAME_LEN*sizeof(double));
        SetMemory(hEncoder->overlapBuff[channel], 0, FRAME_LEN*sizeof(double));
    }

    hEncoder->sin_window_long = (double*)AllocMemory(BLOCK_LEN_LONG*sizeof(double));
    hEncoder->sin_window_short = (double*)AllocMemory(BLOCK_LEN_SHORT*sizeof(double));
    hEncoder->kbd_window_long = (double*)AllocMemory(BLOCK_LEN_LONG*sizeof(double));
    hEncoder->kbd_window_short = (double*)AllocMemory(BLOCK_LEN_SHORT*sizeof(double));

    for( i=0; i<BLOCK_LEN_LONG; i++ )
        hEncoder->sin_window_long[i] = sin((M_PI/(2*BLOCK_LEN_LONG)) * (i + 0.5));
    for( i=0; i<BLOCK_LEN_SHORT; i++ )
        hEncoder->sin_window_short[i] = sin((M_PI/(2*BLOCK_LEN_SHORT)) * (i + 0.5));

    CalculateKBDWindow(hEncoder->kbd_window_long, 4, BLOCK_LEN_LONG*2);
    CalculateKBDWindow(hEncoder->kbd_window_short, 6, BLOCK_LEN_SHORT*2);
}

void FilterBankEnd(faacEncHandle hEncoder)
{
    unsigned int channel;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        if (hEncoder->freqBuff[channel]) FreeMemory(hEncoder->freqBuff[channel]);
        if (hEncoder->overlapBuff[channel]) FreeMemory(hEncoder->overlapBuff[channel]);
    }

    if (hEncoder->sin_window_long) FreeMemory(hEncoder->sin_window_long);
    if (hEncoder->sin_window_short) FreeMemory(hEncoder->sin_window_short);
    if (hEncoder->kbd_window_long) FreeMemory(hEncoder->kbd_window_long);
    if (hEncoder->kbd_window_short) FreeMemory(hEncoder->kbd_window_short);
}

void FilterBank(faacEncHandle hEncoder,
                CoderInfo *coderInfo,
                double *p_in_data,
                double *p_out_mdct,
                double *p_overlap,
                int overlap_select)
{
    double *p_o_buf, *first_window, *second_window;
    double *transf_buf;
    int k, i;
    int block_type = coderInfo->block_type;

    transf_buf = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));

    /* create / shift old values */
    /* We use p_overlap here as buffer holding the last frame time signal*/
    if(overlap_select != MNON_OVERLAPPED) {
        memcpy(transf_buf, p_overlap, FRAME_LEN*sizeof(double));
        memcpy(transf_buf+BLOCK_LEN_LONG, p_in_data, FRAME_LEN*sizeof(double));
        memcpy(p_overlap, p_in_data, FRAME_LEN*sizeof(double));
    } else {
        memcpy(transf_buf, p_in_data, 2*FRAME_LEN*sizeof(double));
    }

    /*  Window shape processing */
    if(overlap_select != MNON_OVERLAPPED) {
        switch (coderInfo->prev_window_shape) {
        case SINE_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                first_window = hEncoder->sin_window_long;
            else
                first_window = hEncoder->sin_window_short;
            break;
        case KBD_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                first_window = hEncoder->kbd_window_long;
            else
                first_window = hEncoder->kbd_window_short;
            break;
        }

        switch (coderInfo->window_shape){
        case SINE_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                second_window = hEncoder->sin_window_long;
            else
                second_window = hEncoder->sin_window_short;
            break;
        case KBD_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                second_window = hEncoder->kbd_window_long;
            else
                second_window = hEncoder->kbd_window_short;
            break;
        }
    } else {
        /* Always long block and sine window for LTP */
        first_window = hEncoder->sin_window_long;
        second_window = hEncoder->sin_window_long;
    }

    /* Set ptr to transf-Buffer */
    p_o_buf = transf_buf;

    /* Separate action for each Block Type */
    switch (block_type) {
    case ONLY_LONG_WINDOW :
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++){
            p_out_mdct[i] = p_o_buf[i] * first_window[i];
            p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
        }
        MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
        break;

    case LONG_SHORT_WINDOW :
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            p_out_mdct[i] = p_o_buf[i] * first_window[i];
        memcpy(p_out_mdct+BLOCK_LEN_LONG,p_o_buf+BLOCK_LEN_LONG,NFLAT_LS*sizeof(double));
        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
            p_out_mdct[i+BLOCK_LEN_LONG+NFLAT_LS] = p_o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
        SetMemory(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(double));
        MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
        break;

    case SHORT_LONG_WINDOW :
        SetMemory(p_out_mdct,0,NFLAT_LS*sizeof(double));
        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
            p_out_mdct[i+NFLAT_LS] = p_o_buf[i+NFLAT_LS] * first_window[i];
        memcpy(p_out_mdct+NFLAT_LS+BLOCK_LEN_SHORT,p_o_buf+NFLAT_LS+BLOCK_LEN_SHORT,NFLAT_LS*sizeof(double));
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
        MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
        break;

    case ONLY_SHORT_WINDOW :
        p_o_buf += NFLAT_LS;
        for ( k=0; k < MAX_SHORT_WINDOWS; k++ ) {
            for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++ ){
                p_out_mdct[i] = p_o_buf[i] * first_window[i];
                p_out_mdct[i+BLOCK_LEN_SHORT] = p_o_buf[i+BLOCK_LEN_SHORT] * second_window[BLOCK_LEN_SHORT-i-1];
            }
            MDCT( p_out_mdct, 2*BLOCK_LEN_SHORT );
            p_out_mdct += BLOCK_LEN_SHORT;
            p_o_buf += BLOCK_LEN_SHORT;
            first_window = second_window;
        }
        break;
    }

    if (transf_buf) FreeMemory(transf_buf);
}

void IFilterBank(faacEncHandle hEncoder,
                 CoderInfo *coderInfo,
                 double *p_in_data,
                 double *p_out_data,
                 double *p_overlap,
                 int overlap_select)
{
    double *o_buf, *transf_buf, *overlap_buf;
    double *first_window, *second_window;

    double  *fp;
    int k, i;
    int block_type = coderInfo->block_type;

    transf_buf = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));
    overlap_buf = (double*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(double));

    /*  Window shape processing */
    if (overlap_select != MNON_OVERLAPPED) {
//      switch (coderInfo->prev_window_shape){
//      case SINE_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                first_window = hEncoder->sin_window_long;
            else
                first_window = hEncoder->sin_window_short;
//          break;
//      case KBD_WINDOW:
//          if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
//              first_window = hEncoder->kbd_window_long;
//          else
//              first_window = hEncoder->kbd_window_short;
//          break;
//      }

//      switch (coderInfo->window_shape){
//      case SINE_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                second_window = hEncoder->sin_window_long;
            else
                second_window = hEncoder->sin_window_short;
//          break;
//      case KBD_WINDOW:
//          if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
//              second_window = hEncoder->kbd_window_long;
//          else
//              second_window = hEncoder->kbd_window_short;
//          break;
//      }
    } else {
        /* Always long block and sine window for LTP */
        first_window  = hEncoder->sin_window_long;
        second_window = hEncoder->sin_window_long;
    }

    /* Assemble overlap buffer */
    memcpy(overlap_buf,p_overlap,BLOCK_LEN_LONG*sizeof(double));
    o_buf = overlap_buf;

    /* Separate action for each Block Type */
    switch( block_type ) {
    case ONLY_LONG_WINDOW :
        memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizeof(double));
        IMDCT( transf_buf, 2*BLOCK_LEN_LONG );
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            transf_buf[i] *= first_window[i];
        if (overlap_select != MNON_OVERLAPPED) {
            for ( i = 0 ; i < BLOCK_LEN_LONG; i++ ){
                o_buf[i] += transf_buf[i];
                o_buf[i+BLOCK_LEN_LONG] = transf_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
            }
        } else { /* overlap_select == NON_OVERLAPPED */
            for ( i = 0 ; i < BLOCK_LEN_LONG; i++ )
                transf_buf[i+BLOCK_LEN_LONG] *= second_window[BLOCK_LEN_LONG-i-1];
        }
        break;

    case LONG_SHORT_WINDOW :
        memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizeof(double));
        IMDCT( transf_buf, 2*BLOCK_LEN_LONG );
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            transf_buf[i] *= first_window[i];
        if (overlap_select != MNON_OVERLAPPED) {
            for ( i = 0 ; i < BLOCK_LEN_LONG; i++ )
                o_buf[i] += transf_buf[i];
            memcpy(o_buf+BLOCK_LEN_LONG,transf_buf+BLOCK_LEN_LONG,NFLAT_LS*sizeof(double));
            for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] = transf_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
            SetMemory(o_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(double));
        } else { /* overlap_select == NON_OVERLAPPED */
            for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                transf_buf[i+BLOCK_LEN_LONG+NFLAT_LS] *= second_window[BLOCK_LEN_SHORT-i-1];
            SetMemory(transf_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(double));
        }
        break;

    case SHORT_LONG_WINDOW :
        memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizeof(double));
        IMDCT( transf_buf, 2*BLOCK_LEN_LONG );
        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
            transf_buf[i+NFLAT_LS] *= first_window[i];
        if (overlap_select != MNON_OVERLAPPED) {
            for ( i = 0 ; i < BLOCK_LEN_SHORT; i++ )
                o_buf[i+NFLAT_LS] += transf_buf[i+NFLAT_LS];
            memcpy(o_buf+BLOCK_LEN_SHORT+NFLAT_LS,transf_buf+BLOCK_LEN_SHORT+NFLAT_LS,NFLAT_LS*sizeof(double));
            for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                o_buf[i+BLOCK_LEN_LONG] = transf_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
        } else { /* overlap_select == NON_OVERLAPPED */
            SetMemory(transf_buf,0,NFLAT_LS*sizeof(double));
            for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                transf_buf[i+BLOCK_LEN_LONG] *= second_window[BLOCK_LEN_LONG-i-1];
        }
        break;

    case ONLY_SHORT_WINDOW :
        if (overlap_select != MNON_OVERLAPPED) {
            fp = o_buf + NFLAT_LS;
        } else { /* overlap_select == NON_OVERLAPPED */
            fp = transf_buf;
        }
        for ( k=0; k < MAX_SHORT_WINDOWS; k++ ) {
            memcpy(transf_buf,p_in_data,BLOCK_LEN_SHORT*sizeof(double));
            IMDCT( transf_buf, 2*BLOCK_LEN_SHORT );
            p_in_data += BLOCK_LEN_SHORT;
            if (overlap_select != MNON_OVERLAPPED) {
                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++){
                    transf_buf[i] *= first_window[i];
                    fp[i] += transf_buf[i];
                    fp[i+BLOCK_LEN_SHORT] = transf_buf[i+BLOCK_LEN_SHORT] * second_window[BLOCK_LEN_SHORT-i-1];
                }
                fp += BLOCK_LEN_SHORT;
            } else { /* overlap_select == NON_OVERLAPPED */
                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++){
                    fp[i] *= first_window[i];
                    fp[i+BLOCK_LEN_SHORT] *= second_window[BLOCK_LEN_SHORT-i-1];
                }
                fp += 2*BLOCK_LEN_SHORT;
            }
            first_window = second_window;
        }
        SetMemory(o_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(double));
        break;
    }

    if (overlap_select != MNON_OVERLAPPED)
        memcpy(p_out_data,o_buf,BLOCK_LEN_LONG*sizeof(double));
    else  /* overlap_select == NON_OVERLAPPED */
        memcpy(p_out_data,transf_buf,2*BLOCK_LEN_LONG*sizeof(double));

    /* save unused output data */
    memcpy(p_overlap,o_buf+BLOCK_LEN_LONG,BLOCK_LEN_LONG*sizeof(double));

    if (overlap_buf) FreeMemory(overlap_buf);
    if (transf_buf) FreeMemory(transf_buf);
}

void specFilter(double *freqBuff,
                int sampleRate,
                int lowpassFreq,
                int specLen
                )
{
    int lowpass,xlowpass;

    /* calculate the last line which is not zero */
    lowpass = (lowpassFreq * specLen) / (sampleRate>>1) + 1;
    xlowpass = (lowpass < specLen) ? lowpass : specLen ;

    SetMemory(freqBuff+xlowpass,0,(specLen-xlowpass)*sizeof(double));
}

static double Izero(double x)
{
    const double IzeroEPSILON = 1E-41;  /* Max error acceptable in Izero */
    double sum, u, halfx, temp;
    int n;

    sum = u = n = 1;
    halfx = x/2.0;
    do {
        temp = halfx/(double)n;
        n += 1;
        temp *= temp;
        u *= temp;
        sum += u;
    } while (u >= IzeroEPSILON*sum);

    return(sum);
}

static void CalculateKBDWindow(double* win, double alpha, int length)
{
    int i;
    double IBeta;
    double tmp;
    double sum = 0.0;

    alpha *= M_PI;
    IBeta = 1.0/Izero(alpha);

    /* calculate lower half of Kaiser Bessel window */
    for(i=0; i<(length>>1); i++) {
        tmp = 4.0*(double)i/(double)length - 1.0;
        win[i] = Izero(alpha*sqrt(1.0-tmp*tmp))*IBeta;
        sum += win[i];
    }

    sum = 1.0/sum;
    tmp = 0.0;

    /* calculate lower half of window */
    for(i=0; i<(length>>1); i++) {
        tmp += win[i];
        win[i] = sqrt(tmp*sum);
    }
}

static void MDCT(double *data, int N)
{
    double *xi, *xr;
    double tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    double freq = TWOPI / N;
    double cosfreq8, sinfreq8;
    int i, n;

    xi = (double*)AllocMemory((N >> 2)*sizeof(double));
    xr = (double*)AllocMemory((N >> 2)*sizeof(double));

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    cosfreq8 = cos (freq * 0.125);
    sinfreq8 = sin (freq * 0.125);
    c = cosfreq8;
    s = sinfreq8;

    for (i = 0; i < (N >> 2); i++) {
        /* calculate real and imaginary parts of g(n) or G(p) */
        n = (N >> 1) - 1 - 2 * i;

        if (i < (N >> 3))
            tempr = data [(N >> 2) + n] + data [N + (N >> 2) - 1 - n]; /* use second form of e(n) for n = N / 2 - 1 - 2i */
        else
            tempr = data [(N >> 2) + n] - data [(N >> 2) - 1 - n]; /* use first form of e(n) for n = N / 2 - 1 - 2i */

        n = 2 * i;
        if (i < (N >> 3))
            tempi = data [(N >> 2) + n] - data [(N >> 2) - 1 - n]; /* use first form of e(n) for n=2i */
        else
            tempi = data [(N >> 2) + n] + data [N + (N >> 2) - 1 - n]; /* use second form of e(n) for n=2i*/

        /* calculate pre-twiddled FFT input */
        xr[i] = tempr * c + tempi * s;
        xi[i] = tempi * c - tempr * s;

        /* use recurrence to prepare cosine and sine for next value of i */
        cold = c;
        c = c * cfreq - s * sfreq;
        s = s * cfreq + cold * sfreq;
    }

    /* Perform in-place complex FFT of length N/4 */
    switch (N) {
    case 256:
        srfft(xr, xi, 6);
        break;
    case 2048:
        srfft(xr, xi, 9);
    }

    /* prepare for recurrence relations in post-twiddle */
    c = cosfreq8;
    s = sinfreq8;

    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < (N >> 2); i++) {
        /* get post-twiddled FFT output  */
        tempr = 2. * (xr[i] * c + xi[i] * s);
        tempi = 2. * (xi[i] * c - xr[i] * s);

        /* fill in output values */
        data [2 * i] = -tempr;   /* first half even */
        data [(N >> 1) - 1 - 2 * i] = tempi;  /* first half odd */
        data [(N >> 1) + 2 * i] = -tempi;  /* second half even */
        data [N - 1 - 2 * i] = tempr;  /* second half odd */

        /* use recurrence to prepare cosine and sine for next value of i */
        cold = c;
        c = c * cfreq - s * sfreq;
        s = s * cfreq + cold * sfreq;
    }

    if (xr) FreeMemory(xr);
    if (xi) FreeMemory(xi);
}

static void IMDCT(double *data, int N)
{
    double *xi, *xr;
    double tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    double freq = 2.0 * M_PI / N;
    double fac, cosfreq8, sinfreq8;
    int i;

    xi = (double*)AllocMemory((N >> 2)*sizeof(double));
    xr = (double*)AllocMemory((N >> 2)*sizeof(double));

    /* Choosing to allocate 2/N factor to Inverse Xform! */
    fac = 2. / N; /* remaining 2/N from 4/N IFFT factor */

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    cosfreq8 = cos (freq * 0.125);
    sinfreq8 = sin (freq * 0.125);
    c = cosfreq8;
    s = sinfreq8;

    for (i = 0; i < (N >> 2); i++) {
        /* calculate real and imaginary parts of g(n) or G(p) */
        tempr = -data[2 * i];
        tempi = data[(N >> 1) - 1 - 2 * i];

        /* calculate pre-twiddled FFT input */
        xr[i] = tempr * c - tempi * s;
        xi[i] = tempi * c + tempr * s;

        /* use recurrence to prepare cosine and sine for next value of i */
        cold = c;
        c = c * cfreq - s * sfreq;
        s = s * cfreq + cold * sfreq;
    }

    /* Perform in-place complex IFFT of length N/4 */
    switch (N) {
    case 256:
        srifft(xr, xi, 6);
        break;
    case 2048:
        srifft(xr, xi, 9);
    }

    /* prepare for recurrence relations in post-twiddle */
    c = cosfreq8;
    s = sinfreq8;

    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < (N >> 2); i++) {

        /* get post-twiddled FFT output  */
        tempr = fac * (xr[i] * c - xi[i] * s);
        tempi = fac * (xi[i] * c + xr[i] * s);

        /* fill in output values */
        data [(N >> 1) + (N >> 2) - 1 - 2 * i] = tempr;
        if (i < (N >> 3))
            data [(N >> 1) + (N >> 2) + 2 * i] = tempr;
        else
            data [2 * i - (N >> 2)] = -tempr;

        data [(N >> 2) + 2 * i] = tempi;
        if (i < (N >> 3))
            data [(N >> 2) - 1 - 2 * i] = -tempi;
        else
            data [(N >> 2) + N - 1 - 2*i] = tempi;

        /* use recurrence to prepare cosine and sine for next value of i */
        cold = c;
        c = c * cfreq - s * sfreq;
        s = s * cfreq + cold * sfreq;
    }

    if (xr) FreeMemory(xr);
    if (xi) FreeMemory(xi);
}
