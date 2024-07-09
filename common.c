#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "common.h"
#include "modespec.h"
#include "pcm.h"

_Bool     Abort           = FALSE;
_Bool     Adaptive        = TRUE;
_Bool    *HasSync         = NULL;
_Bool     ManualActivated = FALSE;
_Bool     ManualResync    = FALSE;
uint8_t   *StoredLum       = NULL;

// Return the FFT bin index matching the given frequency
uint32_t GetBin (double Freq, uint32_t FFTLen) {
  return (Freq / 44100 * FFTLen);
}

// Sinusoid power from complex DFT coefficients
double power (double complex coeff) {
  return pow(creal(coeff),2) + pow(cimag(coeff),2);
}

// Clip to [0..255]
uint8_t clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  (uint8_t)round(a);
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
