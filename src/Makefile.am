bin_PROGRAMS = slowrx
slowrx_CPPFLAGS = -Wall -Wextra -pedantic -g $(GTKMM_CFLAGS) @SNDFILE_CFLAGS@ $(PORTAUDIO_CFLAGS) $(DBG_FLAGS)
slowrx_CCFLAGS = -Wall -Wextra -pedantic -g
slowrx_LDADD = $(GTKMM_LIBS) @SNDFILE_LIBS@ -lfftw3 $(PORTAUDIO_LIBS)
slowrx_SOURCES = slowrx.cc common.cc modespec.cc gui.cc dsp.cc picture.cc video.cc header.cc listener.cc
