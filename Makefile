SOURCE=aac_qc.c aac_se_enc.c bitstream.c enc_tf.c encoder.c is.c mc_enc.c ms.c psych.c pulse.c tns.c transfo.c fastfft.c nok_ltp_enc.c nok_pitch.c winswitch.c rateconv.c

CC=gcc
OBJ = $(SOURCE:.c=.o)

CC_OPTS=-DHAS_ULONG -O9 -funroll-loops -finline-functions
LD_OPTS=

TARGETS=faac

%.o: %.c
	$(CC) $(CC_OPTS) -c $< -o $@	

faac: $(OBJ) Makefile
	$(CC) $(LD_OPTS) -o faac $(OBJ) -lsndfile -lm

all: $(TARGETS)

clean:
	rm *.o $(TARGETS)