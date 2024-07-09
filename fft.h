#ifndef _FFT_H_
#define _FFT_H_
typedef struct _FFTStuff FFTStuff;
struct _FFTStuff {
  double       *in;
  fftw_complex *out;
  fftw_plan     Plan1024;
  fftw_plan     Plan2048;
};
extern FFTStuff fft;

int fft_init(void);
#endif
