TARGET = PSP-Maps
OBJS = pspmaps.o

PSP_FW_VERSION = 371
BUILD_PRX = 1

INCDIR = 
CFLAGS = -O2 -G0 -Wall -g
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP-Maps 0.1
PSP_EBOOT_ICON = icon.png
PSP_EBOOT_PIC1 = screenshot.png

PSPSDK=$(shell psp-config --pspsdk-path)
PSPBIN = $(PSPSDK)/../bin

CFLAGS += -I/usr/local/pspdev/psp/include/SDL $(shell $(PSPBIN)/curl-config --cflags)

LIBS = -lc -lSDL_image -lSDL_gfx -lSDL_ttf -lpng -ljpeg -lSDL -lcurl -lfreetype
LIBS += $(shell $(PSPBIN)/sdl-config --libs)
LIBS += $(shell $(PSPBIN)/curl-config --libs)
LIBS +=  -lpspwlan -lpsputility -lpspgum -lpspgu -lm

include $(PSPSDK)/lib/build.mak
