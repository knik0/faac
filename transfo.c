#include <math.h>
#include <string.h>
#include "all.h"
#include "transfo.h"
#include "kbd_win.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923
#endif

#define NFLAT_LS 448  // (BLOCK_LEN_LONG - BLOCK_LEN_SHORT) / 2

double       sin_window_long[BLOCK_LEN_LONG];
double       sin_window_short[BLOCK_LEN_SHORT];
int sizedouble; // temp value

/*******************************************************************************
                               Fast MDCT & IMDCT Code
*******************************************************************************/
void MDCT (fftw_real *data, int N)
{
    fftw_real tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    fftw_real freq = 2.0 * M_PI / N;
    fftw_real fac,cosfreq8,sinfreq8;
    int i, n;
    int b = N >> 1;
    int N4 = N >> 2;
    int N2 = N >> 1;
    int a = N - b;
    int a2 = a >> 1;
    int a4 = a >> 2;
    int b4 = b >> 2;
    int unscambled;

    /* Choosing to allocate 2/N factor to Inverse Xform! */
    fac = 2.; /* 2 from MDCT inverse  to forward */

    /* prepare for recurrence relation in pre-twiddle */
    cfreq = cos (freq);
    sfreq = sin (freq);
    cosfreq8 = cos (freq * 0.125);
    sinfreq8 = sin (freq * 0.125);
    c = cosfreq8;
    s = sinfreq8;

    for (i = 0; i < N4; i++) {
		/* calculate real and imaginary parts of g(n) or G(p) */

		n = N / 2 - 1 - 2 * i;
		if (i < b4) {
			tempr = data [a2 + n] + data [N + a2 - 1 - n]; /* use second form of e(n) for n = N / 2 - 1 - 2i */
		} else {
			tempr = data [a2 + n] - data [a2 - 1 - n]; /* use first form of e(n) for n = N / 2 - 1 - 2i */
		}
		n = 2 * i;
		if (i < a4) {
			tempi = data [a2 + n] - data [a2 - 1 - n]; /* use first form of e(n) for n=2i */
		} else {
			tempi = data [a2 + n] + data [N + a2 - 1 - n]; /* use second form of e(n) for n=2i*/
		}

		/* calculate pre-twiddled FFT input */
		FFTarray [i].re = tempr * c + tempi * s;
		FFTarray [i].im = tempi * c - tempr * s;

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
    }

    /* Perform in-place complex FFT of length N/4 */
    switch (N) {
	case 256: pfftw_64(FFTarray);
		break;
	case 2048:pfftw_512(FFTarray);
	}

    /* prepare for recurrence relations in post-twiddle */
    c = cosfreq8;
    s = sinfreq8;

    /* post-twiddle FFT output and then get output data */
    for (i = 0; i < N4; i++) {

		/* get post-twiddled FFT output  */
		/* Note: fac allocates 4/N factor from IFFT to forward and inverse */
	switch (N) {
	case 256:
		unscambled = unscambled64[i];
		break;
	case 2048:
		unscambled = unscambled512[i];
	}
	tempr = fac * (FFTarray [unscambled].re * c + FFTarray [unscambled].im * s);
	tempi = fac * (FFTarray [unscambled].im * c - FFTarray [unscambled].re * s);


		/* fill in output values */
		data [2 * i] = -tempr;   /* first half even */
		data [N2 - 1 - 2 * i] = tempi;  /* first half odd */
		data [N2 + 2 * i] = -tempi;  /* second half even */
		data [N - 1 - 2 * i] = tempr;  /* second half odd */

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
    }
}

void IMDCT(fftw_real *data, int N)
{
	fftw_real tempr, tempi, c, s, cold, cfreq, sfreq; /* temps for pre and post twiddle */
    	fftw_real freq = 2.0 * M_PI / N;
	fftw_real fac, cosfreq8, sinfreq8;
	int i;
	int Nd2 = N >> 1;
	int Nd4 = N >> 2;
	int Nd8 = N >> 3;
	int unscambled;

	/* Choosing to allocate 2/N factor to Inverse Xform! */
	fac = 2. / N; /* remaining 2/N from 4/N IFFT factor */

	/* prepare for recurrence relation in pre-twiddle */
	cfreq = cos (freq);
	sfreq = sin (freq);
	cosfreq8 = cos (freq * 0.125);
	sinfreq8 = sin (freq * 0.125);
	c = cosfreq8;
	s = sinfreq8;

	for (i = 0; i < Nd4; i++) {

		/* calculate real and imaginary parts of g(n) or G(p) */
		tempr = -data [2 * i];
		tempi = data [Nd2 - 1 - 2 * i];

		/* calculate pre-twiddled FFT input */
	switch (N) {
	case 256:
		unscambled = unscambled64[i];
		break;
	case 2048:
		unscambled = unscambled512[i];
	}
		FFTarray [unscambled].re = tempr * c - tempi * s;
		FFTarray [unscambled].im = tempi * c + tempr * s;

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
	}

	/* Perform in-place complex IFFT of length N/4 */
    switch (N) {
	case 256: pfftwi_64(FFTarray);
		break;
	case 2048:pfftwi_512(FFTarray);
	}


	/* prepare for recurrence relations in post-twiddle */
	c = cosfreq8;
	s = sinfreq8;

	/* post-twiddle FFT output and then get output data */
	for (i = 0; i < Nd4; i++) {

		/* get post-twiddled FFT output  */
	tempr = fac * (FFTarray[i].re * c - FFTarray[i].im * s);
    	tempi = fac * (FFTarray[i].im * c + FFTarray[i].re * s);

		/* fill in output values */
		data [Nd2 + Nd4 - 1 - 2 * i] = tempr;
		if (i < Nd8) {
			data [Nd2 + Nd4 + 2 * i] = tempr;
		} else {
			data [2 * i - Nd4] = -tempr;
		}
		data [Nd4 + 2 * i] = tempi;
		if (i < Nd8) {
			data [Nd4 - 1 - 2 * i] = -tempi;
		} else {
			data [Nd4 + N - 1 - 2*i] = tempi;
		}

		/* use recurrence to prepare cosine and sine for next value of i */
		cold = c;
		c = c * cfreq - s * sfreq;
		s = s * cfreq + cold * sfreq;
	}
}

void make_MDCT_windows(void)
{
int i;
for( i=0; i<BLOCK_LEN_LONG; i++ )
	sin_window_long[i] = sin((M_PI/(2*BLOCK_LEN_LONG)) * (i + 0.5));
for( i=0; i<BLOCK_LEN_SHORT; i++ )
	sin_window_short[i] = sin((M_PI/(2*BLOCK_LEN_SHORT)) * (i + 0.5));
sizedouble = sizeof (double);
}
/*******************************************************************************
                                    T/F mapping
*******************************************************************************/
void buffer2freq(double           p_in_data[],
		 double           p_out_mdct[],
		 double           p_overlap[],
		 enum WINDOW_TYPE block_type,
		 Window_shape     wfun_select,      /*  current window shape */
		 Window_shape     wfun_select_prev, /*  previous window shape */
		 Mdct_in          overlap_select
		 )
{
	double         transf_buf[ 2*BLOCK_LEN_LONG ];
	double         *p_o_buf, *first_window, *second_window;
	int            k,i;

	static int firstTime=1;

	/* create / shift old values */
	/* We use p_overlap here as buffer holding the last frame time signal*/
	if(overlap_select != MNON_OVERLAPPED){
		if (firstTime){
			firstTime=0;
                        memset(transf_buf,0,BLOCK_LEN_LONG*sizedouble);
		}
		else
                memcpy(transf_buf,p_overlap,BLOCK_LEN_LONG*sizedouble);
                memcpy(transf_buf+BLOCK_LEN_LONG,p_in_data,BLOCK_LEN_LONG*sizedouble);
                memcpy(p_overlap,p_in_data,BLOCK_LEN_LONG*sizedouble);
	}
	else{
                memcpy(transf_buf,p_in_data,2*BLOCK_LEN_LONG*sizedouble);
	}

        /*  Window shape processing */
        switch (wfun_select_prev){
                case WS_SIN:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                                          first_window = sin_window_long;
                                     else
                                          first_window = sin_window_short;
                                break;
                case WS_KBD:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                                          first_window = kbd_window_long;
                                     else
                                          first_window = kbd_window_short;
                                break;
                }

        switch (wfun_select){
                case WS_SIN:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                                          second_window = sin_window_long;
                                     else
                                          second_window = sin_window_short;
                                break;
                case WS_KBD:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                                          second_window = kbd_window_long;
                                     else
                                          second_window = kbd_window_short;
                                break;
                }

	/* Set ptr to transf-Buffer */
	p_o_buf = transf_buf;

	/* Separate action for each Block Type */
	switch( block_type ) {
	case ONLY_LONG_WINDOW :
                for ( i = 0 ; i < BLOCK_LEN_LONG ; i++){
                        p_out_mdct[i] = p_o_buf[i] * first_window[i];
                        p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
                        }
		MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
		break;

	case LONG_SHORT_WINDOW :
//                for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
//                        p_out_mdct[i] = p_o_buf[i] * first_window[i];
                memcpy(p_out_mdct+BLOCK_LEN_LONG,p_o_buf+BLOCK_LEN_LONG,NFLAT_LS*sizedouble);
//                for ( ; i < NFLAT_LS + BLOCK_LEN_LONG; i++){
//                        p_out_mdct[i] = 1.0;
//                        p_out_mdct[i+NFLAT_LS+BLOCK_LEN_SHORT] = 0.0;
//                        }
                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                        p_out_mdct[i+BLOCK_LEN_LONG+NFLAT_LS] = p_o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
                memset(p_out_mdct+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizedouble);
		MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
		break;

	case SHORT_LONG_WINDOW :
                memset(p_out_mdct,0,NFLAT_LS*sizedouble);
//                for ( i = 0 ; i < NFLAT_LS ; i++){
//                        p_out_mdct[i] = 0.0;
//                        p_out_mdct[i+NFLAT_LS+BLOCK_LEN_SHORT] = 1.0;
//                        }
                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                        p_out_mdct[i+NFLAT_LS] = p_o_buf[i+NFLAT_LS] * first_window[i];
                memcpy(p_out_mdct+NFLAT_LS+BLOCK_LEN_SHORT,p_o_buf+NFLAT_LS+BLOCK_LEN_SHORT,NFLAT_LS*sizedouble);
                for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                        p_out_mdct[i+BLOCK_LEN_LONG] = p_o_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
		MDCT( p_out_mdct, 2*BLOCK_LEN_LONG );
		break;

	case ONLY_SHORT_WINDOW :
		if(overlap_select != MNON_OVERLAPPED){ /* YT 970615 for sonPP */
			p_o_buf += NFLAT_LS;
		}
		for ( k=0; k < MAX_SHORT_WINDOWS; k++ ) {
                        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++ ){
                                p_out_mdct[i] = p_o_buf[i] * first_window[i];
                                p_out_mdct[i+BLOCK_LEN_SHORT] = p_o_buf[i+BLOCK_LEN_SHORT] * second_window[BLOCK_LEN_SHORT-i-1];
                                }
			MDCT( p_out_mdct, 2*BLOCK_LEN_SHORT );

			p_out_mdct += BLOCK_LEN_SHORT;
			if(overlap_select != MNON_OVERLAPPED) p_o_buf += BLOCK_LEN_SHORT;
        			else 	p_o_buf += 2*BLOCK_LEN_SHORT;
                        first_window = second_window;
		}
		break;
	}
}

void freq2buffer(double           p_in_data[],
		 double           p_out_data[],
		 double           p_overlap[],
		 enum WINDOW_TYPE block_type,
		 Window_shape     wfun_select,      /*  current window shape */
		 Window_shape     wfun_select_prev, /*  previous window shape */
		 Mdct_in	  overlap_select
		 )
{
	double           *o_buf, transf_buf[ 2*BLOCK_LEN_LONG ];
	double           overlap_buf[ 2*BLOCK_LEN_LONG ], *first_window, *second_window;

	double  *fp;
	int     k,i;

        /*  Window shape processing */
        switch (wfun_select_prev){
                case WS_SIN:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                                          first_window = sin_window_long;
                                     else
                                          first_window = sin_window_short;
                                break;
                case WS_KBD:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == LONG_SHORT_WINDOW))
                                          first_window = kbd_window_long;
                                     else
                                          first_window = kbd_window_short;
                                break;
                }

        switch (wfun_select){
                case WS_SIN:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                                          second_window = sin_window_long;
                                     else
                                          second_window = sin_window_short;
                                break;
                case WS_KBD:  if ( (block_type == ONLY_LONG_WINDOW) || (block_type == SHORT_LONG_WINDOW))
                                          second_window = kbd_window_long;
                                     else
                                          second_window = kbd_window_short;
                                break;
                }

	/* Assemble overlap buffer */
        memcpy(overlap_buf,p_overlap,BLOCK_LEN_LONG*sizedouble);
	o_buf = overlap_buf;

	/* Separate action for each Block Type */
	switch( block_type ) {
	case ONLY_LONG_WINDOW :
                memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizedouble);
		IMDCT( transf_buf, 2*BLOCK_LEN_LONG );
                for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                        transf_buf[i] *= first_window[i];
		if (overlap_select != MNON_OVERLAPPED) {
                        for ( i = 0 ; i < BLOCK_LEN_LONG; i++ ){
                            o_buf[i] += transf_buf[i];
                            o_buf[i+BLOCK_LEN_LONG] = transf_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
                            }
		}
		else { /* overlap_select == NON_OVERLAPPED */
                        for ( i = 0 ; i < BLOCK_LEN_LONG; i++ )
                            transf_buf[i+BLOCK_LEN_LONG] *= second_window[BLOCK_LEN_LONG-i-1];
		}
		break;

	case LONG_SHORT_WINDOW :
                memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizedouble);
		IMDCT( transf_buf, 2*BLOCK_LEN_LONG );
                for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                        transf_buf[i] *= first_window[i];
		if (overlap_select != MNON_OVERLAPPED) {
                        for ( i = 0 ; i < BLOCK_LEN_LONG; i++ )
                            o_buf[i] += transf_buf[i];
                        memcpy(o_buf+BLOCK_LEN_LONG,transf_buf+BLOCK_LEN_LONG,NFLAT_LS*sizedouble);
                        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                                o_buf[i+BLOCK_LEN_LONG+NFLAT_LS] = transf_buf[i+BLOCK_LEN_LONG+NFLAT_LS] * second_window[BLOCK_LEN_SHORT-i-1];
                        memset(o_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizedouble);
		}
		else { /* overlap_select == NON_OVERLAPPED */
                        for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                                transf_buf[i+BLOCK_LEN_LONG+NFLAT_LS] *= second_window[BLOCK_LEN_SHORT-i-1];
                        memset(transf_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizedouble);
		}
		break;

	case SHORT_LONG_WINDOW :
                memcpy(transf_buf, p_in_data,BLOCK_LEN_LONG*sizedouble);
		IMDCT( transf_buf, 2*BLOCK_LEN_LONG );
                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++)
                        transf_buf[i+NFLAT_LS] *= first_window[i];
		if (overlap_select != MNON_OVERLAPPED) {
                        for ( i = 0 ; i < BLOCK_LEN_SHORT; i++ )
                            o_buf[i+NFLAT_LS] += transf_buf[i+NFLAT_LS];
                        memcpy(o_buf+BLOCK_LEN_SHORT+NFLAT_LS,transf_buf+BLOCK_LEN_SHORT+NFLAT_LS,NFLAT_LS*sizedouble);
                        for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                                o_buf[i+BLOCK_LEN_LONG] = transf_buf[i+BLOCK_LEN_LONG] * second_window[BLOCK_LEN_LONG-i-1];
		}
		else { /* overlap_select == NON_OVERLAPPED */
                        memset(transf_buf,0,NFLAT_LS*sizedouble);
                for ( i = 0 ; i < BLOCK_LEN_LONG ; i++)
                        transf_buf[i+BLOCK_LEN_LONG] *= second_window[BLOCK_LEN_LONG-i-1];
		}
		break;

	case ONLY_SHORT_WINDOW :
		if (overlap_select != MNON_OVERLAPPED) {
			fp = o_buf + NFLAT_LS;
		}
		else { /* overlap_select == NON_OVERLAPPED */
			fp = transf_buf;
		}
		for ( k=0; k < MAX_SHORT_WINDOWS; k++ ) {
                        memcpy(transf_buf,p_in_data,BLOCK_LEN_SHORT*sizedouble);
               		IMDCT( transf_buf, 2*BLOCK_LEN_SHORT );
			p_in_data += BLOCK_LEN_SHORT;
			if (overlap_select != MNON_OVERLAPPED) {
                                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++){
                                        transf_buf[i] *= first_window[i];
                                        fp[i] += transf_buf[i];
                                        fp[i+BLOCK_LEN_SHORT] = transf_buf[i+BLOCK_LEN_SHORT] * second_window[BLOCK_LEN_SHORT-i-1];
                                        }
				fp    += BLOCK_LEN_SHORT;
			}
			else { /* overlap_select == NON_OVERLAPPED */
                                for ( i = 0 ; i < BLOCK_LEN_SHORT ; i++){
                                        fp[i] *= first_window[i];
                                        fp[i+BLOCK_LEN_SHORT] *= second_window[BLOCK_LEN_SHORT-i-1];
                                        }
				fp    += 2*BLOCK_LEN_SHORT;
			}
                        first_window = second_window;
                }
                memset(o_buf+BLOCK_LEN_LONG+NFLAT_LS+BLOCK_LEN_SHORT,0,NFLAT_LS*sizedouble);
		break;
	}

	if (overlap_select != MNON_OVERLAPPED) {
                memcpy(p_out_data,o_buf,BLOCK_LEN_LONG*sizedouble);
	}
	else { /* overlap_select == NON_OVERLAPPED */
                memcpy(p_out_data,transf_buf,2*BLOCK_LEN_LONG*sizedouble);
	}

	/* save unused output data */
        memcpy(p_overlap,o_buf+BLOCK_LEN_LONG,BLOCK_LEN_LONG*sizedouble);
}

/******************************************************************************/

void specFilter (double p_in[],
                 double p_out[],
		 int  samp_rate,
		 int lowpass_freq,
		 int    specLen
		 )
{
	int lowpass,xlowpass;

	/* calculate the last line which is not zero */
	lowpass = (lowpass_freq * specLen) / (samp_rate>>1) + 1;
	xlowpass = (lowpass < specLen) ? lowpass : specLen ;

	if( p_out != p_in )  memcpy(p_out,p_in,specLen*sizedouble);
        memset(p_out+xlowpass,0,(specLen-xlowpass)*sizedouble);
}

