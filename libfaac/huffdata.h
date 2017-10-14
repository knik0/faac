/****************************************************************************
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

typedef struct {
    const uint16_t len;
    const uint16_t data;
} hcode16_t;

typedef struct {
    const uint32_t len;
    const uint32_t data;
} hcode32_t;

extern hcode16_t book01[81];
extern hcode16_t book02[81];
extern hcode16_t book03[81];
extern hcode16_t book04[81];
extern hcode16_t book05[81];
extern hcode16_t book06[81];
extern hcode16_t book07[64];
extern hcode16_t book08[64];
extern hcode16_t book09[169];
extern hcode16_t book10[169];
extern hcode16_t book11[289];
extern hcode32_t book12[121];
