CFLAGS = -Wall -O2 -g
SDLCONFIG = sdl-config
CURLCONFIG = curl-config
LIBS = -lSDL_image -lSDL_gfx -lSDL_ttf -lSDL_mixer

pspmaps: pspmaps.c $(ICON)
	$(CC) $(CFLAGS) -o pspmaps$(EXT) pspmaps.c $(ICON) $(LIBS) `$(CURLCONFIG) --cflags --libs` `$(SDLCONFIG) --cflags --libs`

icon.o: icon.rc
	$(WINDRES) -i icon.rc -o icon.o

clean:
	rm -f pspmaps pspmaps.exe pspmaps.o icon.o PSP-Maps.prx PSP-Maps.elf PARAM.SFO EBOOT.PBP pspmaps.gpu
