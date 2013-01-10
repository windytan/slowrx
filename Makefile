CC = gcc

CFLAGS    = -Wall -Wextra -std=gnu99 -pedantic -g -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4
GTKCFLAGS = `pkg-config --cflags gtk+-3.0`
GTKLIBS   = `pkg-config --libs gtk+-3.0`

OFLAGS = -O3

OBJECTS = common.o modespec.o gui.o video.o vis.o sync.o pcm.o fsk.o slowrx.o

all: slowrx

slowrx: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(GTKLIBS) -lfftw3 -lgthread-2.0 -lpnglite -lasound -lm

%.o: %.c common.h
	$(CC) $(CFLAGS) $(GTKCFLAGS) $(OFLAGS) -c -o $@ $<

clean:
	rm -f slowrx $(OBJECTS)
