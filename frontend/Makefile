prefix = /usr/local

CFLAGS := -L../libfaad -I../include $(CFLAGS)

faac: main.c
	gcc $(CFLAGS) -o faac -lfaac -lsndfile main.c

install: faac
	mkdir -p $(prefix)/bin
	cp faac $(prefix)/bin

uninstall:
	rm -f $(prefix)/bin/faac

clean:
	rm -f faac *.o
