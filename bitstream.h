/*
 *	Function prototypes for bitstream writing
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
  $Revision: 1.3 $
  $Date: 2000/10/05 08:39:02 $ (check in)
  $Author: menno $
  *************************************************************************/

#ifndef _bitstream_h_
#define _bitstream_h_

#ifndef min
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif
#ifndef max
#define max(a,b) ( (a) > (b) ? (a) : (b) )
#endif

#define BYTE_NUMBIT 8		/* bits in byte (char) */
#define LONG_NUMBIT 32		/* bits in unsigned long */
#define bit2byte(a) (((a)+BYTE_NUMBIT-1)/BYTE_NUMBIT)
#define byte2bit(a) ((a)*BYTE_NUMBIT)


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
int ByteAlign(BsBitStream* ptrBs, int writeFlag);

#endif

