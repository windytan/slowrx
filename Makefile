CC = gcc

CFLAGS = -Wall -Wextra -std=gnu99 -pedantic -g -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4

OFLAGS = -O3

all: slowrx


slowrx: common.h common.c slowrx.c gui.c video.c sync.c vis.c modespec.c pcm.c fsk.c
	$(CC) $(CFLAGS) $(GFLAGS) $(OFLAGS) common.c modespec.c gui.c video.c vis.c sync.c pcm.c fsk.c slowrx.c -o slowrx -lfftw3 -lm `pkg-config --cflags --libs gtk+-3.0` -lgthread-2.0 -lpnglite -lasound

clean:
	rm -f *.o
