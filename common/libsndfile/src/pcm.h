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


int		pcm_read_sc2s	 (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_uc2s	 (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_bes2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_les2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_bet2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_let2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_bei2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_lei2s (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_read_f2s   (SF_PRIVATE *psf, short *ptr, int len) ;

int		pcm_read_sc2i	 (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_uc2i	 (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_bes2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_les2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_bet2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_let2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_bei2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_lei2i (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_read_f2i   (SF_PRIVATE *psf, int *ptr, int len) ;

int		pcm_read_sc2d	 (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_uc2d	 (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_bes2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_les2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_bet2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_let2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_bei2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_lei2d (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_read_f2d   (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;



int		pcm_write_s2sc  (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2uc  (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2bes (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2les (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2bet (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2let (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2bei (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2lei (SF_PRIVATE *psf, short *ptr, int len) ;
int		pcm_write_s2f   (SF_PRIVATE *psf, short *ptr, int len) ;

int		pcm_write_i2sc  (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2uc  (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2bes (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2les (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2bet (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2let (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2bei (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2lei (SF_PRIVATE *psf, int *ptr, int len) ;
int		pcm_write_i2f   (SF_PRIVATE *psf, int *ptr, int len) ;

int		pcm_write_d2sc  (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2uc  (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2bes (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2les (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2bet (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2let (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2bei (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2lei (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;
int		pcm_write_d2f   (SF_PRIVATE *psf, double *ptr, int len, int normalize) ;

