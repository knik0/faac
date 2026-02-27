/************************* MPEG-2 NBC Audio Decoder **************************
 *                                                                           *
"This software module was originally developed by
AT&T, Dolby Laboratories, Fraunhofer Gesellschaft IIS in the course of
development of the MPEG-2 NBC/MPEG-4 Audio standard ISO/IEC 13818-7,
14496-1,2 and 3. This software module is an implementation of a part of one or more
MPEG-2 NBC/MPEG-4 Audio tools as specified by the MPEG-2 NBC/MPEG-4
Audio standard. ISO/IEC  gives users of the MPEG-2 NBC/MPEG-4 Audio
standards free license to this software module or modifications thereof for use in
hardware or software products claiming conformance to the MPEG-2 NBC/MPEG-4
Audio  standards. Those intending to use this software module in hardware or
software products are advised that this use may infringe existing patents.
The original developer of this software module and his/her company, the subsequent
editors and their companies, and ISO/IEC have no liability for use of this software
module or modifications thereof in an implementation. Copyright is not released for
non MPEG-2 NBC/MPEG-4 Audio conforming products.The original developer
retains full right to use the code for his/her  own purpose, assign or donate the
code to a third party and to inhibit third party from using the code for non
MPEG-2 NBC/MPEG-4 Audio conforming products. This copyright notice must
be included in all copies or derivative works."
Copyright(c)1996.
 *                                                                           *
 ****************************************************************************/
/*
 * $Id: filtbank.c,v 1.14 2012/03/01 18:34:17 knik Exp $
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


static void		CalculateKBDWindow	( faac_real* win, faac_real alpha, int length );
static faac_real	Izero				( faac_real x);
static void		MDCT				( FFT_Tables *fft_tables, faac_real *data, int N );
static void		IMDCT				( FFT_Tables *fft_tables, faac_real *data, int N );



void FilterBankInit(faacEncStruct* hEncoder)
{
    unsigned int i, channel;

    for (channel = 0; channel < hEncoder->numChannels; channel++) {
        hEncoder->freqBuff[channel] = (faac_real*)AllocMemory(2*FRAME_LEN*sizeof(faac_real));
        hEncoder->overlapBuff[channel] = (faac_real*)AllocMemory(FRAME_LEN*sizeof(faac_real));
        SetMemory(hEncoder->overlapBuff[channel], 0, FRAME_LEN*sizeof(faac_real));
    }

    hEncoder->sin_window_long = (faac_real*)AllocMemory(BLOCK_LEN_LONG*sizeof(faac_real));
    hEncoder->sin_window_short = (faac_real*)AllocMemory(BLOCK_LEN_SHORT*sizeof(faac_real));
    hEncoder->kbd_window_long = (faac_real*)AllocMemory(BLOCK_LEN_LONG*sizeof(faac_real));
    hEncoder->kbd_window_short = (faac_real*)AllocMemory(BLOCK_LEN_SHORT*sizeof(faac_real));

    for( i=0; i<BLOCK_LEN_LONG; i++ )
        hEncoder->sin_window_long[i] = FAAC_SIN((M_PI/(2*BLOCK_LEN_LONG)) * (i + 0.5));
    for( i=0; i<BLOCK_LEN_SHORT; i++ )
        hEncoder->sin_window_short[i] = FAAC_SIN((M_PI/(2*BLOCK_LEN_SHORT)) * (i + 0.5));

    CalculateKBDWindow(hEncoder->kbd_window_long, 4, BLOCK_LEN_LONG*2);
    CalculateKBDWindow(hEncoder->kbd_window_short, 6, BLOCK_LEN_SHORT*2);
}

void FilterBankEnd(faacEncStruct* hEncoder)
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

void FilterBank(faacEncStruct* hEncoder,
                CoderInfo *coderInfo,
                faac_real *p_in_data,
                faac_real *p_out_mdct,
                faac_real *p_overlap,
                int overlap_select)
{
    faac_real *p_o_buf, *first_window, *second_window;
    faac_real *transf_buf;
    int k, i;
    int block_type = coderInfo->block_type;

    transf_buf = (faac_real*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(faac_real));

    /* create / shift old values */
    /* We use p_overlap here as buffer holding the last frame time signal*/
    if(overlap_select != MNON_OVERLAPPED) {
        memcpy(transf_buf, p_overlap, FRAME_LEN*sizeof(faac_real));
        memcpy(transf_buf+BLOCK_LEN_LONG, p_in_data, FRAME_LEN*sizeof(faac_real));
        memcpy(p_overlap, p_in_data, FRAME_LEN*sizeof(faac_real));
    } else {
        memcpy(transf_buf, p_in_data, 2*FRAME_LEN*sizeof(faac_real));
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
        default:
        case KBD_WINDOW:
            if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                first_window = hEncoder->kbd_window_long;
            else
                first_window = hEncoder->kbd_window_short;
            break;
        }

        switch (coderInfo->window_shape){
        case SINE_WINDOW:
        default:
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
        MDCT( &hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG );
        break;

    case LONG_SHORT_WINDOW :
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            p_out_mdct[i] = p_o_buf[i] * first_window[i];
        memcpy(p_out_mdct+BLOCK_LEN_LONG,p_o_buf+BLOCK_LEN_LONG,NFLAT_LS*sizeof(faac_real));
        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
            p_out_mdct[i+BLOCK_LEN_LONG+NFLAT_LS] = p_o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
        SetMemory(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(faac_real));
        MDCT( &hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG );
        break;

    case SHORT_LONG_WINDOW :
        SetMemory(p_out_mdct,0,NFLAT_LS*sizeof(faac_real));
        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
            p_out_mdct[i+NFLAT_LS] = p_o_buf[i+NFLAT_LS] * first_window[i];
        memcpy(p_out_mdct+NFLAT_LS+BLOCK_LEN_SHORT,p_o_buf+NFLAT_LS+BLOCK_LEN_SHORT,NFLAT_LS*sizeof(faac_real));
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
        MDCT( &hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_LONG );
        break;

    case ONLY_SHORT_WINDOW :
        p_o_buf += NFLAT_LS;
        for ( k=0; k < MAX_SHORT_WINDOWS; k++ ) {
            for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++ ){
                p_out_mdct[i] = p_o_buf[i] * first_window[i];
                p_out_mdct[i+BLOCK_LEN_SHORT] = p_o_buf[i+BLOCK_LEN_SHORT] * second_window[BLOCK_LEN_SHORT-i-1];
            }
            MDCT( &hEncoder->fft_tables, p_out_mdct, 2*BLOCK_LEN_SHORT );
            p_out_mdct += BLOCK_LEN_SHORT;
            p_o_buf += BLOCK_LEN_SHORT;
            first_window = second_window;
        }
        break;
    }

    if (transf_buf) FreeMemory(transf_buf);
}

void IFilterBank(faacEncStruct* hEncoder,
                 CoderInfo *coderInfo,
                 faac_real *p_in_data,
                 faac_real *p_out_data,
                 faac_real *p_overlap,
                 int overlap_select)
{
    faac_real *o_buf, *transf_buf, *overlap_buf;
    faac_real *first_window, *second_window;

    faac_real  *fp;
    int k, i;
    int block_type = coderInfo->block_type;

    transf_buf = (faac_real*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(faac_real));
    overlap_buf = (faac_real*)AllocMemory(2*BLOCK_LEN_LONG*sizeof(faac_real));

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
    memcpy(overlap_buf,p_overlap,BLOCK_LEN_LONG*sizeof(faac_real));
    o_buf = overlap_buf;

    /* Separate action for each Block Type */
    switch( block_type ) {
    case ONLY_LONG_WINDOW :
        memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizeof(faac_real));
        IMDCT( &hEncoder->fft_tables, transf_buf, 2*BLOCK_LEN_LONG );
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
        memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizeof(faac_real));
        IMDCT( &hEncoder->fft_tables, transf_buf, 2*BLOCK_LEN_LONG );
        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
            transf_buf[i] *= first_window[i];
        if (overlap_select != MNON_OVERLAPPED) {
            for ( i = 0 ; i < BLOCK_LEN_LONG; i++ )
                o_buf[i] += transf_buf[i];
            memcpy(o_buf+BLOCK_LEN_LONG,transf_buf+BLOCK_LEN_LONG,NFLAT_LS*sizeof(faac_real));
            for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] = transf_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
            SetMemory(o_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(faac_real));
        } else { /* overlap_select == NON_OVERLAPPED */
            for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                transf_buf[i+BLOCK_LEN_LONG+NFLAT_LS] *= second_window[BLOCK_LEN_SHORT-i-1];
            SetMemory(transf_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(faac_real));
        }
        break;

    case SHORT_LONG_WINDOW :
        memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizeof(faac_real));
        IMDCT( &hEncoder->fft_tables, transf_buf, 2*BLOCK_LEN_LONG );
        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
            transf_buf[i+NFLAT_LS] *= first_window[i];
        if (overlap_select != MNON_OVERLAPPED) {
            for ( i = 0 ; i < BLOCK_LEN_SHORT; i++ )
                o_buf[i+NFLAT_LS] += transf_buf[i+NFLAT_LS];
            memcpy(o_buf+BLOCK_LEN_SHORT+NFLAT_LS,transf_buf+BLOCK_LEN_SHORT+NFLAT_LS,NFLAT_LS*sizeof(faac_real));
            for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                o_buf[i+BLOCK_LEN_LONG] = transf_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
        } else { /* overlap_select == NON_OVERLAPPED */
            SetMemory(transf_buf,0,NFLAT_LS*sizeof(faac_real));
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
            memcpy(transf_buf,p_in_data,BLOCK_LEN_SHORT*sizeof(faac_real));
            IMDCT( &hEncoder->fft_tables, transf_buf, 2*BLOCK_LEN_SHORT );
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
        SetMemory(o_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizeof(faac_real));
        break;
    }

    if (overlap_select != MNON_OVERLAPPED)
        memcpy(p_out_data,o_buf,BLOCK_LEN_LONG*sizeof(faac_real));
    else  /* overlap_select == NON_OVERLAPPED */
        memcpy(p_out_data,transf_buf,2*BLOCK_LEN_LONG*sizeof(faac_real));

    /* save unused output data */
    memcpy(p_overlap,o_buf+BLOCK_LEN_LONG,BLOCK_LEN_LONG*sizeof(faac_real));

    if (overlap_buf) FreeMemory(overlap_buf);
    if (transf_buf) FreeMemory(transf_buf);
}

void specFilter(faac_real *freqBuff,
                int sampleRate,
                int lowpassFreq,
                int specLen
                )
{
    int lowpass,xlowpass;

    /* calculate the last line which is not zero */
    lowpass = (lowpassFreq * specLen) / (sampleRate>>1) + 1;
    xlowpass = (lowpass < specLen) ? lowpass : specLen ;

    SetMemory(freqBuff+xlowpass,0,(specLen-xlowpass)*sizeof(faac_real));
}

static faac_real Izero(faac_real x)
{
    const faac_real IzeroEPSILON = 1E-41;  /* Max error acceptable in Izero */
    faac_real sum, u, halfx, temp;
    int n;

    sum = u = n = 1;
    halfx = x/2.0;
    do {
        temp = halfx/(faac_real)n;
        n += 1;
        temp *= temp;
        u *= temp;
        sum += u;
    } while (u >= IzeroEPSILON*sum);

    return(sum);
}

static void CalculateKBDWindow(faac_real* win, faac_real alpha, int length)
{
    int i;
    faac_real IBeta;
    faac_real tmp;
    faac_real sum = 0.0;

    alpha *= M_PI;
    IBeta = 1.0/Izero(alpha);

    /* calculate lower half of Kaiser Bessel window */
    for(i=0; i<(length>>1); i++) {
        tmp = 4.0*(faac_real)i/(faac_real)length - 1.0;
        win[i] = Izero(alpha*FAAC_SQRT(1.0-tmp*tmp))*IBeta;
        sum += win[i];
    }

    sum = 1.0/sum;
    tmp = 0.0;

    /* calculate lower half of window */
    for(i=0; i<(length>>1); i++) {
        tmp += win[i];
        win[i] = FAAC_SQRT(tmp*sum);
    }
}

static void MDCT( FFT_Tables *fft_tables, faac_real *data, int N )
{
    faac_real *xi, *xr;
    faac_real tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    faac_real freq = TWOPI / N;
    faac_real cosfreq8, sinfreq8;
    int i, n;

    xi = (faac_real*)AllocMemory((N >> 2)*sizeof(faac_real));
    xr = (faac_real*)AllocMemory((N >> 2)*sizeof(faac_real));

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = FAAC_COS(freq);
    sfreq = FAAC_SIN(freq);
    cosfreq8 = FAAC_COS(freq * 0.125);
    sinfreq8 = FAAC_SIN(freq * 0.125);
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
    case BLOCK_LEN_SHORT * 2:
        fft( fft_tables, xr, xi, 6);
        break;
    case BLOCK_LEN_LONG * 2:
        fft( fft_tables, xr, xi, 9);
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

static void IMDCT( FFT_Tables *fft_tables, faac_real *data, int N)
{
    faac_real *xi, *xr;
    faac_real tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    faac_real freq = 2.0 * M_PI / N;
    faac_real fac, cosfreq8, sinfreq8;
    int i;

    xi = (faac_real*)AllocMemory((N >> 2)*sizeof(faac_real));
    xr = (faac_real*)AllocMemory((N >> 2)*sizeof(faac_real));

    /* Choosing to allocate 2/N factor to Inverse Xform! */
    fac = 2. / N; /* remaining 2/N from 4/N IFFT factor */

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = FAAC_COS(freq);
    sfreq = FAAC_SIN(freq);
    cosfreq8 = FAAC_COS(freq * 0.125);
    sinfreq8 = FAAC_SIN(freq * 0.125);
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
    case BLOCK_LEN_SHORT * 2:
        ffti( fft_tables, xr, xi, 6);
        break;
    case BLOCK_LEN_LONG * 2:
        ffti( fft_tables, xr, xi, 9);
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
