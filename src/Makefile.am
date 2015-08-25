bin_PROGRAMS = slowrx
slowrx_CPPFLAGS = -g $(GTKMM_CFLAGS) @SNDFILE_CFLAGS@ $(PORTAUDIO_CFLAGS)
slowrx_LDADD = $(GTKMM_LIBS) @SNDFILE_LIBS@ -lfftw3 $(PORTAUDIO_LIBS)
slowrx_SOURCES = slowrx.cc common.cc modespec.cc gui.cc dsp.cc vis.cc video.cc sync.cc tests.cc
