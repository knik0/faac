
#include <math.h>
#include <stdlib.h>

#include "tf_main.h"
#include "transfo.h"
#include "block.h"
#include "dolby_win.h"

#define DPI	3.14159265358979323846264338327950288

static double zero = 0;

static void vcopy( double src[], double dest[], int inc_src, int inc_dest, int vlen )
{
	int i;

	for( i=0; i<vlen; i++ ) {
		*dest = *src;
		dest += inc_dest;
		src  += inc_src;
	}
}

static void vmult( double src1[], double src2[], double dest[], 
            int inc_src1, int inc_src2, int inc_dest, int vlen )
{
	int i;

	for( i=0; i<vlen; i++ ) {
		*dest = *src1 * *src2;
		dest += inc_dest;
		src1 += inc_src1;
		src2 += inc_src2;
	}
}

static void vadd( double src1[], double src2[], double dest[], 
            int inc_src1, int inc_src2, int inc_dest, int vlen )
{
	int i;

	for( i=0; i<vlen; i++ ) {
		*dest = *src1 + *src2;
		dest += inc_dest;
		src1 += inc_src1;
		src2 += inc_src2;
	}
}
 

/* Calculate window */
void calc_window( double window[], int len, Window_shape wfun_select )
{
	int i;
	extern double dolby_win_1024[];
	extern double dolby_win_128[];

	switch(wfun_select)
    {
    case WS_FHG: 
		for( i=0; i<len; i++ )
			window[i] = sin((DPI/(2*len)) * (i + 0.5));
		break;
		
    case WS_DOLBY: 
		switch(len)
		{
		case BLOCK_LEN_SHORT:
			for( i=0; i<len; i++ ) 
				window[i] = dolby_win_128[i]; 
			break;
		case BLOCK_LEN_LONG:
			for( i=0; i<len; i++ ) 
				window[i] = dolby_win_1024[i]; 
			break;
		}
		break;
    }
}
#define MAX_SHIFT_LEN_LONG 4096

/* %%%%%%%%%%%%%%%%% MDCT - STUFF %%%%%%%%%%%%%%%% */

void mdct( double in_data[], double out_data[], int len )
{
	vcopy(in_data, out_data, 1, 1, len); 
	MDCT(out_data, len);
}

void buffer2freq(
  double           p_in_data[], 
  double           p_out_mdct[],
  double           p_overlap[],
  enum WINDOW_TYPE block_type,
  Window_shape     wfun_select,      /* offers the possibility to select different window functions */
  int              nlong,            /* shift length for long windows   */
  int              nmed,             /* shift length for medium windows */
  int              nshort,           /* shift length for short windows  */
  Mdct_in      overlap_select     /*  YT 970615 for son_PP */
)
{
	double         transf_buf[ 2*MAX_SHIFT_LEN_LONG ];
	double         windowed_buf[ 2*MAX_SHIFT_LEN_LONG ];
	double         *p_o_buf;
	int            k;

	double           window_long[MAX_SHIFT_LEN_LONG]; 
	double           window_long_prev[MAX_SHIFT_LEN_LONG]; 
	double           window_short[MAX_SHIFT_LEN_LONG]; 
	double           window_short_prev[MAX_SHIFT_LEN_LONG]; 
	double           *window_short_prev_ptr;

	int nflat_ls    = (nlong-nshort)/ 2; 
	int transfak_ls =  nlong/nshort; 
	
	static Window_shape wfun_select_prev=WS_FHG;
	static int firstTime=1;
	window_short_prev_ptr = window_short_prev ; 

	calc_window( window_long,      nlong, wfun_select ); 
	calc_window( window_long_prev, nlong, wfun_select_prev ); 
	calc_window( window_short,      nshort, wfun_select ); 
	calc_window( window_short_prev, nshort, wfun_select_prev ); 
	
	/* create / shift old values */
	/* We use p_overlap here as buffer holding the last frame time signal*/
	/* YT 970615 for son_pp */
	if (firstTime){
		firstTime=0;
		vcopy( &zero, transf_buf, 0, 1, nlong );
	} else
		vcopy( p_overlap, transf_buf, 1, 1, nlong );

	/* Append new data */
	vcopy( p_in_data, transf_buf+nlong, 1, 1, nlong );
	vcopy( p_in_data, p_overlap,        1, 1, nlong );

	/* Set ptr to transf-Buffer */
	p_o_buf = transf_buf;
	
	
	/* Separate action for each Block Type */
	switch( block_type ) {
	case ONLY_LONG_WINDOW :
		vmult( p_o_buf, window_long_prev, windowed_buf,       1, 1,  1, nlong );
		vmult( p_o_buf+nlong, window_long+nlong-1, windowed_buf+nlong, 1, -1, 1, nlong );
		mdct( windowed_buf, p_out_mdct, 2*nlong );    
		break;
		
	case LONG_SHORT_WINDOW :
		vmult( p_o_buf, window_long_prev, windowed_buf, 1, 1, 1, nlong );
		vcopy( p_o_buf+nlong, windowed_buf+nlong, 1, 1, nflat_ls );
		vmult( p_o_buf+nlong+nflat_ls, window_short+nshort-1, windowed_buf+nlong+nflat_ls, 1, -1, 1, nshort );
		vcopy( &zero, windowed_buf+2*nlong-1, 0, -1, nflat_ls );
		mdct( windowed_buf, p_out_mdct, 2*nlong );
		break;

	case SHORT_LONG_WINDOW :
		vcopy( &zero, windowed_buf, 0, 1, nflat_ls );
		vmult( p_o_buf+nflat_ls, window_short_prev_ptr, windowed_buf+nflat_ls, 1, 1, 1, nshort );
		vcopy( p_o_buf+nflat_ls+nshort, windowed_buf+nflat_ls+nshort, 1, 1, nflat_ls );
		vmult( p_o_buf+nlong, window_long+nlong-1, windowed_buf+nlong, 1, -1, 1, nlong );
		mdct( windowed_buf, p_out_mdct, 2*nlong );
		break;

	case ONLY_SHORT_WINDOW :
		p_o_buf += nflat_ls;
		for (k=transfak_ls-1; k-->=0; ) {
			vmult( p_o_buf,        window_short_prev_ptr,          windowed_buf,        1, 1,  1, nshort );
			vmult( p_o_buf+nshort, window_short+nshort-1, windowed_buf+nshort, 1, -1, 1, nshort );
			mdct( windowed_buf, p_out_mdct, 2*nshort );

			p_out_mdct += nshort;
			/* YT 970615 for sonPP*/
			p_o_buf += nshort;
			window_short_prev_ptr = window_short; 
		}
		break;
	}

	/* Set output data 
	vcopy(transf_buf, p_out_mdct,1, 1, nlong); */
	
	/* --- Save old window shape --- */
	wfun_select_prev = wfun_select;
}

/***********************************************************************************************/ 
