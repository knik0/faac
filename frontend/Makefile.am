bin_PROGRAMS = faac

faac_SOURCES = faac.c input.c

CXXFLAGS = -O2
CFLAGS = -O2 -Wall
INCLUDES = -I$(top_srcdir)/include 
LDFLAGS = -s
LDADD = $(top_builddir)/libfaac/libfaac.la -lm

