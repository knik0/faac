prefix = /usr/local

MIN = 'min(a,b)=( (a) < (b) ? (a) : (b) )'
MAX = 'max(a,b)=( (a) > (b) ? (a) : (b) )'

#CFLAGS := -ansi -pedantic $(CFLAGS)
CFLAGS := -DHAS_ULONG -D$(MIN) -D$(MAX) $(CFLAGS)

SRC= \
aacquant.c \
bitstream.c \
channels.c \
fft.c \
filtbank.c \
frame.c \
huffman.c \
joint.c \
ltp.c \
psych.c \
tns.c \
util.c

OBJ = $(SRC:.c=.o)

all: libfaac.a libfaac.so

libfaac.so: $(OBJ)
	gcc -shared -o libfaac.so $(OBJ) -lm

libfaac.a: $(OBJ)
	ar rcs $@ $(OBJ)
	ranlib $@

install: libfaac.so libfaac.a
	mkdir -p $(prefix)/include
	mkdir -p $(prefix)/lib
	cp ../include/faac.h $(prefix)/include
	cp libfaac.so $(prefix)/lib
	cp libfaac.a $(prefix)/lib

uninstall:
	rm -f $(prefix)/include/faac.h
	rm -f $(prefix)/lib/libfaac.so
	rm -f $(prefix)/lib/libfaac.a

clean:
	rm -f $(OBJ)
	rm -f libfaac.a libfaac.so
