CC = gcc

CFLAGS    = -Wall -Wextra -std=gnu99 -pedantic -g -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4
GTKCFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTKLIBS   = $(shell pkg-config --libs gtk+-3.0)

OFLAGS = -O3

SOURCES = common.c modespec.c gui.c video.c vis.c sync.c pcm.c fsk.c slowrx.c
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
DEPENDS = $(patsubst %.c,%.d,$(SOURCES))

all: slowrx

slowrx: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(GTKLIBS) -lfftw3 -lgthread-2.0 -lasound -lm -lpthread

%.o: %.c common.h
	$(CC) -MM -MF $(*F).d $(CFLAGS) $(GTKCFLAGS) $(OFLAGS) $<
	$(CC) $(CFLAGS) $(GTKCFLAGS) $(OFLAGS) -c -o $@ $<

clean:
	rm -f slowrx $(OBJECTS) $(DEPENDS)

-include $(DEPENDS)
