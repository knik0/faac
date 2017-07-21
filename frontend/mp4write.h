/****************************************************************************
    MP4 output module

    Copyright (C) 2017 Krzysztof Nikiel

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/

#include <stdint.h>

typedef struct
{
    uint32_t samplerate;
    // total sound samples
    uint32_t samples;
    uint32_t channels;
    // sample depth
    uint32_t bits;
    // buffer config
    uint16_t buffersize;
    uint32_t bitratemax;
    uint32_t bitrateavg;
    uint32_t framesamples;
    struct
    {
        uint16_t *data;
        uint32_t ents;
        uint32_t bufsize;
    } frame;
    // AudioSpecificConfig data:
    struct
    {
        uint8_t *data;
        unsigned long size;
    } asc;
    uint32_t mdatofs;
    uint32_t mdatsize;

    struct
    {
        // meta fields
        const char *encoder;
        const char *artist;
        const char *composer;
        const char *title;
        const char *genre;
        const char *album;
        uint8_t compilation;
        uint32_t trackno;
        uint32_t discno;
        const char *year;
        const char *cover;      // cover filename
        const char *comment;
    } tag;
} mp4config_t;

extern mp4config_t mp4config;

int mp4atom_open(char *name);
int mp4atom_head(void);
int mp4atom_tail(void);
int mp4atom_frame(uint8_t * bitbuf, int bytesWritten, int frame_samples);
int mp4atom_close(void);
