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
 * $Id: channels.h,v 1.3 2001/02/26 14:07:55 oxygene Exp $
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __unix__
#define min(a,b) ( (a) < (b) ? (a) : (b) )
#endif

#include "coder.h"

typedef struct {
    int is_present;  
    int ms_used[MAX_SCFAC_BANDS];
    int ms_usedS[8][MAX_SCFAC_BANDS];
} MSInfo;

typedef struct {
	int tag;
	int present;
	int ch_is_left;
	int paired_ch;
	int common_window;
	int cpe;
	int sce;
	int lfe;
	MSInfo msInfo;
} ChannelInfo;

void GetChannelInfo(ChannelInfo *channelInfo, int numChannels, int useLfe);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CHANNEL_H */
