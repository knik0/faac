
#ifndef PULSE_H_
#define PULSE_H_

#include "interface.h"
//#define LEN_NEC_NPULSE	2
//#define LEN_NEC_ST_SFB	6
//#define LEN_NEC_POFF	5
//#define LEN_NEC_PAMP	4
//#define NUM_NEC_LINES	4
//#define NEC_OFFSET_AMP	4

typedef struct
{
    int pulse_data_present;
    int number_pulse;
    int pulse_start_sfb;
    int pulse_position[NUM_NEC_LINES];
    int pulse_offset[NUM_NEC_LINES];
    int pulse_amp[NUM_NEC_LINES];
} AACPulseInfo;

typedef struct {
	int offset;
	int sfb;
	int amp;
} pulse;


#endif