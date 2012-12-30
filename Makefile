CC = gcc

CFLAGS = -Wall -Wextra -std=gnu99 -pedantic -g `pkg-config --cflags --libs gtk+-3.0`

OFLAGS = -O3

OBJECTS = common.o modespec.o gui.o video.o vis.o sync.o pcm.o fsk.o slowrx.o

all: slowrx

slowrx: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ -lfftw3 -lm -lgthread-2.0 -lpnglite -lasound

%.o: %.c common.h
	$(CC) $(CFLAGS) $(OFLAGS) -c -o $@ $<

clean:
	rm -f slowrx $(OBJECTS)
