
#ifndef _WINSWITCH_H_
#define _WINSWITCH_H_

void getWindowType(int    flag,        /* Input : trigger for short block length */
				   enum WINDOW_TYPE    *w_type,     /* Output : code index for block type*/
				   int    InitFlag );  /* Control : initialization flag */

int checkAttack(double in[],         /* Input : input signal */
				int    *flag,/* Output : trigger for short block length */
				int    InitFlag); /* Control : initialization flag */


int winSwitch(/* Input */
			  double sig[],          /* input signal */
			  /* Output */
			  enum WINDOW_TYPE    *w_type,     /* Output : code index for block type*/
			  /* Control */
			  int    InitFlag);      /* initialization flag */
   
void winSwitchInit(int max_ch);

#endif

