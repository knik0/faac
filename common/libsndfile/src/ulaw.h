/*
** Copyright (C) 1999-2000 Erik de Castro Lopo <erikd@zip.com.au>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


int		ulaw_read_ulaw2s (SF_PRIVATE *psf, short *ptr, unsigned int len) ;
int		ulaw_read_ulaw2i (SF_PRIVATE *psf, short *ptr, unsigned int len) ;
int		ulaw_read_ulaw2d (SF_PRIVATE *psf, short *ptr, unsigned int len) ;

int		ulaw_write_s2ulaw (SF_PRIVATE *psf, short *ptr, unsigned int len) ;
int		ulaw_write_i2ulaw (SF_PRIVATE *psf, short *ptr, unsigned int len) ;
int		ulaw_write_d2ulaw (SF_PRIVATE *psf, short *ptr, unsigned int len) ;

