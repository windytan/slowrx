#include <string.h>
#include <fftw3.h>

#include "fft.h"

FFTStuff     fft;

int fft_init(void) {
  fft.in = fftw_alloc_real(2048);
  if (fft.in == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    return -1;
  }
  fft.out = fftw_alloc_complex(2048);
  if (fft.out == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(fft.in);
    return -1;
  }
  memset(fft.in,  0, sizeof(double) * 2048);

  fft.Plan1024 = fftw_plan_dft_r2c_1d(1024, fft.in, fft.out, FFTW_ESTIMATE);
  fft.Plan2048 = fftw_plan_dft_r2c_1d(2048, fft.in, fft.out, FFTW_ESTIMATE);

  return 0;
}
