all:
	make -C libfaac
	make -C frontend

install:
	make -C libfaac $@
	make -C frontend $@

uninstall:
	make -C libfaac $@
	make -C frontend $@

clean:
	make -C libfaac $@
	make -C frontend $@
