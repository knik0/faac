# Recomended compile options for gcc with a PentII or better for maximum speed
# are: -O6 -fomit-frame-pointer -march=i686 -funroll-loops -finline-functions
# Note: for fastest compilation, -funroll-loops -finline-function
# -fomit-frame-pointer and the highest -O that is safe with your compiler
# is STRONGLY recommended!

prefix = /usr/local

DESTDIR = 

SOURCE=aac_qc.c aac_se_enc.c bitstream.c enc_tf.c encoder.c is.c mc_enc.c ms.c psych.c pulse.c tns.c transfo.c fastfft.c nok_ltp_enc.c nok_pitch.c winswitch.c rateconv.c faac.c

OBJ = $(SOURCE:.c=.o)

CC=gcc

#PROFILE_OPTS = -pg -a

CC_OPTS=-DHAS_ULONG $(CFLAGS) $(PROFILE_OPTS)
LD_OPTS=

TARGETS=faac

all: $(TARGETS)

faac: $(OBJ) Makefile
	$(CC) $(LD_OPTS) -o faac $(OBJ) -lsndfile -lm $(PROFILE_OPTS)

%.o: %.c
	$(CC) $(CC_OPTS) -c $< -o $@	

install:
	@if test -f faac; then \
	  install -d ${DESTDIR}${prefix}/bin && \
	  install faac ${DESTDIR}${prefix}/bin; \
	fi

uninstall:
	@rm -f ${DESTDIR}${prefix}/bin/faac

clean:
	@rm -f *.o $(TARGETS)
