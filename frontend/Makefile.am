bin_PROGRAMS = faac
man_MANS = ../docs/faac.1

faac_SOURCES = main.c input.c mp4atom.c

AM_CPPFLAGS = -I$(top_srcdir)/include
LDADD = $(top_builddir)/libfaac/libfaac.la -lm

if MINGW
LDADD += -lws2_32
endif
