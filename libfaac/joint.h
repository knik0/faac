/*
 * FAAC - Freeware Advanced Audio Coder
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
 * $Id: joint.h,v 1.5 2001/06/08 18:01:09 menno Exp $
 */

#ifndef JOINT_H
#define JOINT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "coder.h"


void MSEncode(CoderInfo *coderInfo, ChannelInfo *channelInfo, double *spectrum[MAX_CHANNELS],
              unsigned int numberOfChannels, unsigned int msenable);
void MSReconstruct(CoderInfo *coderInfo, ChannelInfo *channelInfo, int numberOfChannels);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JOINT_H */
