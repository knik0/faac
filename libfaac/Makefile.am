common_SOURCES = bitstream.c fft.c frame.c blockswitch.c util.c channels.c filtbank.c tns.c quantize.c huff2.c huffdata.c stereo.c
common_INCLUDES = channels.h filtbank.h blockswitch.h coder.h frame.h tns.h bitstream.h fft.h util.h quantize.h huffdata.h huff2.h stereo.h
common_LIBADD = -lm
common_CFLAGS = -fvisibility=hidden
if CPUSSE
common_CFLAGS += -msse2
endif

if USE_DRM
lib_LTLIBRARIES = libfaac_drm.la
libfaac_drm_la_SOURCES = ${common_SOURCES} ${common_INCLUDES} kiss_fft/kiss_fftr.c kiss_fft/kiss_fft.c kiss_fft/kiss_fft.h
EXTRA_libfaac_drm_la_SOURCES = kiss_fft/_kiss_fft_guts.h kiss_fft/kiss_fftr.h kiss_fft/README kiss_fft/README.kiss_fft
libfaac_drm_la_LIBADD = ${common_LIBADD}
libfaac_drm_la_CFLAGS = ${common_CFLAGS} -DDRM
else
lib_LTLIBRARIES = libfaac.la
libfaac_la_SOURCES = ${common_SOURCES} ${common_INCLUDES}
libfaac_la_LIBADD = ${common_LIBADD}
libfaac_la_CFLAGS = ${common_CFLAGS}
endif

libfaac_la_LDFLAGS = -no-undefined
if MINGW
libfaac_la_LDFLAGS += -Wl,--add-stdcall-alias
endif

AM_CPPFLAGS = -I$(top_srcdir)/include
