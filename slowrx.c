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
  fft.in = fftw_alloc_real(2048);
  if (fft.in == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }
  fft.out = fftw_alloc_complex(2048);
  if (fft.out == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(fft.in);
    exit(EXIT_FAILURE);
  }
  memset(fft.in,  0, sizeof(double) * 2048);

  fft.Plan1024 = fftw_plan_dft_r2c_1d(1024, fft.in, fft.out, FFTW_ESTIMATE);
  fft.Plan2048 = fftw_plan_dft_r2c_1d(2048, fft.in, fft.out, FFTW_ESTIMATE);

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
