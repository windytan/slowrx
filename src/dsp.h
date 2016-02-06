#ifndef DSP_H_
#define DSP_H_

#include <mutex>
#include <thread>
#include <complex>
#include "fftw3.h"
#include "common.h"
#include "input.h"

namespace window {
  Wave Hann (int);
  Wave Blackmann (int);
  Wave Rect (int);
  Wave Gauss (int,double,double amp=1.0);
}

class Kernel {
  public:
    Kernel(int,double scale=1.0);
    double at(double) const;
    double getSupport() const;
  private:
    const int m_type;
    const double m_scale;
    Wave m_precalc;
};

class FM {
  public:

    FM (int rate, std::shared_ptr<CirBuffer<std::complex<double>>> cirbuf);
    //~FM();

    double              calcPeakFreq    (double minf, double maxf, int which_window=0);
    std::vector<double> calcBandPowerPerHz (const std::vector<std::vector<double>>&, int which_window=0);
    double              calcVideoSNR    ();
    double              calcVideoLevel  (const ModeSpec&, int denoise_level=0);
    double              calcSyncPower   ();
    void                calcPowerSpectrum (int which_window=0);
    std::vector<double> getLastPowerSpectrum() const;
    void                setRate(int rate);

    int                 freq2bin        (double freq) const;
    double              bin2freq(double bin) const;
    //WindowType          bestWindowFor   (const ModeSpec&, double SNR=99);
    void                set_fshift      (double);

  private:

    int           m_samplerate;
    std::vector<std::complex<double>> m_fft_inbuf;
    std::vector<std::complex<double>> m_fft_outbuf;
    fftw_plan     m_fft_plan;
    std::vector<double> m_mag;
    double        m_fshift;
    double        m_next_snr_time;
    double        m_SNR;
    //WindowType    m_sync_window;
    int           m_fft_decim_ratio;
    const double        m_freq_if;
    const int     m_fft_len;
    std::vector<double> m_lpf_coeffs;

    std::vector<Wave> m_window;
    std::shared_ptr<CirBuffer<std::complex<double>>> m_input_cirbuf;

};

Wave   convolve     (const Wave&, const Kernel&, int border_treatment=BORDER_ZERO);
double convolveSingle (const Wave&, const Kernel&, double, int border_treatment=BORDER_ZERO);
Wave   deriv        (const Wave&);
std::vector<double>   peaks        (const Wave&, int);
std::vector<double>   derivPeaks   (const Wave&, int);
Wave   rms          (const Wave&, int);
Wave   upsample     (const Wave& orig, double factor, int kern_type, int border_treatment=BORDER_ZERO);
double gaussianPeak (const Wave& signal, int idx_peak, bool wrap_around=false);
double power        (fftw_complex coeff);
double complexMag   (fftw_complex coeff);
double sqComplexMag (fftw_complex coeff);
std::complex<double> mixComplex(double input, double if_phi);

#endif
