/**********************************************************************
audio sample rate converter

$Id: rateconv.c,v 1.1 2000/01/17 21:53:04 menno Exp $

Source file: rateconv.c

Authors:
NM    Nikolaus Meine, Uni Hannover (c/o Heiko Purnhagen)
HP    Heiko Purnhagen, Uni Hannover <purnhage@tnt.uni-hannover.de>

Changes:
xx-jun-98   NM      resamp.c
18-sep-98   HP      converted into module
03-aug-99   NM      now even faster ...
04-nov-99   NM/HP   double ratio, htaps1 /= 2
11-nov-99   HP      improved RateConvInit() debuglevel
**********************************************************************/

/*
 * Sample-rate converter
 *
 * Realized in three steps:
 *
 * 1. Upsampling by factor two while doing appropriate lowpass-filtering.
 *    This is done by using an FFT-based convolution algorithm with a multi-tap
 *    Kaiser-windowed lowpass filter.
 *    If the cotoff-frequency is less than 0.5, only lowpass-filtering without
 *    the upsampling is done.
 * 2. Upsampling by factor 128 using a 15 tap Kaiser-windowed lowpass filter
 *    (alpha=12.5) and conventional convolution algorithm.
 *    Two values (the next neighbours) are computed for every sample needed.
 * 3. Linear interpolation between the two bounding values.
 *
 * Stereo and mono data is supported.
 * Up- and downsampling of any ratio is possible.
 * Input and output file format is Sun-audio.
 *
 * Written by N.Meine, 1998
 *
 */

/* Multi channel data is interleaved: l0 r0 l1 r1 ... */
/* Total number of samples (over all channels) is used. */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#include "aacenc.h"
#include "rateconv.h"

/* ---------- declarations ---------- */

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef min
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

/* ---------- declarations (structures) ---------- */

#define real	double	/* float seems to be precise enough, but it	*/
			/* doesn't run much faster than double		*/

typedef struct {
	real	re,im;
} complex;


struct RCBufStruct		/* buffer handle */
{
  int numChannel;		/* number of channels */
  long currentSampleIn;		/* number of input samples */
  long currentSampleOut;	/* number of output samples */

  real *wtab;
  complex *x;
  real *tp1;
  real *tp2;
  complex *ibuf;

  long nfn;
  long adv;
  long istart;
  long numode;
  long fm;
  double p1;
  double d1;

  long outSize;
  float *out;
};


/* ---------- variables ---------- */

static int RCdebugLevel = 0;	/* debug level */


#define deftaps1	485

#define ov2exp	7		/* Oversampling exponent in the second step */
#define ov2fac	(1<<ov2exp)	/* Oversampling factor in the second step */
#define htaps2	7		/* Num. taps of the 2nd filter: 2*htaps2+1 */
#define alpha2	12.5		/* Kaiser-parameter for the second filter */
#define cutoff2	0.97		/* cutoff-frequency for the second filter */

#define cexp	(CACHE1-(sizeof(real)==4 ? 3 : 4 ))
#define cache	(1<<cexp)

#define pi	3.1415926535897932
#define zpi	6.2831853071795965


/* ---------- local functions ---------- */


static void mkeiwutab(real *st,long fn)
{
	register int	i,k;
	register double	phi;

	for (k=fn; k; k>>=1)
	{
		phi = zpi/((double) k);
		for (i=0; i<k; i++) st[i]=(real) sin(phi*((double) i));
		st+=k;
	}
}

static void r4fftstep(register real *x0,register long n,register real *t)
{
	register long		n4 = n/4;
	register real		*x1 = x0+2*n4;
	register real		*x2 = x1+2*n4;
	register real		*x3 = x2+2*n4;
	register real		*y;
	register real		wr,wi,ar,ai,br,bi,cr,ci,dr,di,hr,hi;
	register long		i;


	if ( n>16 )
	{
		y=t+6*n4;
		r4fftstep(x0,n4,y);
		r4fftstep(x1,n4,y);
		r4fftstep(x2,n4,y);
		r4fftstep(x3,n4,y);
	}
	else
	{
		if ( n==8 )
		{
			for (y=x0; y<x0+16; y+=4)
			{
				ar=y[0]; ai=y[1];
				br=y[2]; bi=y[3];
				y[0]=ar+br;
				y[1]=ai+bi;
				y[2]=ar-br;
				y[3]=ai-bi;
			}
		}
		else
		{
			for (y=x0; y<x0+32; y+=8)
			{
				ar=y[0]; ai=y[1];
				br=y[2]; bi=y[3];
				cr=y[4]; ci=y[5];
				dr=y[6]; di=y[7];
				hr=ar+cr; wr=ar-cr; hi=ai+ci; wi=ai-ci;
				ar=br+dr; cr=br-dr; ai=bi+di; ci=bi-di;
				y[0]=hr+ar; y[1]=hi+ai;
				y[2]=wr+ci; y[3]=wi-cr;
				y[4]=hr-ar; y[5]=hi-ai;
				y[6]=wr-ci; y[7]=wi+cr;
			}
		}
	}

	y=t+n4;
	for (i=0; i<n4; i++)
	{
		ar=x0[0]; ai=x0[1];
		wr=y[  i]; wi=t[  i]; hr=x1[0]; hi=x1[1]; br=wr*hr+wi*hi; bi=wr*hi-wi*hr;
		wr=y[2*i]; wi=t[2*i]; hr=x2[0]; hi=x2[1]; cr=wr*hr+wi*hi; ci=wr*hi-wi*hr;
		wr=y[3*i]; wi=t[3*i]; hr=x3[0]; hi=x3[1]; dr=wr*hr+wi*hi; di=wr*hi-wi*hr;
		hr=ar+cr; wr=ar-cr; hi=ai+ci; wi=ai-ci;
		ar=br+dr; cr=br-dr; ai=bi+di; ci=bi-di;
		x0[0]=hr+ar; x0[1]=hi+ai;
		x1[0]=wr+ci; x1[1]=wi-cr;
		x2[0]=hr-ar; x2[1]=hi-ai;
		x3[0]=wr-ci; x3[1]=wi+cr;
		x0+=2; x1+=2; x2+=2; x3+=2;
	}
}


static void r4ifftstep(register real *x0,register long n,register real *t)
{
	register long		n4 = n/4;
	register real		*x1 = x0+2*n4;
	register real		*x2 = x1+2*n4;
	register real		*x3 = x2+2*n4;
	register real		*y;
	register real		wr,wi,ar,ai,br,bi,cr,ci,dr,di,hr,hi;
	register long		i;

	if ( n>16 )
	{
		y=t+6*n4;
		r4ifftstep(x0,n4,y);
		r4ifftstep(x1,n4,y);
		r4ifftstep(x2,n4,y);
		r4ifftstep(x3,n4,y);
	}
	else
	{
		if ( n==8 )
		{
			for (y=x0; y<x0+16; y+=4)
			{
				ar=y[0]; ai=y[1];
				br=y[2]; bi=y[3];
				y[0]=ar+br;
				y[1]=ai+bi;
				y[2]=ar-br;
				y[3]=ai-bi;
			}
		}
		else
		{
			for (y=x0; y<x0+32; y+=8)
			{
				ar=y[0]; ai=y[1];
				br=y[2]; bi=y[3];
				cr=y[4]; ci=y[5];
				dr=y[6]; di=y[7];
				hr=ar+cr; wr=ar-cr; hi=ai+ci; wi=ai-ci;
				ar=br+dr; cr=br-dr; ai=bi+di; ci=bi-di;
				y[0]=hr+ar; y[1]=hi+ai;
				y[2]=wr-ci; y[3]=wi+cr;
				y[4]=hr-ar; y[5]=hi-ai;
				y[6]=wr+ci; y[7]=wi-cr;
			}
		}
	}

	y=t+n4;
	for (i=0; i<n4; i++)
	{
		ar=x0[0]; ai=x0[1];
		wr=y[  i]; wi=t[  i]; hr=x1[0]; hi=x1[1]; br=wr*hr-wi*hi; bi=wr*hi+wi*hr;
		wr=y[2*i]; wi=t[2*i]; hr=x2[0]; hi=x2[1]; cr=wr*hr-wi*hi; ci=wr*hi+wi*hr;
		wr=y[3*i]; wi=t[3*i]; hr=x3[0]; hi=x3[1]; dr=wr*hr-wi*hi; di=wr*hi+wi*hr;
		hr=ar+cr; wr=ar-cr; hi=ai+ci; wi=ai-ci;
		ar=br+dr; cr=br-dr; ai=bi+di; ci=bi-di;
		x0[0]=hr+ar; x0[1]=hi+ai;
		x1[0]=wr-ci; x1[1]=wi+cr;
		x2[0]=hr-ar; x2[1]=hi-ai;
		x3[0]=wr+ci; x3[1]=wi-cr;
		x0+=2; x1+=2; x2+=2; x3+=2;
	}
}


static void perm4(real *x,long p)
{
	register real	a,b,c,d;
	register long	i,j,k,l;

	l=1<<(p-1);
	for (i=j=0; i<(2<<p); i+=2)
	{
		if ( j>i )
		{
			a=x[i]; b=x[i+1]; c=x[j]; d=x[j+1]; x[j]=a; x[j+1]=b; x[i]=c; x[i+1]=d;	
		}
		j+=l;
		for (k=l<<2; j&k; k>>=2) j=(j^=k)+(k>>4);
	}
}

static void perm42(real *x,long p)
{
	register real	*y,*z;
	register long	i,n;

	n=1<<p;
	perm4(x  ,p-1);
	perm4(x+n,p-1);

	y=x+2*n;
	z=x+n;
	for (i=0; i<n; i+=2)
	{
		y[2*i  ]=x[i  ];
		y[2*i+1]=x[i+1];
		y[2*i+2]=z[i  ];
		y[2*i+3]=z[i+1];
		x[i  ]=y[i  ];
		x[i+1]=y[i+1];
	}
	for (; i<2*n; i+=2)
	{
		x[i  ]=y[i  ];
		x[i+1]=y[i+1];
	}
}

static void r4fft(real *t,real *x,long p)
{
	register long	n  = 1<<p;

	if ( p&1 ) perm42(x,p); else perm4(x,p);
	r4fftstep(x,n,t);
}

static void r4ifft(real *t,real *x,long p)
{
	register long	n  = 1<<p;

	if ( p&1 ) perm42(x,p); else perm4(x,p);
	r4ifftstep(x,n,t);
}


static double bessel(double x)
{
  register double	p,s,ds,k;

  x*=0.5;
  k=0.0;
  p=s=1.0;
  do
    {
      k+=1.0;
      p*=x/k;
      ds=p*p;
      s+=ds;
    }
  while ( ds>1.0e-17*s ); 

  return s;
}

static void mktp1(complex *y,double fg,double a,long fn,long htaps1)
{
  register long	i;
  register double	f,g,x,px;

  y[0].re=fg;
  y[0].im=0.0;

  f=1.0/bessel(a);
  g=1.0/(htaps1+0.5);
  g*=g;

  for (i=1; i<=htaps1; i++)
    {
      x=(double) i;
      px=pi*x;
      y[i].re=(real) f*bessel(a*sqrt(1.0-g*x*x))*sin(fg*px)/px;
      y[i].im=(real) 0.0;
      y[fn-i]=y[i];
    }
  for (; i<fn-htaps1; i++) y[i].re=y[i].im=0.0;
}

static void mktp2(real *y,double fg,double a)
{
  register long	i;
  register double	f,g,x,px;

  y[0]=fg;

  f=1.0/bessel(a);
  g=1.0/(((double) ((htaps2+1)*ov2fac))-0.5);
  g*=g;

  for (i=1; i<(htaps2+1)*ov2fac; i++)
    {
      x=(double) i;
      px=(pi/ov2fac)*x;
      y[i]=(real) f*bessel(a*sqrt(1.0-g*x*x))*sin(fg*px)/px;
    }
  for (; i<(htaps2+2)*ov2fac; i++) y[i]=0.0;
}

static void ctp2_m(real *x,double p,real *k,float **out)
{
  register long	n,i,j;
  register double	h,l,r;
  register real	*ka,*kb;
  register real	*xa,*xb;

  h=ov2fac*p;
  i=(long) h;
  h-=(double) i;

  j=i&(ov2fac-1); i>>=ov2exp;

  l=r=0.0;
  ka=k+j;	kb=k+ov2fac-j;
  i<<=1;
  xa=x+i;	xb=x+i+2;

  n=htaps2+1;
  do
    {
      l+=xa[0]*ka[ 0] + xb[0]*kb[ 0];
      r+=xa[0]*ka[ 1] + xb[0]*kb[-1];

      ka+=ov2fac;
      kb+=ov2fac;
      xa-=2;
      xb+=2;
    }
  while (--n);

  *(*out)++ = (float) (l+h*(r-l));
}

static void ctp2_s(complex *x,double p,real *k,float **out)
{
  register long	n,i,j;
  register double	h,l0,l1,r0,r1;
  register real	*ka,*kb;
  register complex *xa,*xb;

  h=ov2fac*p;
  i=(long) h;
  h-=(double) i;

  j=i&(ov2fac-1); i>>=ov2exp;

  l0=l1=r0=r1=0.0;
  ka=k+j;	kb=k+ov2fac-j;
  xa=x+i;	xb=x+i+1;

  n=htaps2+1;
  do
    {
      l0+=xa[0].re*ka[ 0] + xb[0].re*kb[ 0];
      l1+=xa[0].im*ka[ 0] + xb[0].im*kb[ 0];
      r0+=xa[0].re*ka[ 1] + xb[0].re*kb[-1];
      r1+=xa[0].im*ka[ 1] + xb[0].im*kb[-1];

      ka+=ov2fac;
      kb+=ov2fac;
      xa--;
      xb++;
    }
  while (--n);

  *(*out)++ = (float) (l0+h*(r0-l0));
  *(*out)++ = (float) (l1+h*(r1-l1));
}

static void ctp1_1(complex *x,real *tp1,real *wtab,long fm)
{
  register long	i,fn2;

  fn2=1<<(fm-1);
  r4fft(wtab,(real *) x,fm);
  for (i=0; i<fn2; i++)
    {
      x[i+fn2].re*=tp1[fn2-i];
      x[i+fn2].im*=tp1[fn2-i];
      x[i    ].re*=tp1[i    ];
      x[i    ].im*=tp1[i    ];
    }
  r4ifft(wtab,(real *) x,fm);
}

static void ctp1_2(complex *x,real *tp1,real *wtab,long fm)
{
  register long	i,fn2;

  fn2=1<<(fm-1);

  r4fft(wtab+(1<<fm),(real *) x,fm-1);
  for (i=0; i<fn2; i++)
    {
      x[i+fn2].re=x[i].re*tp1[fn2-i];
      x[i+fn2].im=x[i].im*tp1[fn2-i];
      x[i].re*=tp1[i];
      x[i].im*=tp1[i];
    }
  r4ifft(wtab,(real *) x,fm);
}


/* ---------- functions ---------- */

/* RateConvInit() */
/* Init audio sample rate conversion. */

RCBuf *RateConvInit (
  int debugLevel,		/* in: debug level */
				/*     0=off  1=basic  2=full */
  double ratio,			/* in: outputRate / inputRate */
  int numChannel,		/* in: number of channels */
  int htaps1,			/* in: num taps */
				/*      -1 = auto */
  float a1,			/* in: alpha for Kaiser window */
				/*      -1 = auto */
  float fc,			/* in: 6dB cutoff freq / input bandwidth */
				/*      -1 = auto */
  float fd,			/* in: 100dB cutoff freq / input bandwidth */
				/*      -1 = auto */
  long *numSampleIn)		/* out: num input samples / frame */
				/* returns: */
				/*  buffer (handle) */
				/*  or NULL if error */
{
  real *wtab;
  complex *x;
  real *tp1;
  real *tp2;
  complex *ibuf;

  long nfn;
  long adv;
  long istart;
  long numode;
  long fm;
  double p1;
  double d1;

  double sf2;
  double trw2;
  long fn;
  long fn2;
  long i;
  double h;
  double a;

  RCBuf *buf;

  RCdebugLevel = debugLevel;
  if (RCdebugLevel)
    printf("RateConvInit: debugLevel=%d ratio=%f numChannel=%d\n"
	   "htaps1=%d a1=%f fc=%f fd=%f\n",
	   RCdebugLevel,ratio,numChannel,htaps1,a1,fc,fd);

  buf=(RCBuf*)malloc(sizeof(RCBuf));
//  if (!(buf=(RCBuf*)malloc(sizeof(RCBuf))))
//    CommonExit(-1,"RateConvInit: Can not allocate memory");
  buf->currentSampleIn = 0;
  buf->currentSampleOut = 0;
  buf->numChannel = numChannel;

  if (htaps1<0)
    htaps1 = deftaps1;
  htaps1 /= 2;	/* NM 991104 */
  if (a1<0)
    a1 = 10.0;
  numode = 0;
  for (fm=2; (1<<fm)<8*htaps1; fm+=2);
  fn=1<<fm;
  fn2=fn/2;

  wtab=(real *) malloc((4*fn + fn2+1 + (htaps2+2)*ov2fac)*sizeof(real));
//  if (!(wtab=
//	(real *) malloc((4*fn + fn2+1 + (htaps2+2)*ov2fac)*sizeof(real))))
//    CommonExit(-1,"RateConvInit: Can not allocate memory");
  x=((complex *) wtab)+fn;
  tp1=(real *) (x+fn);
  tp2=tp1+fn2+1;

  mkeiwutab(wtab,fn);
  mktp1(x,0.5,a1,fn,htaps1);
  r4fft(wtab,(real *) x,fm);
  h=x[0].re*x[0].re*1.0e-10;
  for (i=fn/2; x[i].re*x[i].re<h; i--);
  trw2=((double) (i-(fn2/2)))/((double) (fn2/2));

  sf2 = ratio;
  if ( sf2<0.0 ) sf2=1.0;

  d1=1.0/sf2;
  p1=htaps1+htaps2+2;		/* delay compensation */

  if ( fc<0.0 )
    {
      if ( fd<0.0 )
	{
	  fc=1.0/d1-trw2;
	  if ( fc<0.5-0.5*trw2 ) { numode=1; fc=2.0/d1-trw2; }
	  if ( fc>1.0-trw2 ) fc=1.0-trw2;
	}
      else
	{
	  fc=fd-trw2;
	  if ( fd<=0.5 ) { numode=1; fc=2.0*fd-trw2; }
	}
    }
  else
    {
      if ( fc<0.5-trw2 ) { numode=1; fc+=fc; }
    }

  if ( fc>2.0 ) fc=2.0;

  if ( fc<=0.0 ) {
//    CommonWarning("RateConvInit: cutoff frequency to low: %f %f %f\n",
//		  fc,fd,trw2);
    free(wtab);
    free(buf);
    return NULL;
  }

  nfn=fn;
  adv=fn-2*(htaps1+htaps2+2);
  istart=htaps1+htaps2+2;
  if ( !numode ) { nfn>>=1; adv>>=1; d1+=d1; }

  mktp1(x,0.5*fc,a1,fn,htaps1);
  r4fft(wtab,(real *) x,fm);
  a=1.0/((double) (((1+numode)*fn2)));
  for (i=0; i<=fn2 ; i++) tp1[i]=a*x[i].re;

  mktp2(tp2,cutoff2,alpha2);

  *numSampleIn = 2*adv;
  buf->outSize = (long)((2*adv+4)*ratio+4);
  buf->out=(float*)malloc(buf->outSize*sizeof(float));
//  if (!(buf->out=(float*)malloc(buf->outSize*sizeof(float))))
//    CommonExit(-1,"RateConvInit: Can not allocate memory");

  ibuf = (complex *) malloc((nfn-adv)*sizeof(complex));
//  if (!(ibuf = (complex *) malloc((nfn-adv)*sizeof(complex))))
//    CommonExit(-1,"RateConvInit: Can not allocate memory");


  if ( buf->numChannel==1 ) {
    /* ibuf[?].im not used ... */
    for (i=0; i<(nfn-adv)/2; i++)
      ibuf[i].re=0.0;
    for (   ; i<(nfn-adv)  ; i++)
      ibuf[i].re=0.0;
    /* ibuf[i].re=(real) dataIn[idxIn++]; */
  }
  else {
    for (i=0; i<  (nfn-adv); i++)
      ((real *) ibuf)[i]=0.0;
    for (   ; i<2*(nfn-adv); i++)
      ((real *) ibuf)[i]=0.0;
    /* ((real *) ibuf)[i]=(real) dataIn[idxIn++]; */
  }

  if (RCdebugLevel)
    printf("RateConvInit: inSize=%ld outSize=%ld\n"
	   "  output/input ratio : %8.2f\n"
	   "  cutoff frequency   : %10.4f\n"
	   "  Kaiser parameter   : %10.4f\n"
	   "  filter 1 taps      : %5li\n"
	   "  filter 1 transition: %10.4f\n"
	   "  FFT length         : %5li\n"
	   "  oversampling factor: %5li\n",
	   2*adv,buf->outSize,
	   sf2,fc/(1+numode),a1,(long)2*htaps1+1,0.5*trw2,
	   fn,ov2fac*(2-numode));

  buf->wtab = wtab;
  buf->x= x;
  buf->tp1 = tp1;
  buf->tp2 = tp2;
  buf->ibuf = ibuf;

  buf->nfn = nfn;
  buf->adv = adv;
  buf->istart = istart;
  buf->numode = numode;
  buf->fm = fm;
  buf->p1 = p1;
  buf->d1 = d1;

  return buf;
}


/* RateConv() */
/* Convert sample rate for one frame of audio data. */

long RateConv (
  RCBuf *buf,			/* in: buffer (handle) */
  short *dataIn,		/* in: input data[] */
  long numSampleIn,		/* in: number of input samples */
  float **dataOut)		/* out: output data[] */
				/* returns: */
				/*  numSampleOut */
{
  real *wtab = buf->wtab;
  complex *x = buf->x;
  real *tp1 = buf->tp1;
  real *tp2 = buf->tp2;
  complex *ibuf = buf->ibuf;

  long nfn = buf->nfn;
  long adv = buf->adv;
  long istart = buf->istart;
  long numode = buf->numode;
  long fm = buf->fm;
  double p1 = buf->p1;
  double d1 = buf->d1;

  long i;
  double p,h;

  long idxIn,numOut;
  float *out;

  if (RCdebugLevel)
    printf("RateConv: numSampleIn=%ld\n",numSampleIn);

//  if (numSampleIn != 2*adv)
//    CommonExit(-1,"RateConv: wrong numSampleIn");

  idxIn = 0;
  out = buf->out;

  if ( buf->numChannel==1 ) {
    /* ibuf[?].im not used ... */
    for (i=0      ; i<nfn-adv; i++)
      x[i].re=ibuf[i].re;
    for (         ; i<    adv; i++)
      x[i].re=(real) dataIn[idxIn++];
    for (         ; i<    nfn; i++)
      x[i].re=x[i-adv].im=(real) dataIn[idxIn++];
    for (i=nfn-adv; i<    adv; i++)
      x[i].im=(real) dataIn[idxIn++];
    for (         ; i<    nfn; i++)
      x[i].im=ibuf[i-adv].re=(real) dataIn[idxIn++];
      
    if ( numode )
      ctp1_1(x,tp1,wtab,fm);
    else
      ctp1_2(x,tp1,wtab,fm);
      
    i=adv;
    h=(double) ((2*i)>>numode);
    for (p=p1; p<h; p+=d1) ctp2_m(&(x[istart].re),p,tp2,&out);
    p1=p-h;
      
    i=adv;
    h=(double) ((2*i)>>numode);
    for (p=p1; p<h; p+=d1) ctp2_m(&(x[istart].im),p,tp2,&out);
    p1=p-h;
  }
  else {
    for (i=0; i<nfn-adv; i++)
      x[i]=ibuf[i];
    for (; i<adv; i++) {
      x[i].re=(real) dataIn[idxIn++];
      x[i].im=(real) dataIn[idxIn++];
    }
    for (; i<nfn; i++) {
      x[i].re=ibuf[i-adv].re=(real) dataIn[idxIn++];
      x[i].im=ibuf[i-adv].im=(real) dataIn[idxIn++];
    }
      
    if ( numode )
      ctp1_1(x,tp1,wtab,fm);
    else
      ctp1_2(x,tp1,wtab,fm);
      
    i=2*adv;
    h=(double) (i>>numode);
    for (p=p1; p<h; p+=d1) ctp2_s(x+istart,p,tp2,&out);
    p1=p-h;
  }

  buf->p1 = p1;

  *dataOut = buf->out;
  numOut = out - buf->out;
//  if (numOut > buf->outSize)
//    CommonExit(-1,"RateConv: output buffer size troubles");
  buf->currentSampleIn += 2*adv;
  buf->currentSampleOut += numOut;
  
  return numOut;
}


/* RateConvFree() */
/* Free RateConv buffers. */

void RateConvFree (
  RCBuf *buf)			/* in: buffer (handle) */
{
  if (RCdebugLevel)
    printf("RateConvFree: sampleIn=%ld sampleOut=%ld\n",
	   buf->currentSampleIn,buf->currentSampleOut);
  free(buf->out);
  free(buf->wtab);
  free(buf->ibuf);
  free(buf);
}


/* end of rateconv.c */

