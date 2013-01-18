CC ?= gcc
CFLAGS += -O2 -g -Wall `sdl-config --cflags` `curl-config --cflags` `xml2-config --cflags`
LIBS = -lSDL_image -lSDL_gfx -lSDL_ttf -lSDL_mixer `sdl-config --libs` `curl-config --libs` `xml2-config --libs`
PREFIX ?= /usr/local
DESTDIR ?= 

.PHONY: all install uninstall clean

all: pspmaps

pspmaps: pspmaps.c $(ICON) global.o kml.o tile.c io.c
	$(CC) $(CFLAGS) -o pspmaps$(EXEEXT) pspmaps.c $(ICON) global.o kml.o $(LIBS)

global.o: global.c global.h
	$(CC) $(CFLAGS) -c global.c

kml.o: kml.c kml.h
	$(CC) $(CFLAGS) -c kml.c

icon.o: icon.rc
	$(WINDRES) -i icon.rc -o icon.o

install: pspmaps
	install -v -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	install -v -m 0755 ./pspmaps$(EXEEXT) $(DESTDIR)$(PREFIX)/bin

uninstall: pspmaps
	rm -rfv $(DESTDIR)$(PREFIX)/bin/pspmaps$(EXEEXT)

clean:
	rm -rfv pspmaps pspmaps.exe *.o PSP-Maps.prx PSP-Maps.elf PARAM.SFO EBOOT.PBP pspmaps.gpu cache/ data/*.dat kml/
