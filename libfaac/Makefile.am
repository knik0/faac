common_SOURCES = aacquant.c bitstream.c fft.c frame.c midside.c blockswitch.c util.c channels.c filtbank.c huffman.c tns.c quantize.c
common_INCLUDES = aacquant.h channels.h filtbank.h hufftab.h blockswitch.h coder.h frame.h midside.h tns.h bitstream.h fft.h huffman.h util.h quantize.h version.h
common_LIBADD = -lm
common_CFLAGS = -fvisibility=hidden

if USE_DRM
lib_LTLIBRARIES = libfaac_drm.la
libfaac_drm_la_SOURCES = ${common_SOURCES} ${common_INCLUDES} kiss_fft/kiss_fftr.c kiss_fft/kiss_fft.c
libfaac_drm_la_LIBADD = ${common_LIBADD}
libfaac_drm_la_CFLAGS = ${common_CFLAGS} -DDRM
else
lib_LTLIBRARIES = libfaac.la
libfaac_la_SOURCES = ${common_SOURCES} ${common_INCLUDES}
libfaac_la_LIBADD = ${common_LIBADD}
libfaac_la_CFLAGS = ${common_CFLAGS}
endif

libfaac_la_LDFLAGS = -no-undefined

AM_CPPFLAGS = -I$(top_srcdir)/include
