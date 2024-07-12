#ifndef _FFT_H_
#define _FFT_H_

#define FFT_BUFFER_SZ	(2048)
#define FFT_HALF_SZ	(FFT_BUFFER_SZ/2)
#define FFT_FULL_SZ	(FFT_BUFFER_SZ)

typedef struct _FFTStuff FFTStuff;
struct _FFTStuff {
  double       *in;
  fftw_complex *out;
  fftw_plan     PlanHalf;
  fftw_plan     PlanFull;
};
extern FFTStuff fft;

int fft_init(void);
#endif
