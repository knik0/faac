
#include <math.h>
#include <stdlib.h>

#include "tf_main.h"
#include "transfo.h"
#include "block.h"
#include "dolby_win.h"

#define DPI	3.14159265358979323846264338327950288

double zero = 0;

void vcopy( double src[], double dest[], int inc_src, int inc_dest, int vlen )
{
	int i;

	for( i=0; i<vlen; i++ ) {
		*dest = *src;
		dest += inc_dest;
		src  += inc_src;
	}
}

void vmult1(double src1[], double src2[], double dest[], int vlen )
{
	int i;

	for( i=0; i<vlen; i++ ) {
		*dest = *src1 * *src2;
		dest++;
		src1++;
		src2++;
	}
}

void vmult2( double src1[], double src2[], double dest[], int vlen)
{
	int i;

	for( i=0; i<vlen; i++ ) {
		*dest = *src1 * *src2;
		dest++;
		src1++;
		src2--;
	}
}

void vadd( double src1[], double src2[], double dest[], 
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
  double           window_med[MAX_SHIFT_LEN_LONG]; 
  double           window_med_prev[MAX_SHIFT_LEN_LONG]; 
  double           window_short[MAX_SHIFT_LEN_LONG]; 
  double           window_short_prev[MAX_SHIFT_LEN_LONG]; 
  double           *window_short_prev_ptr;


  int nflat_ls    = (nlong-nshort)/ 2; 
  int transfak_ls =  nlong/nshort; 
  int nflat_lm    = (nlong-nmed)  / 2; 
  int transfak_lm =  nlong/nmed; 
  int nflat_ms    = (nmed-nshort) / 2; 
  int transfak_ms =  nmed/nshort; 
 
  static Window_shape wfun_select_prev=WS_FHG;
  static int firstTime=1;
  window_short_prev_ptr = window_short_prev ; 


// if( (nlong%nshort) || (nlong > MAX_SHIFT_LEN_LONG) || (nshort > MAX_SHIFT_LEN_LONG/2) ) { 
//    CommonExit( 1, "mdct_analysis: Problem with window length" ); } 
//  if( (nlong%nmed) || (nmed%nshort) || (nmed > MAX_SHIFT_LEN_LONG/2) ) { 
//    CommonExit( 1, "mdct_analysis: Problem with window length" );  } 
//  if( transfak_lm%2 ) { 
//    CommonExit( 1, "mdct_analysis: Problem with window length" );  } 
  
  /* --- PATCH: Use WS_FHG window shape if we start with SHORT windows --- */
  if  (block_type==LONG_SHORT_WINDOW || block_type==ONLY_SHORT_WINDOW ){
    wfun_select=WS_FHG; } 
  /* --- PATCH  End  --- */
  calc_window( window_long,      nlong, wfun_select ); 
  calc_window( window_long_prev, nlong, wfun_select_prev ); 
  calc_window( window_med, nmed, wfun_select ); 
  calc_window( window_med_prev, nmed, wfun_select_prev ); 
  calc_window( window_short,      nshort, wfun_select ); 
  calc_window( window_short_prev, nshort, wfun_select_prev ); 
  
  /* create / shift old values */
  /* We use p_overlap here as buffer holding the last frame time signal*/
/* YT 970615 for son_pp */
if(overlap_select != MNON_OVERLAPPED){
  if (firstTime){
    firstTime=0;
    vcopy( &zero, transf_buf, 0, 1, nlong );
  }
  else
    vcopy( p_overlap, transf_buf, 1, 1, nlong );

  /* Append new data */
  vcopy( p_in_data, transf_buf+nlong, 1, 1, nlong );
  vcopy( p_in_data, p_overlap,        1, 1, nlong );
}
else{
	vcopy( p_in_data, transf_buf, 1, 1, 2*nlong );
}

  /* Set ptr to transf-Buffer */
  p_o_buf = transf_buf;
  
  
  /* Separate action for each Block Type */
  switch( block_type ) {
   case ONLY_LONG_WINDOW :
     vmult1( p_o_buf, window_long_prev, windowed_buf, nlong );
     vmult2( p_o_buf+nlong, window_long+nlong-1, windowed_buf+nlong, nlong );
     mdct( windowed_buf, p_out_mdct, 2*nlong );    
     break;
   
   case LONG_SHORT_WINDOW :
    vmult1( p_o_buf, window_long_prev, windowed_buf, nlong );
    vcopy( p_o_buf+nlong, windowed_buf+nlong, 1, 1, nflat_ls );
    vmult2( p_o_buf+nlong+nflat_ls, window_short+nshort-1, windowed_buf+nlong+nflat_ls, nshort );
    vcopy( &zero, windowed_buf+2*nlong-1, 0, -1, nflat_ls );
    mdct( windowed_buf, p_out_mdct, 2*nlong );
    break;

   case SHORT_LONG_WINDOW :
    vcopy( &zero, windowed_buf, 0, 1, nflat_ls );
    vmult1( p_o_buf+nflat_ls, window_short_prev_ptr, windowed_buf+nflat_ls, nshort );
    vcopy( p_o_buf+nflat_ls+nshort, windowed_buf+nflat_ls+nshort, 1, 1, nflat_ls );
    vmult2( p_o_buf+nlong, window_long+nlong-1, windowed_buf+nlong, nlong );
    mdct( windowed_buf, p_out_mdct, 2*nlong );
    break;

   case ONLY_SHORT_WINDOW :
	if(overlap_select != MNON_OVERLAPPED){ /* YT 970615 for sonPP */
    	p_o_buf += nflat_ls;
	}
    for (k=transfak_ls-1; k-->=0; ) {
      vmult1( p_o_buf,        window_short_prev_ptr, windowed_buf, nshort );
      vmult2( p_o_buf+nshort, window_short+nshort-1, windowed_buf+nshort, nshort );
      mdct( windowed_buf, p_out_mdct, 2*nshort );

      p_out_mdct += nshort;
	/* YT 970615 for sonPP*/
	  if(overlap_select != MNON_OVERLAPPED) p_o_buf += nshort;
      else 	p_o_buf += 2*nshort;
      window_short_prev_ptr = window_short; 
    }
    break;
   default :
    break;
//      CommonExit( 1, "mdct_synthesis: Unknown window type" ); 
  }

  /* Set output data 
  vcopy(transf_buf, p_out_mdct,1, 1, nlong); */
  
  /* --- Save old window shape --- */
  wfun_select_prev = wfun_select;
}

void imdct(double in_data[], double out_data[], int len)
{
  vcopy(in_data, out_data, 1, 1, len/2);
  IMDCT(out_data, len);
}

void freq2buffer(
  double           p_in_data[], 
  double           p_out_data[],
  double           p_overlap[],
  enum WINDOW_TYPE block_type,
  int              nlong,            /* shift length for long windows   */
  int              nmed,             /* shift length for medium windows */
  int              nshort,           /* shift length for short windows  */
  Window_shape     wfun_select,      /* offers the possibility to select different window functions */
  Mdct_in	   overlap_select    /* select imdct output *TK*	*/
				     /* switch (overlap_select) {	*/
				     /* case OVERLAPPED:		*/
				     /*   p_out_data[]			*/
				     /*   = overlapped and added signal */
				     /*		(bufferlength: nlong)	*/
				     /* case MNON_OVERLAPPED:		*/
				     /*   p_out_data[]			*/
				     /*   = non overlapped signal	*/
				     /*		(bufferlength: 2*nlong)	*/
  )
{
  double           *o_buf, transf_buf[ 2*MAX_SHIFT_LEN_LONG ];
  double           overlap_buf[ 2*MAX_SHIFT_LEN_LONG ]; 
 
  double           window_long[MAX_SHIFT_LEN_LONG]; 
  double           window_long_prev[MAX_SHIFT_LEN_LONG]; 
  double           window_med[MAX_SHIFT_LEN_LONG]; 
  double           window_med_prev[MAX_SHIFT_LEN_LONG]; 
  double           window_short[MAX_SHIFT_LEN_LONG]; 
  double           window_short_prev[MAX_SHIFT_LEN_LONG]; 
  double           *window_short_prev_ptr;
 
  double  *fp; 
  int     k; 
 
  int nflat_ls    = (nlong-nshort)/ 2; 
  int transfak_ls =  nlong/nshort; 
  int nflat_lm    = (nlong-nmed)  / 2; 
  int transfak_lm =  nlong/nmed; 
  int nflat_ms    = (nmed-nshort) / 2; 
  int transfak_ms =  nmed/nshort;  
 
  static Window_shape wfun_select_prev=WS_FHG;
  window_short_prev_ptr=window_short_prev ; 


  if( (nlong%nshort) || (nlong > MAX_SHIFT_LEN_LONG) || (nshort > MAX_SHIFT_LEN_LONG/2) ) { 
//    CommonExit( 1, "mdct_synthesis: Problem with window length" ); 
  } 
  if( (nlong%nmed) || (nmed%nshort) || (nmed > MAX_SHIFT_LEN_LONG/2) ) { 
//    CommonExit( 1, "mdct_synthesis: Problem with window length" ); 
  } 
  if( transfak_lm%2 ) { 
//    CommonExit( 1, "mdct_synthesis: Problem with window length" ); 
  } 



  /* --- PATCH: Use WS_FHG window shape if we start 
                with SHORT windows            --- */
  if  (block_type==LONG_SHORT_WINDOW ||
       block_type==ONLY_SHORT_WINDOW ){
    wfun_select=WS_FHG;
  } 
  /* --- PATCH  End  --- */
  
  calc_window( window_long,      nlong, wfun_select ); 
  calc_window( window_long_prev, nlong, wfun_select_prev ); 
  calc_window( window_med, nmed, wfun_select ); 
  calc_window( window_med_prev, nmed, wfun_select_prev );
  calc_window( window_short,      nshort, wfun_select ); 
  calc_window( window_short_prev_ptr, nshort, wfun_select_prev ); 
  
 
  /* Assemble overlap buffer */ 
  vcopy( p_overlap, overlap_buf, 1, 1, nlong ); 
  o_buf = overlap_buf; 
 

  /* Separate action for each Block Type */ 
   switch( block_type ) { 
   case ONLY_LONG_WINDOW : 
    imdct( p_in_data, transf_buf, 2*nlong ); 
    vmult1( transf_buf, window_long_prev, transf_buf, nlong ); 
    if (overlap_select != MNON_OVERLAPPED) {
      vadd( transf_buf, o_buf, o_buf, 1, 1, 1, nlong ); 
      vmult2( transf_buf+nlong, window_long+nlong-1, o_buf+nlong, nlong ); 
    }
    else { /* overlap_select == NON_OVERLAPPED */
      vmult2( transf_buf+nlong, window_long+nlong-1, transf_buf+nlong, nlong );
    }

    break; 
 
  case LONG_SHORT_WINDOW : 
    imdct( p_in_data, transf_buf, 2*nlong ); 
    vmult1( transf_buf, window_long_prev, transf_buf, nlong ); 
    if (overlap_select != MNON_OVERLAPPED) {
      vadd( transf_buf, o_buf, o_buf, 1, 1, 1, nlong ); 
      vcopy( transf_buf+nlong, o_buf+nlong, 1, 1, nflat_ls ); 
      vmult2( transf_buf+nlong+nflat_ls, window_short+nshort-1, o_buf+nlong+nflat_ls, nshort ); 
      vcopy( &zero, o_buf+2*nlong-1, 0, -1, nflat_ls ); 
    }
    else { /* overlap_select == NON_OVERLAPPED */
      vmult2( transf_buf+nlong+nflat_ls, window_short+nshort-1, transf_buf+nlong+nflat_ls, nshort ); 
      vcopy( &zero, transf_buf+2*nlong-1, 0, -1, nflat_ls ); 
    }
    break; 
    
  case SHORT_LONG_WINDOW : 
    imdct( p_in_data, transf_buf, 2*nlong ); 
    vmult1( transf_buf+nflat_ls, window_short_prev_ptr, transf_buf+nflat_ls, nshort ); 
    if (overlap_select != MNON_OVERLAPPED) {
      vadd( transf_buf+nflat_ls, o_buf+nflat_ls, o_buf+nflat_ls, 1, 1, 1, nshort ); 
      vcopy( transf_buf+nflat_ls+nshort, o_buf+nflat_ls+nshort, 1, 1, nflat_ls ); 
      vmult2( transf_buf+nlong, window_long+nlong-1, o_buf+nlong, nlong ); 
    }
    else { /* overlap_select == NON_OVERLAPPED */
      vcopy( &zero, transf_buf, 0, 1, nflat_ls);
      vmult2( transf_buf+nlong, window_long+nlong-1, transf_buf+nlong, nlong);
    }
    break; 
 
  case ONLY_SHORT_WINDOW : 
    if (overlap_select != MNON_OVERLAPPED) {
      fp = o_buf + nflat_ls; 
    }
    else { /* overlap_select == NON_OVERLAPPED */
      fp = transf_buf;
    }
    for( k = transfak_ls-1; k-->= 0; ) { 
      if (overlap_select != MNON_OVERLAPPED) {
        imdct( p_in_data, transf_buf, 2*nshort ); 
        vmult1( transf_buf, window_short_prev_ptr, transf_buf, nshort ); 
        vadd( transf_buf, fp, fp, 1, 1, 1, nshort ); 
        vmult2( transf_buf+nshort, window_short+nshort-1, fp+nshort, nshort ); 
        p_in_data += nshort; 
        fp        += nshort; 
        window_short_prev_ptr = window_short; 
      }
      else { /* overlap_select == NON_OVERLAPPED */
        imdct( p_in_data, fp, 2*nshort );
        vmult1( fp, window_short_prev_ptr, fp, nshort ); 
        vmult2( fp+nshort, window_short+nshort-1, fp+nshort, nshort ); 
        p_in_data += nshort; 
        fp        += 2*nshort;
        window_short_prev_ptr = window_short; 
      }
    } 
    vcopy( &zero, o_buf+2*nlong-1, 0, -1, nflat_ls ); 
    break;     
   default :
	   break;
    //CommonExit( 1, "mdct_synthesis: Unknown window type" ); 
  } 
 
  if (overlap_select != MNON_OVERLAPPED) {
    vcopy( o_buf, p_out_data, 1, 1, nlong ); 
  }
  else { /* overlap_select == NON_OVERLAPPED */
    vcopy( transf_buf, p_out_data, 1, 1, 2*nlong ); 
  }
  
  /* save unused output data */ 
  vcopy( o_buf+nlong, p_overlap, 1, 1, nlong ); 

  /* --- Save old window shape --- */
  wfun_select_prev = wfun_select;
  
} 

/***********************************************************************************************/ 
