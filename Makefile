all:
	make -C libfaac
	make -C frontend

clean:
	make -C libfaac clean
	make -C frontend clean
