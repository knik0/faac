noinst_PROGRAMS = sfversion read_seek_test double_test write_read_test \
	lossy_comp_test error_test ulaw_test alaw_test sftest

SNDFILEDIR =../src
INCLUDES = -I$(srcdir)/$(SNDFILEDIR)

sfversion_SOURCES = sfversion.c 
sfversion_LDADD = $(SNDFILEDIR)/libsndfile.la

read_seek_test_SOURCES = read_seek_test.c 
read_seek_test_LDADD = $(SNDFILEDIR)/libsndfile.la

write_read_test_SOURCES = write_read_test.c 
write_read_test_LDADD = $(SNDFILEDIR)/libsndfile.la

lossy_comp_test_SOURCES = lossy_comp_test.c 
lossy_comp_test_LDADD = $(SNDFILEDIR)/libsndfile.la -lm

error_test_SOURCES = error_test.c 
error_test_LDADD = $(SNDFILEDIR)/libsndfile.la

ulaw_test_SOURCES = ulaw_test.c 
ulaw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

alaw_test_SOURCES = alaw_test.c 
alaw_test_LDADD = $(SNDFILEDIR)/libsndfile.la

double_test_SOURCES = double_test.c 
double_test_LDADD = $(SNDFILEDIR)/libsndfile.la -lm

sftest_SOURCES = sftest.c 
sftest_LDADD = $(SNDFILEDIR)/libsndfile.la

check: generic-test ulaw_test alaw_test double_test first-test \
		wav-tests aiff-tests au-tests raw-tests paf-tests svx-tests

generic-test : error_test
	./error_test
	./ulaw_test
	./alaw_test
	./double_test
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed common tests."
	@echo "--------------------------------------------------------------------"

first-test: read_seek_test
	./read_seek_test wav
	./read_seek_test aiff
	./read_seek_test au
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed first set of tests."
	@echo "--------------------------------------------------------------------"

wav-tests: write_read_test
	./write_read_test wav
	./lossy_comp_test wav_ima
	./lossy_comp_test wav_msadpcm
	./lossy_comp_test wav_ulaw
	./lossy_comp_test wav_alaw
	./lossy_comp_test wav_gsm610
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on WAV files."
	@echo "--------------------------------------------------------------------"

aiff-tests: write_read_test
	./write_read_test aiff
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on AIFF files."
	@echo "--------------------------------------------------------------------"

au-tests: write_read_test
	./write_read_test au
	./write_read_test aule
	./lossy_comp_test au_ulaw
	./lossy_comp_test au_alaw
	./lossy_comp_test aule_ulaw
	./lossy_comp_test aule_alaw
	./lossy_comp_test au_g721
	./lossy_comp_test au_g723
	./lossy_comp_test aule_g721
	./lossy_comp_test aule_g723
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on AU files."
	@echo "--------------------------------------------------------------------"

raw-tests: write_read_test
	./write_read_test raw
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on RAW (header-less PCM) files."
	@echo "--------------------------------------------------------------------"

paf-tests: write_read_test
	./write_read_test paf
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on PAF files."
	@echo "--------------------------------------------------------------------"

svx-tests: write_read_test
	./write_read_test svx
	@echo "--------------------------------------------------------------------"
	@echo "  `./sfversion` passed tests on SVX files."
	@echo "--------------------------------------------------------------------"

