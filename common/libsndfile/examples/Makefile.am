noinst_PROGRAMS = sfconvert sndfile2oct wav32_aiff24 make_sine sfinfo sfhexdump

SNDFILEDIR =../src
INCLUDES = -I$(srcdir)/$(SNDFILEDIR)

sfconvert_SOURCES = sfconvert.c 
sfconvert_LDADD = $(SNDFILEDIR)/libsndfile.la

sndfile2oct_SOURCES = sndfile2oct.c 
sndfile2oct_LDADD = $(SNDFILEDIR)/libsndfile.la

wav32_aiff24_SOURCES = wav32_aiff24.c 
wav32_aiff24_LDADD = $(SNDFILEDIR)/libsndfile.la

make_sine_SOURCES = make_sine.c 
make_sine_LDADD = $(SNDFILEDIR)/libsndfile.la -lm

sfinfo_SOURCES = sfinfo.c 
sfinfo_LDADD = $(SNDFILEDIR)/libsndfile.la

sfhexdump_SOURCES = sfhexdump.c 
sfhexdump_LDADD = $(SNDFILEDIR)/libsndfile.la
