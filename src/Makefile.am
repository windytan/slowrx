bin_PROGRAMS = slowrx
slowrx_CPPFLAGS = -g $(GTKMM_CFLAGS) @SNDFILE_CFLAGS@
slowrx_LDADD = $(GTKMM_LIBS) @SNDFILE_LIBS@ -lfftw3
slowrx_SOURCES = slowrx.cc common.cc modespec.cc gui.cc pcm.cc vis.cc video.cc
