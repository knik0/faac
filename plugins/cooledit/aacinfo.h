/*
 * FAAD - Freeware Advanced Audio Decoder
 * Copyright (C) 2001 Menno Bakker
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: aacinfo.h,v 1.7 2002/08/22 22:58:57 menno Exp $
 */

typedef struct {
	int version;
	int channels;
	int sampling_rate;
	int bitrate;
	int length;
	int object_type;
	int headertype;
} faadAACInfo;


int get_AAC_format(char *filename, faadAACInfo *info, int *seek_table);

static int f_id3v2_tag(HANDLE file);
static int read_ADIF_header(HANDLE file, faadAACInfo *info);
static int read_ADTS_header(HANDLE file, faadAACInfo *info, int *seek_table,
							int tagsize);
int StringComp(char const *str1, char const *str2, unsigned long len);
