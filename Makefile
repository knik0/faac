SOURCE=aac_qc.c aac_se_enc.c bitstream.c enc_tf.c encoder.c is.c mc_enc.c ms.c psych.c pulse.c tns.c transfo.c fastfft.c nok_ltp_enc.c nok_pitch.c winswitch.c


OBJ = $(SOURCE:.c=.o)

CC_OPTS=-g -DHAS_ULONG -O3

TARGETS=faac

%.o: %.c
	$(CC) $(CC_OPTS) -c $< -o $@	

faac: $(OBJ) Makefile
	gcc -o faac $(OBJ) -lsndfile -lm

all: $(TARGETS)

clean:
	rm *.o $(TARGETS)