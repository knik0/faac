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
 * $Id: aacinfo.c,v 1.1 2001/10/11 09:53:44 menno Exp $
 */

#include <windows.h>
#include "aacinfo.h"

#define ADIF_MAX_SIZE 30 /* Should be enough */
#define ADTS_MAX_SIZE 10 /* Should be enough */

const int sample_rates[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000};

static int read_ADIF_header(HANDLE file, faadAACInfo *info)
{
	unsigned long tmp;
	int bitstream;
	unsigned char buffer[ADIF_MAX_SIZE];
	int skip_size = 0;
	int sf_idx;

	/* Get ADIF header data */
	
	info->headertype = 1;

	ReadFile(file, buffer, ADIF_MAX_SIZE, &tmp, 0);

	/* copyright string */	
	if(buffer[4] & 128)
		skip_size += 9; /* skip 9 bytes */

	bitstream = buffer[4 + skip_size] & 16;
	info->bitrate = ((unsigned int)(buffer[4 + skip_size] & 0x0F)<<19)|
		((unsigned int)buffer[5 + skip_size]<<11)|
		((unsigned int)buffer[6 + skip_size]<<3)|
		((unsigned int)buffer[7 + skip_size] & 0xE0);

	if (bitstream == 0) {
		info->object_type = ((buffer[9 + skip_size]&0x01)<<1)|((buffer[10 + skip_size]&0x80)>>7);
		sf_idx = (buffer[10 + skip_size]&0x78)>>3;
		info->channels = ((buffer[10 + skip_size]&0x07)<<1)|((buffer[11 + skip_size]&0x80)>>7);
	} else {
		info->object_type = (buffer[7 + skip_size] & 0x18)>>3;
		sf_idx = ((buffer[7 + skip_size] & 0x07)<<1)|((buffer[8 + skip_size] & 0x80)>>7);
		info->channels = (buffer[8 + skip_size]&0x78)>>3;
	}
	info->sampling_rate = sample_rates[sf_idx];

	return 0;
}

static int read_ADTS_header(HANDLE file, faadAACInfo *info, int *seek_table,
							int tagsize)
{
	/* Get ADTS header data */
	unsigned char buffer[ADTS_MAX_SIZE];
	int frames, t_framelength = 0, frame_length, sr_idx, ID;
	int second = 0, pos;
	float frames_per_sec = 0;
	unsigned long bytes;

	info->headertype = 2;

	/* Seek to the first frame */
	SetFilePointer(file, tagsize, NULL, FILE_BEGIN);

	/* Read all frames to ensure correct time and bitrate */
	for(frames=0; /* */; frames++)
	{
		/* 12 bit SYNCWORD */
		ReadFile(file, buffer, ADTS_MAX_SIZE, &bytes, 0);
		if(bytes != ADTS_MAX_SIZE)
		{
			/* Bail out if no syncword found */
			break;
		}

		if (!((buffer[0] == 0xFF)&&((buffer[1] & 0xF6) == 0xF0)))
			break;

		pos = SetFilePointer(file, 0, NULL, FILE_CURRENT) - ADTS_MAX_SIZE;

		if(!frames)
		{
			/* fixed ADTS header is the same for every frame, so we read it only once */ 
			/* Syncword found, proceed to read in the fixed ADTS header */ 
			ID = buffer[1] & 0x08;
			info->object_type = (buffer[2]&0xC0)>>6;
			sr_idx = (buffer[2]&0x3C)>>2;
			info->channels = ((buffer[2]&0x01)<<2)|((buffer[3]&0xC0)>>6);

			frames_per_sec = sample_rates[sr_idx] / 1024.f;
		}

		/* ...and the variable ADTS header */
		if (ID == 0) {
			info->version = 4;
			frame_length = (((unsigned int)buffer[4]) << 5) |
				((unsigned int)buffer[5] >> 3);
		} else { /* MPEG-2 */
			info->version = 2;
			frame_length = ((((unsigned int)buffer[3] & 0x3)) << 11)
				| (((unsigned int)buffer[4]) << 3) | (buffer[5] >> 5);
		}

		t_framelength += frame_length;

		if (frames > second*frames_per_sec)
		{
			seek_table[second] = pos;
			second++;
		}

		SetFilePointer(file, frame_length - ADTS_MAX_SIZE, NULL, FILE_CURRENT);
	}

	info->sampling_rate = sample_rates[sr_idx];
	info->bitrate = (int)(((t_framelength / frames) * (info->sampling_rate/1024.0)) +0.5)*8;
	info->length = (int)((float)(frames/frames_per_sec))*1000;

	return 0;
}

static int f_id3v2_tag(HANDLE file)
{
	unsigned char buffer[10];
	unsigned long tmp;

	ReadFile(file, buffer, 10, &tmp, 0);

	if (StringComp(buffer, "ID3", 3) == 0) {
		unsigned long tagsize;

		/* high bit is not used */
		tagsize = (buffer[6] << 21) | (buffer[7] << 14) |
			(buffer[8] <<  7) | (buffer[9] <<  0);

		tagsize += 10;

		SetFilePointer(file, tagsize, NULL, FILE_BEGIN);

		return tagsize;
	} else {
		SetFilePointer(file, 0, NULL, FILE_BEGIN);

		return 0;
	}
}

int get_AAC_format(char *filename, faadAACInfo *info, int *seek_table)
{
	unsigned int tagsize;
	HANDLE file;
	unsigned long file_len;
	unsigned char adxx_id[5];
	unsigned long tmp;

	if(StringComp(filename, "http://", 7) == 0)
	{
		info->version = 2;
		info->length = 0;
		info->bitrate = 128000;
		info->sampling_rate = 44100;
		info->channels = 2;
		info->headertype = 0;
		info->object_type = 1;

		return 0;
	}

	file = CreateFile(filename, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
	if (file == INVALID_HANDLE_VALUE)
		return -1;

	file_len = GetFileSize(file, NULL);

	tagsize = f_id3v2_tag(file); /* Skip the tag, if it's there */
	file_len -= tagsize;

	ReadFile(file, adxx_id, 4, &tmp, 0);
	SetFilePointer(file, tagsize, NULL, FILE_BEGIN);

	adxx_id[5-1] = 0;

	info->length = 0;

	if(StringComp(adxx_id, "ADIF", 4) == 0)
	{
		read_ADIF_header(file, info);
	}
	else
	{
		if ((adxx_id[0] == 0xFF)&&((adxx_id[1] & 0xF6) == 0xF0))
		{
//			SetFilePointer(file, tagsize, NULL, FILE_BEGIN);
			read_ADTS_header(file, info, seek_table, tagsize);
		}
		else
		{
			/* Unknown/headerless AAC file, assume format: */
			info->version = 2;
			info->bitrate = 128000;
			info->sampling_rate = 44100;
			info->channels = 2;
			info->headertype = 0;
			info->object_type = 1;
		}
	}

	if (info->length == 0)
		info->length = (int)((file_len/(((info->bitrate*8)/1024)*16))*1000);

	CloseHandle(file);

	return 0;
}

int StringComp(char const *str1, char const *str2, unsigned long len)
{
	signed int c1 = 0, c2 = 0;

	while (len--) {
		c1 = *str1++;
		c2 = *str2++;

		if (c1 == 0 || c1 != c2)
			break;
	}

	return c1 - c2;
}
