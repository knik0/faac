prefix=/usr/local/bin
CFLAGS=-I../include

faac: main.o
	gcc main.o -o faac -L../libfaac -lfaac -lsndfile

install: faac
	mkdir -p $(prefix)/bin
	cp faac $(prefix)/bin

clean:
	rm faac *.o
