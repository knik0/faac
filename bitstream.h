
#ifndef _bitstream_h_
#define _bitstream_h_

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif
#ifndef max
#define max(a,b) ( (a) > (b) ? (a) : (b) )
#endif

typedef struct _bitstream
{
  unsigned char *data;		/* data bits */
  long numBit;			/* number of bits in buffer */
  long size;			/* buffer size in bits */
  long currentBit;		/* current bit position in bit stream */
  long numByte;			/* number of bytes read/written (only file) */
} BsBitStream;


BsBitStream *BsOpenWrite(int size);

void BsClose(BsBitStream *bs);

long BsBufferNumBit (BsBitStream *bs);

int BsWriteByte (BsBitStream *stream,
				 unsigned long data,
				 int numBit);

int BsPutBit (BsBitStream *stream,
			  unsigned long data,
			  int numBit);

/* ByteAlign(...), used to byte align bitstream */
int ByteAlign(BsBitStream* ptrBs);

#endif

