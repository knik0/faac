/************************* MPEG-2 NBC Audio Decoder **************************
 *                                                                           *
 "This software module was originally developed by 
 Fraunhofer Gesellschaft IIS / University of Erlangen (UER) in the course of 
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

-----
This software module was modified by

Tadashi Araki (Ricoh Company, ltd.)
Tatsuya Okada (Waseda Univ.)

and edited by

in the course of development of the MPEG-2 NBC/MPEG-4 Audio standard
ISO/IEC 13818-7, 14496-1,2 and 3.

Copyright (c) 1997.

 *                                                                           *
 ****************************************************************************/

/* CREATED BY :  Bernhard Grill -- August-96  */

#include "all.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


typedef struct { 
  long   sampling_rate;                   /* the following entries are for this sampling rate */
  int    num_cb_long;
  int    num_cb_short;
  int    cb_width_long[NSFB_LONG];
  int    cb_width_short[NSFB_SHORT+2];
  double fixed_ratio_long[NSFB_LONG];
  double fixed_ratio_short[NSFB_SHORT+2];
} SR_INFO;

/* added by T. Araki (1997.10.17) */
#define OFFSET_FOR_SHORT 448
#define NPART_LONG 72
#define NPART_SHORT 48
/* added by T. Araki (1997.10.17) end */

/* added by T. Araki (1997.07.10) */
typedef struct {
  double bval[NPART_LONG];
  double qsthr[NPART_LONG];
  double e_qsthr[NPART_LONG]; /* absolute threshold (energy) in each partition */  
  double rnorm[NPART_LONG];
  double bmax[NPART_LONG];
  double spreading[NPART_LONG][NPART_LONG];
  double hw[BLOCK_LEN_LONG*2];
} DYN_PARTITION_TABLE_LONG;
 
typedef struct {
  int    sampling_rate;
  int    len;      /* length of the table */
  int    w_low[NPART_LONG];
  int    w_high[NPART_LONG];
  int    width[NPART_LONG];
  DYN_PARTITION_TABLE_LONG *dyn;
} PARTITION_TABLE_LONG;

typedef struct {
  double bval[NPART_SHORT];
  double qsthr[NPART_SHORT];
  double e_qsthr[NPART_SHORT]; /* absolute threshold (energy) in each partition */  
  double rnorm[NPART_SHORT];
  double bmax[NPART_SHORT];
  double spreading[NPART_SHORT][NPART_SHORT];
  double hw[BLOCK_LEN_SHORT*2];
} DYN_PARTITION_TABLE_SHORT;

typedef struct {
  int    sampling_rate;
  int    len;      /* length of the table */ 
  int    w_low[NPART_SHORT];
  int    w_high[NPART_SHORT];
  int    width[NPART_SHORT];
  DYN_PARTITION_TABLE_SHORT *dyn;
} PARTITION_TABLE_SHORT;

typedef struct {
  float xreal[BLOCK_LEN_LONG*2];
  float fft_energy[BLOCK_LEN_LONG*2];
  double nb[NPART_LONG*2];
  double en[NPART_LONG];
  double save_npart_l[NSFB_LONG];
  int    p_nb; /* pointer for nb */
  double ismr[NSFB_LONG]; /* 1/SMR in each swb */
} PSY_STATVARIABLE_LONG;

typedef struct {
  double c[BLOCK_LEN_LONG];
  double e[NPART_LONG];
  double cw[NPART_LONG];
  double en[NPART_LONG];
  double cb[NPART_LONG];
  double tb[NPART_LONG];
  double snr[NPART_LONG];
  double bc[NPART_LONG];
  double pe;
  double epart[NSFB_LONG];
  double thr[BLOCK_LEN_LONG];
  double npart[NSFB_LONG];
} PSY_VARIABLE_LONG;

typedef struct {
  float xreal[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT*2];
  float fft_energy[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT*2];
  double nb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double en[MAX_SHORT_WINDOWS][NPART_SHORT];
  double save_npart_s[MAX_SHORT_WINDOWS][NSFB_SHORT];
  double last7_nb[NPART_SHORT];
  double ismr[MAX_SHORT_WINDOWS][NSFB_SHORT]; /* 1/SMR in each swb */
} PSY_STATVARIABLE_SHORT;

typedef struct {
  double c[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double e[MAX_SHORT_WINDOWS][NPART_SHORT];
  double cw[MAX_SHORT_WINDOWS][NPART_SHORT];
  double en[MAX_SHORT_WINDOWS][NPART_SHORT];
  double cb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double tb[MAX_SHORT_WINDOWS][NPART_SHORT];
  double snr[MAX_SHORT_WINDOWS][NPART_SHORT];
  double bc[MAX_SHORT_WINDOWS][NPART_SHORT];
  double epart[MAX_SHORT_WINDOWS][NSFB_SHORT];
  double thr[MAX_SHORT_WINDOWS][BLOCK_LEN_SHORT];
  double npart[MAX_SHORT_WINDOWS][NSFB_SHORT];
} PSY_VARIABLE_SHORT;
/* added by T. Araki (1997.07.10) end */

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

void EncTf_psycho_acoustic_init( void );
void psy_fill_lookahead(double *p_time_signal[], int no_of_chan);
void EncTf_psycho_acoustic( 
  /* input */
  double sampling_rate,
  int    no_of_chan,         /* no of audio channels */
  double *p_time_signal[],
  enum WINDOW_TYPE block_type[],
  int qcSelect,
  int frameLength,
  /* output */
  CH_PSYCH_OUTPUT_LONG p_chpo_long[],
  CH_PSYCH_OUTPUT_SHORT p_chpo_short[][MAX_SHORT_WINDOWS]
);

/* added by T. Okada( 1997.07.10 ) */
/* Jul 10 */
#define psy_max(x,y) ((x) > (y) ? (x) : (y))
#define psy_min(x,y) ((x) < (y) ? (x) : (y))
#define psy_sqr(x) ((x)*(x))


double psy_get_absthr(double f); /* Jul 8 */

void psy_part_table_init(double sampling_rate,
			 PARTITION_TABLE_LONG **part_tbl_long, 
			 PARTITION_TABLE_SHORT **part_tbl_short
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
               PARTITION_TABLE_LONG *part_tbl_long, 
			   PARTITION_TABLE_SHORT *part_tbl_short, 
			   PSY_STATVARIABLE_LONG *psy_stvar_long, 
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
			   int ch
			   );

void psy_step3(PSY_STATVARIABLE_LONG *psy_stvar_long, 
               PSY_STATVARIABLE_SHORT *psy_stvar_short, 
               PSY_VARIABLE_LONG *psy_var_long, 
               PSY_VARIABLE_SHORT *psy_var_short, 
               int ch
	       );

void psy_step4(PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
	       PSY_VARIABLE_LONG *psy_var_long, 
	       PSY_VARIABLE_SHORT *psy_var_short,
	       int ch
	       );

void psy_step5(PARTITION_TABLE_LONG *part_tbl_long, 
			   PARTITION_TABLE_SHORT *part_tbl_short, 
			   PSY_STATVARIABLE_LONG *psy_stvar_long,
               PSY_STATVARIABLE_SHORT *psy_stvar_short,
			   PSY_VARIABLE_LONG *psy_var_long, 
			   PSY_VARIABLE_SHORT *psy_var_short,
			   int ch
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
		PSY_VARIABLE_SHORT *psy_var_short,
		int ch /* ch */
		);

void psy_step11(PARTITION_TABLE_LONG *part_tbl_long, 
		PARTITION_TABLE_SHORT *part_tbl_short, 
		PSY_STATVARIABLE_LONG *psy_stvar_long, 
		PSY_STATVARIABLE_SHORT *psy_stvar_short, 
		int ch
		);

void psy_step11andahalf(PARTITION_TABLE_LONG *part_tbl_long, 
						PARTITION_TABLE_SHORT *part_tbl_short, 
						PSY_STATVARIABLE_LONG *psy_stvar_long, 
						PSY_STATVARIABLE_SHORT *psy_stvar_short, 
						int ch);

void psy_step12(PARTITION_TABLE_LONG *part_tbl_long, 
		PARTITION_TABLE_SHORT *part_tbl_short, 
		PSY_STATVARIABLE_LONG *psy_stvar_long, 
		PSY_STATVARIABLE_SHORT *psy_stvar_short, 
		PSY_VARIABLE_LONG *psy_var_long, 
		PSY_VARIABLE_SHORT *psy_var_short,
		int ch
		);

void psy_step13(PSY_VARIABLE_LONG *psy_var_long, 
		enum WINDOW_TYPE *block_type,
		int ch
		);

void psy_step14(SR_INFO *p_sri,
		PARTITION_TABLE_LONG *part_tbl_long, 
		PARTITION_TABLE_SHORT *part_tbl_short, 
		PSY_STATVARIABLE_LONG *psy_stvar_long, 
		PSY_STATVARIABLE_SHORT *psy_stvar_short, 
		PSY_VARIABLE_LONG *psy_var_long, 
		PSY_VARIABLE_SHORT *psy_var_short,
		int ch);

void psy_step15(int use_ms_l[NSFB_LONG],
				int use_ms_s[MAX_SHORT_WINDOWS][NSFB_SHORT],
				SR_INFO *p_sri,
				PSY_STATVARIABLE_LONG *psy_stvar_long, 
				PSY_STATVARIABLE_SHORT *psy_stvar_short, 
				PSY_VARIABLE_LONG *psy_var_long, 
				PSY_VARIABLE_SHORT *psy_var_short,
				int ch
				);

#ifdef __cplusplus
}
#endif
