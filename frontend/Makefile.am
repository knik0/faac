if !USE_DRM # no frontend for DRM version

bin_PROGRAMS = faac
dist_man_MANS = ../docs/faac.1

faac_SOURCES = main.c input.c mp4write.c input.h mp4write.h
EXTRA_faac_SOURCES = getopt.c

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = $(top_builddir)/libfaac/libfaac.la -lm

if MINGW
LDADD += -lws2_32
endif

endif # !USE_DRM
