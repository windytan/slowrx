#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "modespec.h"
#include "pcm.h"

gboolean     Abort           = FALSE;
gboolean     Adaptive        = TRUE;
gboolean    *HasSync         = NULL;
gshort       HedrShift       = 0;
gboolean     ManualActivated = FALSE;
gboolean     ManualResync    = FALSE;
guchar      *StoredLum       = NULL;

pthread_t    thread1;

PicMeta      CurrentPic;

GKeyFile    *config          = NULL;

// Return the FFT bin index matching the given frequency
guint GetBin (double Freq, guint FFTLen) {
  return (Freq / 44100 * FFTLen);
}

// Sinusoid power from complex DFT coefficients
double power (fftw_complex coeff) {
  return pow(coeff[0],2) + pow(coeff[1],2);
}

// Clip to [0..255]
guchar clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  (guchar)round(a);
}

// Convert degrees -> radians
double deg2rad (double Deg) {
  return (Deg / 180) * M_PI;
}

// Convert radians -> degrees
double rad2deg (double rad) {
  return (180 / M_PI) * rad;
}

void ensure_dir_exists(const char *dir) {
  struct stat buf;

  int i = stat(dir, &buf);
  if (i != 0) {
    if (mkdir(dir, 0777) != 0) {
      perror("Unable to create directory for output file");
      exit(EXIT_FAILURE);
    }
  }
}
