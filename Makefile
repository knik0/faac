SOURCE=aac_back_pred.c aac_qc.c aac_se_enc.c bitstream.c enc_tf.c encoder.c imdct.c is.c mc_enc.c ms.c psych.c pulse.c tns.c transfo.c fastfft.c


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