#ifndef DSP_H_
#define DSP_H_

#include <mutex>
#include <thread>
#include "fftw3.h"
#include "common.h"
#include "input.h"

// moment length only affects length of global delay, I/O interval,
// and maximum window size.
#define FFT_LEN     512

#define FREQ_MIN    500.0
#define FREQ_MAX   3300.0
#define FREQ_BLACK 1500.0
#define FREQ_WHITE 2300.0
#define FREQ_SYNC  1200.0

namespace window {
  Wave Hann (int);
  Wave Blackmann (int);
  Wave Rect (int);
  Wave Gauss (int);
}

class Kernel {
  public:
    Kernel(int);
    double at(double) const;
    double getHalfWidth() const;
  private:
    int m_type;
    Wave m_precalc;
};

class DSP {
  public:

    DSP ();
    ~DSP();

    double              calcPeakFreq    (double minf, double maxf, WindowType win_type);
    std::vector<double> calcBandPowerPerHz (const std::vector<std::vector<double>>&, WindowType wintype=WINDOW_HANN2047);
    double              calcVideoSNR    ();
    double              calcVideoLevel  (SSTVMode, bool is_adaptive=false);
    double              calcSyncPower   ();
    void                calcWindowedFFT (WindowType win_type, int fft_len);

    int                 freq2bin        (double freq, int fft_len) const;
    WindowType          bestWindowFor   (SSTVMode, double SNR=99);
    void                set_fshift      (double);

    std::shared_ptr<Input> getInput        ();

  private:

    fftw_complex* m_fft_inbuf;
    fftw_complex* m_fft_outbuf;
    fftw_plan     m_fft_plan;
    std::vector<double> m_mag;
    double        m_fshift;
    double        m_next_snr_time;
    double        m_SNR;
    WindowType    m_sync_window;
    int           m_decim_ratio;
    double        m_freq_if;

    std::vector<Wave> m_window;

    std::shared_ptr<Input> m_input;
};

Wave   convolve     (const Wave&, const Wave&, bool wrap_around=false);
double convolveSingle (const Wave&, const Kernel&, double);
Wave   deriv        (const Wave&);
Wave   peaks        (const Wave&, int);
Wave   derivPeaks   (const Wave&, int);
Wave   rms          (const Wave&, int);
Wave   upsample     (const Wave& orig, double factor, int kern_type, int border_treatment=BORDER_ZERO);
double gaussianPeak (const Wave& signal, int idx_peak, bool wrap_around=false);
double power        (fftw_complex coeff);
double complexMag   (fftw_complex coeff);

#endif
