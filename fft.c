#include <string.h>
#include <fftw3.h>

#include "fft.h"

FFTStuff     fft;

int fft_init(void) {
  fft.in = fftw_alloc_real(FFT_BUFFER_SZ);
  if (fft.in == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    return -1;
  }
  fft.out = fftw_alloc_complex(FFT_BUFFER_SZ);
  if (fft.out == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(fft.in);
    return -1;
  }
  memset(fft.in,  0, sizeof(double) * FFT_BUFFER_SZ);

  fft.PlanHalf = fftw_plan_dft_r2c_1d(FFT_HALF_SZ, fft.in, fft.out, FFTW_ESTIMATE);
  fft.PlanFull = fftw_plan_dft_r2c_1d(FFT_FULL_SZ, fft.in, fft.out, FFTW_ESTIMATE);

  return 0;
}
