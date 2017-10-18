if !USE_DRM # no frontend for DRM version

bin_PROGRAMS = faac
dist_man_MANS = ../docs/faac.1

faac_SOURCES = main.c input.c mp4write.c input.h mp4write.h
EXTRA_faac_SOURCES = getopt.c faacgui.rc icon.rc faac.ico

AM_CPPFLAGS = -I$(top_srcdir)/include
faac_LDADD = $(top_builddir)/libfaac/libfaac.la -lm

if MINGW
bin_PROGRAMS += faacgui
faacgui_SOURCES = maingui.c input.c input.h resource.h
faac_LDADD += -lws2_32 icon.o
faacgui_LDADD = $(faac_LDADD) faacguires.o -lcomdlg32
faacguires.o:
	$(WINDRES) faacgui.rc faacguires.o
icon.o:
	$(WINDRES) icon.rc icon.o
endif

endif # !USE_DRM
