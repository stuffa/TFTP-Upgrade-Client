# Copyright  2010  Chris Martin<chris@martin.cc>

WIN32_CC=/usr/bin/i586-mingw32msvc-gcc

WIN32_CFLAGS=-g -Wall -I/usr/i586-mingw32msvc/include

WIN32_LFLAGS=-L/usr/i586-mingwmsvc/lib

all: upgrade-win32 upgrade-linux

upgrade-win32: tftp.c
	mkdir -p  ./win32
	$(WIN32_CC) $(WIN32_CFLAGS) $(WIN32_LFLAGS) -o ./win32/upgrade.exe tftp.c -lwsock32

upgrade-linux: tftp.c
	mkdir -p  ./linux
	$(CC) -o ./linux/upgrade  tftp.c

clean:
	rm -rf ./win32
	rm -rf ./linux
	rm -f *~
	rm -f *.o
