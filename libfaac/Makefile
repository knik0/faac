prefix=/usr/local/lib

CFLAGS=-ansi -pedantic

SRC= \
aacquant.c\
bitstream.c\
channels.c\
fft.c\
filtbank.c\
frame.c\
huffman.c\
joint.c\
psych.c\
util.c

OBJ := $(SRC:.c=.o)

all: libfaac.a libfaac.so

libfaac.so: $(OBJ)
	gcc -shared -o libfaac.so $(OBJ) -lm

libfaac.a: $(OBJ)
	ar rcs $@ $(OBJ)
	ranlib $@

install: libfaac.so libfaac.a
	mkdir -p $(prefix)/include
	mkdir -p $(prefix)/lib
	cp ../include/faac.h $(prefix)/include/faac.h
	cp libfaac.so $(prefix)/lib/libfaac.so
	cp libfaac.a $(prefix)/lib/libfaac.a

clean:
	rm -f $(OBJ)
	rm -f libfaac.a libfaac.so
