CC ?= gcc
AR ?= ar
RANLIB ?= ranlib

CFLAGS    = -Wall -Wextra -std=gnu99 -pedantic -g
GLIBCFLAGS = $(shell pkg-config --cflags glib-2.0)
GLIBLIBS   = $(shell pkg-config --libs glib-2.0)

GTKCFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTKLIBS   = $(shell pkg-config --libs gtk+-3.0)

OFLAGS = -O3

GUI_BIN = slowrx
COMMON_LIB = libslowrx.a

TARGETS = $(GUI_BIN)

COMMON_CFLAGS  = $(CFLAGS) $(OFLAGS)
COMMON_LDFLAGS = -lfftw3 -lasound -lm -lpthread

LIB_SOURCES = common.c fft.c fsk.c listen.c modespec.c sync.c pic.c pcm.c vis.c video.c
LIB_OBJECTS = $(patsubst %.c,%.o,$(LIB_SOURCES))
LIB_DEPENDS = $(patsubst %.c,%.d,$(LIB_SOURCES))
LIB_CFLAGS  = $(COMMON_CFLAGS) $(GLIBCFLAGS)

GUI_SOURCES = config.c gui.c slowrx.c
GUI_OBJECTS = $(patsubst %.c,%.o,$(GUI_SOURCES))
GUI_DEPENDS = $(patsubst %.c,%.d,$(GUI_SOURCES))
GUI_CFLAGS  = $(COMMON_CFLAGS) $(GTKCFLAGS) -DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_4
GUI_LDFLAGS = $(COMMON_LDFLAGS) -lgthread-2.0 $(GLIBLIBS) $(GTKLIBS)

OBJECTS = $(GUI_OBJECTS) $(LIB_OBJECTS)
DEPENDS = $(GUI_DEPENDS) $(LIB_DEPENDS)

all: $(TARGETS)

$(GUI_BIN): $(COMMON_LIB) $(GUI_OBJECTS)
	$(CC) $(GUI_CFLAGS) -o $@ -Wl,--as-needed -Wl,--start-group $^ $(GUI_LDFLAGS) -Wl,--end-group 

$(COMMON_LIB): $(LIB_OBJECTS)
	$(AR) cr $@ $^
	$(RANLIB) $@

%.o: %.c
	$(CC) -MM -MF $(*F).d $(OBJ_CFLAGS) $<
	$(CC) $(OBJ_CFLAGS) -c -o $@ $<

$(GUI_OBJECTS): OBJ_CFLAGS=$(GUI_CFLAGS)
$(LIB_OBJECTS): OBJ_CFLAGS=$(LIB_CFLAGS)

clean:
	rm -f $(TARGETS) $(COMMON_LIB) $(OBJECTS) $(DEPENDS)

-include $(DEPENDS)

gui.c: aboutdialog.ui slowrx.ui
