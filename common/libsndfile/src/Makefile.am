# This file was automatically generated
# DO NOT EDIT!
#
DIST_SUBDIRS = GSM610 G72x
SUBDIRS = GSM610 G72x

lib_LTLIBRARIES = libsndfile.la
include_HEADERS = sndfile.h
INCLUDES = -I$(srcdir)/../src -I$(srcdir)

noinst_HEADERS = common.h sfendian.h pcm.h ulaw.h alaw.h wav.h au.h

COMMON = common.c newpcm.c pcm.c ulaw.c alaw.c sndfile.c
FILESPECIFIC = aiff.c au.c au_g72x.c paf.c raw.c svx.c wav.c wav_float.c \
				wav_gsm610.c wav_ima_adpcm.c wav_ms_adpcm.c

libsndfile_la_SOURCES = $(FILESPECIFIC) $(COMMON) sndfile.h common.h
libsndfile_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@
libsndfile_la_LIBADD  = GSM610/libgsm.la G72x/libg72x.la

# Disable autoheader. 
AUTOHEADER=echo
