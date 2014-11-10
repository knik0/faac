lib_LTLIBRARIES = libfaac.la libfaac_drm.la

libfaac_la_SOURCES = aacquant.c bitstream.c fft.c frame.c midside.c psychkni.c util.c backpred.c channels.c filtbank.c huffman.c ltp.c tns.c
libfaac_la_INCLUDES = aacquant.h channels.h filtbank.h hufftab.h psych.h backpred.h coder.h frame.h midside.h tns.h bitstream.h fft.h huffman.h ltp.h util.h
libfaac_la_LIBADD = -lm
libfaac_la_CFLAGS = -fvisibility=hidden
libfaac_drm_la_SOURCES = ${libfaac_la_SOURCES} kiss_fft/kiss_fftr.c kiss_fft/kiss_fft.c
libfaac_drm_la_INCLUDES = ${libfaac_la_INCLUDES}
libfaac_drm_la_LIBADD = ${libfaac_la_LIBADD}
libfaac_drm_la_CFLAGS = ${libfaac_la_CFLAGS} -DDRM

INCLUDES = -I$(top_srcdir)/include


