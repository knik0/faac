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
void MDCT( FFT_Tables *fft_tables, faac_real *data, int N );



static void InitializeTwiddles(faac_real *twiddles, int N)
{
    faac_real freq = TWOPI / N;
    faac_real cfreq = FAAC_COS(freq);
    faac_real sfreq = FAAC_SIN(freq);
    faac_real c = FAAC_COS(freq * 0.125);
    faac_real s = FAAC_SIN(freq * 0.125);
    int i;

    for (i = 0; i < (N >> 2); i++) {
        twiddles[2*i] = c;
        twiddles[2*i+1] = s;

        faac_real cold = c;
        c = c * cfreq - s * sfreq;
        s = s * cfreq + cold * sfreq;
    }
}

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

    hEncoder->fft_tables.mdct_twiddles_long = (faac_real*)AllocMemory(2*(2*BLOCK_LEN_LONG >> 2)*sizeof(faac_real));
    hEncoder->fft_tables.mdct_twiddles_short = (faac_real*)AllocMemory(2*(2*BLOCK_LEN_SHORT >> 2)*sizeof(faac_real));

    InitializeTwiddles(hEncoder->fft_tables.mdct_twiddles_long, 2*BLOCK_LEN_LONG);
    InitializeTwiddles(hEncoder->fft_tables.mdct_twiddles_short, 2*BLOCK_LEN_SHORT);
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

void MDCT( FFT_Tables *fft_tables, faac_real *data, int N )
{
    faac_real *xi, *xr;
    faac_real tempr, tempi, c, s;
    faac_real *twiddles;
    int i, n;
    int N4 = N >> 2;
    int N8 = N >> 3;
    int N2 = N >> 1;
    int N4_1 = (N >> 2) - 1;
    int N_N4_1 = N + (N >> 2) - 1;

    xi = (faac_real*)AllocMemory((N >> 2)*sizeof(faac_real));
    xr = (faac_real*)AllocMemory((N >> 2)*sizeof(faac_real));

    if (N == 2*BLOCK_LEN_LONG)
        twiddles = fft_tables->mdct_twiddles_long;
    else
        twiddles = fft_tables->mdct_twiddles_short;

    for (i = 0; i < N8; i++) {
        /* calculate real and imaginary parts of g(n) or G(p) */
        int i2 = 2 * i;
        n = N2 - 1 - i2;
        tempr = data [(N >> 2) + n] + data [N_N4_1 - n];

        tempi = data [(N >> 2) + i2] - data [N4_1 - i2];

        /* calculate pre-twiddled FFT input */
        c = twiddles[i2];
        s = twiddles[i2+1];
        xr[i] = tempr * c + tempi * s;
        xi[i] = tempi * c - tempr * s;
    }
    for (; i < N4; i++) {
        /* calculate real and imaginary parts of g(n) or G(p) */
        int i2 = 2 * i;
        n = N2 - 1 - i2;
        tempr = data [(N >> 2) + n] - data [N4_1 - n];

        tempi = data [(N >> 2) + i2] + data [N_N4_1 - i2];

        /* calculate pre-twiddled FFT input */
        c = twiddles[i2];
        s = twiddles[i2+1];
        xr[i] = tempr * c + tempi * s;
        xi[i] = tempi * c - tempr * s;
    }

    /* Perform in-place complex FFT of length N/4 */
    switch (N) {
    case BLOCK_LEN_SHORT * 2:
        fft( fft_tables, xr, xi, 6);
        break;
    case BLOCK_LEN_LONG * 2:
        fft( fft_tables, xr, xi, 9);
    }

    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < N4; i++) {
        /* get post-twiddled FFT output  */
        c = twiddles[2*i];
        s = twiddles[2*i+1];
        tempr = 2. * (xr[i] * c + xi[i] * s);
        tempi = 2. * (xi[i] * c - xr[i] * s);

        /* fill in output values */
        data [2 * i] = -tempr;   /* first half even */
        data [(N >> 1) - 1 - 2 * i] = tempi;  /* first half odd */
        data [(N >> 1) + 2 * i] = -tempi;  /* second half even */
        data [N - 1 - 2 * i] = tempr;  /* second half odd */
    }

    if (xr) FreeMemory(xr);
    if (xi) FreeMemory(xi);
}

