/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "config.h"
#include "fft.h"
#include "gui.h"
#include "listen.h"
#include "pcm.h"


/*
 * main
 */

int main(int argc, char *argv[]) {
  GString     *confpath;

  gtk_init (&argc, &argv);

  gdk_threads_init ();

  // Load config
  load_config_settings(&confpath);

  // Prepare FFT
  if (fft_init() < 0) {
    exit(EXIT_FAILURE);
  }

  createGUI();
  populateDeviceList();

  gtk_main();

  // Save config on exit
  save_config_settings(confpath);

  g_object_unref(pixbuf_rx);
  free(StoredLum);
  fftw_free(fft.in);
  fftw_free(fft.out);

  return (EXIT_SUCCESS);
}
