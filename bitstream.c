/*
 *	Bitstream writing
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
  $Revision: 1.4 $
  $Date: 2000/10/05 08:39:02 $ (check in)
  $Author: menno $
  *************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitstream.h"		/* bit stream module */



BsBitStream *BsOpenWrite(int size)
{
	BsBitStream *bs;

	bs = malloc(sizeof(BsBitStream));
	bs->size = size;
	bs->numBit = 0;
	bs->currentBit = 0;
	bs->data = malloc(bit2byte(size));
	memset(bs->data, 0, bit2byte(size));

	return bs;
}

void BsClose(BsBitStream *bs)
{
	free(bs->data);
	free(bs);
}

long BsBufferNumBit (BsBitStream *bs)
{
	return bs->numBit;
}

int BsWriteByte (BsBitStream *stream,
				 unsigned long data,
				 int numBit)
{
	long numUsed,idx;

	idx = (stream->currentBit / BYTE_NUMBIT) % bit2byte(stream->size);
	numUsed = stream->currentBit % BYTE_NUMBIT;
	if (numUsed == 0)
		stream->data[idx] = 0;
	stream->data[idx] |= (data & ((1<<numBit)-1)) <<
		(BYTE_NUMBIT-numUsed-numBit);
	stream->currentBit += numBit;
	stream->numBit = stream->currentBit;

	return 0;
}

int BsPutBit (BsBitStream *stream,
			  unsigned long data,
			  int numBit)
{
	int num,maxNum,curNum;
	unsigned long bits;

	if (numBit == 0)
		return 0;

	/* write bits in packets according to buffer byte boundaries */
	num = 0;
	maxNum = BYTE_NUMBIT - stream->currentBit % BYTE_NUMBIT;
	while (num < numBit) {
		curNum = min(numBit-num,maxNum);
		bits = data>>(numBit-num-curNum);
		if (BsWriteByte(stream,bits,curNum)) {
			return 1;
		}
		num += curNum;
		maxNum = BYTE_NUMBIT;
	}

	return 0;
}

int ByteAlign(BsBitStream* ptrBs, int writeFlag)
{
	int len, i,j;
	len = BsBufferNumBit( ptrBs );
	   
	j = (8 - (len%8))%8;

	if ((len % 8) == 0) j = 0;
	if (writeFlag) {
		for( i=0; i<j; i++ ) {
			BsPutBit( ptrBs, 0, 1 ); 
		}
	}
	return j;
}


