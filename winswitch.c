
/*
 * Window switching module
 * Most of this has been taken from NTT source code in the MPEG-4
 * Verification Model.
 *
 * TODO:
 *  - better multi-channel support
 */

#include <math.h>
#include "tf_main.h"
#include "winswitch.h"

#define ntt_N_SUP_MAX       2

int num_channel;


int winSwitch(/* Input */
			double sig[],         /* input signal */
			/* Output */
			enum WINDOW_TYPE *w_type,     /* code index for block type */
			/* Control */
			int    InitFlag)/* initialization flag */
{
    /*--- Variables ---*/
    int s_attack;
	int ratio;

    /*--- A.Jin 1997.10.19 ---*/
	ratio = checkAttack(sig, &s_attack, InitFlag);

    getWindowType( s_attack,  w_type, InitFlag );
	return ratio;
}


void getWindowType(int flag,      /* Input : trigger for short block length */
		 enum WINDOW_TYPE *w_type,   /* Output : code index for block type */
		 int InitFlag ) /* Control : initialization flag */
{
    static int	w_type_pre;
    static int	flag_pre;

    if ( InitFlag ){
		flag_pre = 0;
		w_type_pre = ONLY_LONG_WINDOW;
    }

    if (InitFlag){
		if (flag)        *w_type = ONLY_SHORT_WINDOW;
		else             *w_type = ONLY_LONG_WINDOW;
    }
    else{
		switch( w_type_pre ){
		case ONLY_LONG_WINDOW:
			if ( flag )      *w_type = LONG_SHORT_WINDOW;
			else             *w_type = ONLY_LONG_WINDOW;
			break;
		case LONG_SHORT_WINDOW:
			*w_type = ONLY_SHORT_WINDOW;
			break;
		case SHORT_LONG_WINDOW:
			if ( flag )      *w_type = LONG_SHORT_WINDOW;
			else             *w_type = ONLY_LONG_WINDOW;
			break;
		case ONLY_SHORT_WINDOW:
			if (flag || flag_pre) *w_type = ONLY_SHORT_WINDOW;
			else                  *w_type = SHORT_LONG_WINDOW;
		}
    }
    w_type_pre = *w_type;
    flag_pre = flag;
}


#define ST_CHKATK    (BLOCK_LEN_LONG/2+BLOCK_LEN_SHORT/2)

#define N_BLK_MAX	4096
#define	N_SFT_MAX   256
#define	N_LPC_MAX	2

__inline void ZeroMem(int n, double xx[])
{
	int i = n;
	while(i-- != 0)
		*(xx++) = 0.0;
}

void LagWindow(/* Output */
                double wdw[],  /* lag window data */
                /* Input */
                int n,         /* dimension of wdw[.] */
                double h)      /* ratio of window half value band width to sampling frequency */
{
	int i;
	double pi=3.14159265358979323846264338327959288419716939;
	double a,b,w;
	if(h<=0.0) for(i=0;i<=n;i++) wdw[i]=1.0;
	else
	{
		a=log(0.5)*0.5/log(cos(0.5*pi*h));
		a=(double)((int)a);
		w=1.0; b=a; wdw[0]=1.0;
		for(i=1;i<=n;i++)
		{
			b+=1.0; w*=a/b; wdw[i]=w; a-=1.0;
		}
	}
}

void HammingWindow(/* Output */
                double wdw[],  /* Hamming window data */
                int n)         /* window length */
{
	int i;
	double d,pi=3.14159265358979323846264338327950288419716939;
	if(n>0)
	{
		d=2.0*pi/n;
		for(i=0;i<n;i++)
			wdw[i]=0.54-0.46*cos(d*i);
	}
}

__inline void ntt_cutfr(int    st,      /* Input  --- Start point */
	   int    len,     /* Input  --- Block length */
	   int    ich,     /* Input  --- Channel number */
	   double frm[],   /* Input  --- Input frame */
	   double buf[])   /* Output --- Output data buffer */
{
    /*--- Variables ---*/
    int stb, sts, edb, nblk, iblk, ibuf, ifrmb, ifrms;

    stb = (st/BLOCK_LEN_LONG)*num_channel + ich;      /* start block */
    sts = st % BLOCK_LEN_LONG;            /* start sample */
    edb = ((st+len)/BLOCK_LEN_LONG)*num_channel + ich;        /* end block */
    nblk = (edb-stb)/num_channel;             /* number of overflow */

    ibuf=0; ifrmb=stb; ifrms=sts;
    for ( iblk=0; iblk<nblk; iblk++ ){
		while( ifrms < BLOCK_LEN_LONG )  buf[ibuf++] = frm[(ifrms++)+ifrmb*BLOCK_LEN_LONG];
		ifrms = 0;
		ifrmb += num_channel;
    }
    while( ibuf < len )
		buf[ibuf++] = frm[(ifrms++)+ifrmb*BLOCK_LEN_LONG];
}

__inline void CopyMem(int n, double xx[], double yy[])
{
	int i = n;
	if (i <= 0)
		return;
	do {
		*(yy++) = *(xx++);
	} while(--i);
}

__inline double DotProd(/* Input */
                 int n,       /* dimension of data */
                 double xx[],
                 double yy[])
{
	int i;
	double s;
	s=0.0;
	for(i=0;i<n;i++)
		s+=xx[i]*yy[i];
	return(s);
}

__inline void ntt_mulddd(/* Input */
		int n,        /* dimension of data */
		double xx[],
		double yy[],
		/* Output */
		double zz[])
{
    int i;
    for (i=0; i<n; i++ )
		zz[i] = xx[i]*yy[i];
}

__inline void AutoCorr(double *sig, /* Input : signal sample sequence */
		int n,       /* Input : length of sample sequence*/
		double *_pow,/* Output : power */
		double cor[],/* Output : autocorrelation coefficients */
		int p)
{
	int k; 
	register double sqsum,c, dsqsum;

	if (n>0) {
		sqsum = DotProd(n, sig, sig)+1.e-35;
		dsqsum = 1./sqsum;
		k=p;
		do {
			c = DotProd(n-k, sig, sig+k);
			cor[k] = c*dsqsum;
		}while(--k);
	}
	*_pow = (sqsum-1.e-35)/(double)n;
}

__inline void Corr2Ref(int p,          /* Input : LPC analysis order */
	    double cor[],   /* Input : correlation coefficients */
	    double alf[],   /* Output : linear predictive coefficients */
	    double ref[],   /* Output : reflection coefficients */
	    double *resid_) /* Output : normalized residual power */
{
	int i,j,k;
	double resid,r,a;
	if(p>0) {
		ref[1]=cor[1];
		alf[1]= -ref[1];
		resid=(1.0-ref[1])*(1.0+ref[1]);
		for(i=2;i<=p;i++) {
			r=cor[i];
			for(j=1;j<i;j++)
				r+=alf[j]*cor[i-j];
			alf[i]= -(ref[i]=(r/=resid));
			j=0; k=i;
			while(++j<=--k) {
				a=alf[j];
				alf[j]-=r*alf[k];
				if(j<k) alf[k]-=r*a;
			}
			resid*=(1.0-r)*(1.0+r);
		}
		*resid_=resid;
	}
	else
		*resid_=1.0;
}

int checkAttack(double in[],  /* Input signal */
                  int    *flag, /* flag for attack */
                  int    InitFlag) /* Initialization Flag */
{
	/*--- Variables ---*/
	int                 n_div, iblk, ismp, ich;
	double              bufin[N_SFT_MAX];
	static double sig[ntt_N_SUP_MAX][N_BLK_MAX+N_LPC_MAX];
	double        wsig[N_BLK_MAX];
	static double wdw[N_BLK_MAX];
	static double wlag[N_LPC_MAX+1];
	double        cor[N_LPC_MAX+1],ref[N_LPC_MAX+1],alf[N_LPC_MAX+1];
	static double prev_alf[ntt_N_SUP_MAX][N_LPC_MAX+1];
	double        resid, wpowfr;
	double        long_power;
	double        synth, resid2;
	static int    N_BLK, N_SFT, S_POW, L_POW, N_LPC;
	double ratio, sum, product;

	/*--- Initialization ---*/
	if ( InitFlag ){
		/* Set parameters */
		N_BLK      = BLOCK_LEN_LONG;
		N_SFT      = BLOCK_LEN_SHORT;
		S_POW      = BLOCK_LEN_SHORT;
		L_POW      = BLOCK_LEN_LONG;
		N_LPC      = 1;
		/* clear buffers */
		for ( ich=0; ich<num_channel; ich++ ){
			ZeroMem( N_LPC+N_BLK, sig[ich] );
			ZeroMem( N_LPC+1, prev_alf[ich] );
		}
		/* set windows */
		LagWindow( wlag, N_LPC, 0.02 );
		HammingWindow( wdw, N_BLK );
	}


	n_div = BLOCK_LEN_LONG/N_SFT;
	*flag = 0;
	sum=0.0;
	product=1.0;
	for ( ich=0; ich<num_channel; ich++ ){
		for ( iblk=0; iblk<n_div; iblk++ ){
			ntt_cutfr( ST_CHKATK+iblk*N_SFT, N_SFT, ich, in, bufin );
			CopyMem(N_SFT,bufin,&sig[ich][N_LPC+N_BLK-N_SFT]);
			/*--- Calculate long power ---*/
			long_power =
				(DotProd( L_POW, &sig[ich][N_BLK-L_POW], &sig[ich][N_BLK-L_POW] )+0.1)
				/ L_POW;
			long_power = sqrt(long_power);

			/*--- Calculate alpha parameter ---*/
			ntt_mulddd( N_BLK, &sig[ich][N_LPC], wdw, wsig );
			AutoCorr( wsig, N_BLK, &wpowfr, cor, N_LPC ); cor[0] = 1.0;
			ntt_mulddd( N_LPC+1, cor, wlag, cor );
			Corr2Ref( N_LPC, cor, alf, ref, &resid );
			/*--- Get residual signal and its power ---*/
			resid2 = 0.;
			for ( ismp=N_BLK-S_POW; ismp<N_BLK; ismp++ ){
				synth = sig[ich][ismp]+
					prev_alf[ich][1]*sig[ich][ismp-1];
				resid2 += synth*synth;
			}
			resid2 /= long_power;
			resid2 = sqrt(resid2);
			sum += resid2+0.0001;
			product *= (resid2+0.0001);

			CopyMem( N_LPC, &alf[1], &prev_alf[ich][1] );

			CopyMem( N_BLK-N_SFT, &sig[ich][N_LPC+N_SFT], &sig[ich][N_LPC] );
		}
	}
/*	ratio = sum/(n_div*num_channel)/pow(product, 1./(double)(n_div*num_channel)); */
	/* This below is a bit different than the commented line above here,
	   but I like it better this way, because it gives higher values,
	   that are better to tune. */
	ratio = pow(product, 1./(double)(n_div*num_channel));
	ratio = (n_div*num_channel)/ratio;
	ratio = sum/ratio;

	if(ratio > 8000) {
		*flag = 1;
	} else {
		*flag = 0;
	}
	return (int)(ratio+0.5);
}


void winSwitchInit(int max_ch)
{
	num_channel = max_ch;
}

