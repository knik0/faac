/*
 *	Function prototypes for psychoacoustics
 *
 *	Copyright (c) 1999 M. Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**************************************************************************
  Version Control Information			Method: CVS
  Identifiers:
  $Revision: 1.23 $
  $Date: 2000/10/08 20:32:33 $ (check in)
  $Author: menno $
  *************************************************************************/

#include "interface.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif
#ifndef max
#define max(a,b) ( (a) > (b) ? (a) : (b) )
#endif

typedef struct { 
  long   sampling_rate;  /* the following entries are for this sampling rate */
  int    num_cb_long;
  int    num_cb_short;
  int    cb_width_long[NSFB_LONG];
  int    cb_width_short[NSFB_SHORT];
} SR_INFO;

#define OFFSET_FOR_SHORT 448
#define NPART_LONG 100
#define NPART_SHORT 100

typedef struct {
  double hw[BLOCK_LEN_LONG*2];     /* Hann window table */
} FFT_TABLE_LONG;

typedef struct {
  double hw[BLOCK_LEN_SHORT*2];     /* Hann window table */
} FFT_TABLE_SHORT;
 
typedef struct {
  double bval[NPART_LONG];
  double qsthr[NPART_LONG];
  double rnorm[NPART_LONG];
  double bmax[NPART_LONG];
  double spreading[NPART_LONG][NPART_LONG];
} DYN_PART_TABLE_LONG;

typedef struct {
  int    sampling_rate;
  int    len;      /* length of the table */
  int    w_low[NPART_LONG];
  int    w_high[NPART_LONG];
  int    width[NPART_LONG];
  DYN_PART_TABLE_LONG *dyn;
} PARTITION_TABLE_LONG;

typedef struct {
  double bval[NPART_SHORT];
  double qsthr[NPART_SHORT];
  double rnorm[NPART_SHORT];
  double bmax[NPART_SHORT];
  double spreading[NPART_SHORT][NPART_SHORT];
} DYN_PART_TABLE_SHORT;

typedef struct {
  int    sampling_rate;
  int    len;      /* length of the table */ 
  int    w_low[NPART_SHORT];
  int    w_high[NPART_SHORT];
  int    width[NPART_SHORT];
  DYN_PART_TABLE_SHORT *dyn;
} PARTITION_TABLE_SHORT;

typedef struct {
  double fft_r[BLOCK_LEN_LONG*3];
  double fft_f[BLOCK_LEN_LONG*3];
  int    p_fft; /* pointer for fft_r and fft_f */
  double nb[NPART_LONG*2];
  double en[NPART_LONG];
  int    p_nb; /* pointer for nb */
  double ismr[NSFB_LONG]; /* 1/SMR in each swb */
  int use_ms[NSFB_LONG];
} PSY_STATVARIABLE_LONG;

typedef struct {
  double r_pred[BLOCK_LEN_LONG];
  double f_pred[BLOCK_LEN_LONG];
  double c[BLOCK_LEN_LONG];
  double e[NPART_LONG];
  double cw[NPART_LONG];
  double en[NPART_LONG];
  double cb[NPART_LONG];
  double cbb[NPART_LONG];
  double tb[NPART_LONG];
  double snr[NPART_LONG];
  double bc[NPART_LONG];
  double epart[NSFB_LONG];
  double thr[BLOCK_LEN_LONG];
  double npart[NSFB_LONG];
} PSY_VARIABLE_LONG;

typedef struct {
  double fft_r[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double fft_f[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double last6_fft_r[BLOCK_LEN_SHORT];
  double last6_fft_f[BLOCK_LEN_SHORT];
  double last7_fft_r[BLOCK_LEN_SHORT];
  double last7_fft_f[BLOCK_LEN_SHORT];
  double nb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double en[MAX_SHORT_WINDOWS][NPART_SHORT];
  double last7_nb[NPART_SHORT];
  double ismr[MAX_SHORT_WINDOWS][NSFB_SHORT]; /* 1/SMR in each swb */
  int use_ms[MAX_SHORT_WINDOWS][NSFB_SHORT];
} PSY_STATVARIABLE_SHORT;

typedef struct {
  double r_pred[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double f_pred[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double c[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double e[MAX_SHORT_WINDOWS][NPART_SHORT];
  double cw[MAX_SHORT_WINDOWS][NPART_SHORT];
  double en[MAX_SHORT_WINDOWS][NPART_SHORT];
  double cb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double cbb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double tb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double snr[MAX_SHORT_WINDOWS][NPART_SHORT];
  double bc[MAX_SHORT_WINDOWS][NPART_SHORT];
  double epart[MAX_SHORT_WINDOWS][NSFB_SHORT];
  double thr[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double npart[MAX_SHORT_WINDOWS][NSFB_SHORT];
} PSY_VARIABLE_SHORT;

typedef struct {
  double *p_ratio;
  int    *cb_width;
  int    use_ms[NSFB_LONG];
  int    no_of_cb;
} CH_PSYCH_OUTPUT_LONG;

typedef struct {
  double *p_ratio;
  int    *cb_width;
  int    use_ms[NSFB_SHORT];
  int    no_of_cb;
} CH_PSYCH_OUTPUT_SHORT;

#ifdef __cplusplus
extern "C" {
#endif

void Psy_Init( void );
void Psy_FillBuffer(double *p_time_signal[], int no_of_chan);
void Psy_Calculate( 
  /* input */
  double sampling_rate,
  int    no_of_chan,         /* no of audio channels */
  Ch_Info* chInfo,
  double *p_time_signal[],
  enum WINDOW_TYPE block_type[],
  int use_MS,
  /* output */
  CH_PSYCH_OUTPUT_LONG p_chpo_long[],
  CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS]
);

#define psy_sqr(x) ((x)*(x))


double psy_get_absthr(double f); /* Jul 8 */

void psy_fft_table_init(FFT_TABLE_LONG *fft_tbl_long, 
			FFT_TABLE_SHORT *fft_tbl_short
			);

void psy_part_table_init(double sampling_rate,
			 PARTITION_TABLE_LONG *part_tbl_long, 
			 PARTITION_TABLE_SHORT *part_tbl_short
			 );

void psy_calc_init(double sample[][BLOCK_LEN_LONG*2],
		   PSY_STATVARIABLE_LONG *psy_stvar_long, 
		   PSY_STATVARIABLE_SHORT *psy_stvar_short
		   ); 

void psy_step1(double* p_time_signal[], 
	       double sample[][BLOCK_LEN_LONG*2], 
	       int ch
	       );

void psy_step2(double sample[][BLOCK_LEN_LONG*2], 
               PSY_STATVARIABLE_LONG *psy_stvar_long, 
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
	       FFT_TABLE_LONG *fft_tbl_long,
	       FFT_TABLE_SHORT *fft_tbl_short,
	       int ch
	       );

void psy_step3(PSY_STATVARIABLE_LONG *psy_stvar_long, 
               PSY_STATVARIABLE_SHORT *psy_stvar_short, 
               PSY_VARIABLE_LONG *psy_var_long, 
               PSY_VARIABLE_SHORT *psy_var_short
	       );

void psy_step4(PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
	       PSY_VARIABLE_LONG *psy_var_long,
	       PSY_VARIABLE_SHORT *psy_var_short
	       );

void psy_step5(PARTITION_TABLE_LONG *part_tbl_long,
			   PARTITION_TABLE_SHORT *part_tbl_short,
			   PSY_STATVARIABLE_LONG *psy_stvar_long,
                           PSY_STATVARIABLE_SHORT *psy_stvar_short,
			   PSY_VARIABLE_LONG *psy_var_long,
			   PSY_VARIABLE_SHORT *psy_var_short
			   );

void psy_step6(PARTITION_TABLE_LONG *part_tbl_long,
			   PARTITION_TABLE_SHORT *part_tbl_short,
			   PSY_STATVARIABLE_LONG *psy_stvar_long,
                           PSY_STATVARIABLE_SHORT *psy_stvar_short,
			   PSY_VARIABLE_LONG *psy_var_long,
			   PSY_VARIABLE_SHORT *psy_var_short
			   );

void psy_step7(PARTITION_TABLE_LONG *part_tbl_long,
	       PARTITION_TABLE_SHORT *part_tbl_short,
	       PSY_VARIABLE_LONG *psy_var_long,
	       PSY_VARIABLE_SHORT *psy_var_short
	       );

void psy_step8(PARTITION_TABLE_LONG *part_tbl_long,
	       PARTITION_TABLE_SHORT *part_tbl_short,
	       PSY_VARIABLE_LONG *psy_var_long,
	       PSY_VARIABLE_SHORT *psy_var_short
	       );

void psy_step9(PARTITION_TABLE_LONG *part_tbl_long,
	       PARTITION_TABLE_SHORT *part_tbl_short,
	       PSY_VARIABLE_LONG *psy_var_long,
	       PSY_VARIABLE_SHORT *psy_var_short
	       );

void psy_step10(PARTITION_TABLE_LONG *part_tbl_long,
		PARTITION_TABLE_SHORT *part_tbl_short,
		PSY_STATVARIABLE_LONG *psy_stvar_long,
		PSY_STATVARIABLE_SHORT *psy_stvar_short,
		PSY_VARIABLE_LONG *psy_var_long,
		PSY_VARIABLE_SHORT *psy_var_short
		);

void psy_step11(PARTITION_TABLE_LONG *part_tbl_long,
		PARTITION_TABLE_SHORT *part_tbl_short,
		PSY_STATVARIABLE_LONG *psy_stvar_long,
		PSY_STATVARIABLE_SHORT *psy_stvar_short
		);

void psy_step12(PARTITION_TABLE_LONG *part_tbl_long,
				PARTITION_TABLE_SHORT *part_tbl_short,
				PSY_STATVARIABLE_LONG *psy_stvar_long,
				PSY_STATVARIABLE_SHORT *psy_stvar_short,
				PSY_VARIABLE_LONG *psy_var_long,
				PSY_VARIABLE_SHORT *psy_var_short,
				enum WINDOW_TYPE *block_type
				);

void psy_step14(SR_INFO *p_sri,
				PARTITION_TABLE_LONG *part_tbl_long,
				PARTITION_TABLE_SHORT *part_tbl_short,
				PSY_STATVARIABLE_LONG *psy_stvar_long,
				PSY_STATVARIABLE_SHORT *psy_stvar_short,
				PSY_VARIABLE_LONG *psy_var_long,
				PSY_VARIABLE_SHORT *psy_var_short
                );

void psy_step15(SR_INFO *p_sri,
				PSY_STATVARIABLE_LONG *psy_stvar_long,
				PSY_STATVARIABLE_SHORT *psy_stvar_short,
				PSY_VARIABLE_LONG *psy_var_long, PSY_VARIABLE_SHORT *psy_var_short,
				int leftChan, int rightChan, int midChan, int sideChan
				);

void psy_step2MS(PSY_STATVARIABLE_LONG *psy_stvar_long,
				 PSY_STATVARIABLE_SHORT *psy_stvar_short,
				 int leftChan, int rightChan,
				 int midChan, int sideChan);

void psy_step4MS(PSY_VARIABLE_LONG *psy_var_long,
				 PSY_VARIABLE_SHORT *psy_var_short,
				 int leftChan, int rightChan,
				 int midChan, int sideChan);

void psy_step7MS(PSY_VARIABLE_LONG *psy_var_long,
				 PSY_VARIABLE_SHORT *psy_var_short,
				 int leftChan, int rightChan,
				 int midChan, int sideChan);

void psy_step11MS(PARTITION_TABLE_LONG *part_tbl_long,
				  PARTITION_TABLE_SHORT *part_tbl_short,
				  PSY_STATVARIABLE_LONG *psy_stvar_long,
				  PSY_STATVARIABLE_SHORT *psy_stvar_short,
				  int leftChan, int rightChan,
				  int midChan, int sideChan);

#ifdef __cplusplus
}
#endif
