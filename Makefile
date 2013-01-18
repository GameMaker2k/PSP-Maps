CC ?= gcc
CFLAGS += -O2 -g
SDLCONFIG = sdl-config
CURLCONFIG = curl-config
XMLCONFIG = xml2-config
LIBS = -lSDL_image -lSDL_gfx -lSDL_ttf -lSDL_mixer
PREFIX ?= /usr/local
DESTDIR ?= 

.PHONY: all install uninstall clean

all: pspmaps

pspmaps: pspmaps.c $(ICON) global.o kml.o tile.c io.c
	$(CC) $(CFLAGS) -o pspmaps$(EXEEXT) pspmaps.c $(ICON) global.o kml.o $(LIBS) `$(CURLCONFIG) --cflags --libs` `$(SDLCONFIG) --cflags --libs` `$(XMLCONFIG) --libs`

global.o: global.c global.h
	$(CC) $(CFLAGS) -c global.c

kml.o: kml.c kml.h
	$(CC) $(CFLAGS) -c kml.c `$(SDLCONFIG) --cflags` `$(XMLCONFIG) --cflags`

icon.o: icon.rc
	$(WINDRES) -i icon.rc -o icon.o

install: pspmaps
	install -v -m 0755 -d $(DESTDIR)$(PREFIX)/bin
	install -v -m 0755 ./pspmaps$(EXEEXT) $(DESTDIR)$(PREFIX)/bin

uninstall: pspmaps
	rm -rfv $(DESTDIR)$(PREFIX)/bin/pspmaps$(EXEEXT)

clean:
	rm -rfv pspmaps pspmaps.exe *.o PSP-Maps.prx PSP-Maps.elf PARAM.SFO EBOOT.PBP pspmaps.gpu cache/ data/*.dat kml/
