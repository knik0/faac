/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2001 Menno Bakker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: util.c,v 1.3 2001/02/10 12:28:54 menno Exp $
 */

/* Returns the sample rate index */
int GetSRIndex(unsigned int sampleRate)
{
	if (92017 <= sampleRate) return 0;
	if (75132 <= sampleRate) return 1;
	if (55426 <= sampleRate) return 2;
	if (46009 <= sampleRate) return 3;
	if (37566 <= sampleRate) return 4;
	if (27713 <= sampleRate) return 5;
	if (23004 <= sampleRate) return 6;
	if (18783 <= sampleRate) return 7;
	if (13856 <= sampleRate) return 8;
	if (11502 <= sampleRate) return 9;
	if (9391 <= sampleRate) return 10;

	return 11;
}

/* Returns the maximum bitrate per channel for that sampling frequency */
unsigned int MaxBitrate(unsigned long sampleRate)
{
	/*
	 *  Maximum of 6144 bit for a channel
	 */
	return (int)(6144.0 * (double)sampleRate/1024.0 + .5);
}

/* Returns the minimum bitrate per channel for that sampling frequency */
unsigned int MinBitrate(unsigned long sampleRate)
{
	return 8000;
}

int max(a, b)
{
	return (((a) > (b)) ? (a) : (b));
}

int min(a, b)
{
	return (((a) < (b)) ? (a) : (b));
}
