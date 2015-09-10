#ifndef DSP_H_
#define DSP_H_

#include "portaudio.h"
#include <sndfile.hh>
#include <gtkmm.h>
#include "fftw3.h"
#include "common.h"

// moment length only affects length of global delay, I/O interval,
// and maximum window size.
#define MOMENT_LEN     2047
#define FFT_LEN_SMALL  1024
#define FFT_LEN_BIG    2048
#define CIRBUF_LEN_FACTOR 8
#define CIRBUF_LEN ((MOMENT_LEN+1)*CIRBUF_LEN_FACTOR)
#define READ_CHUNK_LEN ((MOMENT_LEN+1)/2)

#define FREQ_MIN    500.0
#define FREQ_MAX   3300.0
#define FREQ_BLACK 1500.0
#define FREQ_WHITE 2300.0
#define FREQ_SYNC  1200.0

namespace window {
  Wave Hann (size_t);
  Wave Blackmann (size_t);
  Wave Rect (size_t);
  Wave Gauss (size_t);
}

class DSP {

  public:

    DSP();

    void   openAudioFile (std::string);
    void   openPortAudio ();
    void   readMore ();
    double forward (unsigned);
    double forward ();
    double forward_time (double);
    void   forward_to_time (double);
    void   set_fshift (double);

    void   windowedMoment (WindowType, fftw_complex*);
    double peakFreq (double, double, WindowType);
    int    freq2bin        (double, int) const;
    std::vector<double> bandPowerPerHz (const std::vector<std::vector<double>>&, WindowType wintype=WINDOW_HANN2047);
    WindowType bestWindowFor (SSTVMode, double SNR=99);
    double videoSNR();
    double lum(SSTVMode, bool is_adaptive=false);
    
    bool   is_open   () const;
    double get_t     () const;
    bool   isLive    () const;
    double syncPower ();

    void setLatestPixbuf(Glib::RefPtr<Gdk::Pixbuf>);
    Glib::RefPtr<Gdk::Pixbuf> getLatestPixbuf();

  private:

    //mutable Glib::Threads::Mutex Mutex;
    double*   cirbuf_;
    int       cirbuf_head_;
    int       cirbuf_tail_;
    int       cirbuf_fill_count_;
    bool      please_stop_;
    float*  read_buffer_;
    SndfileHandle file_;
    fftw_complex *fft_inbuf_;
    fftw_complex *fft_outbuf_;
    fftw_plan     fft_plan_small_;
    fftw_plan     fft_plan_big_;
    double  samplerate_;
    size_t num_chans_;
    PaStream *pa_stream_;
    eStreamType stream_type_;
    bool is_open_;
    double t_;
    double fshift_;
    double next_snr_time_;
    double SNR_;
    Glib::RefPtr<Gdk::Pixbuf> latest_pixbuf_;
    WindowType sync_window_;

    mutable Glib::Threads::Mutex mutex_;
    
    std::vector<Wave> window_;
};

class MyPortaudioClass{

  int myMemberCallback(const void *input, void *output,
      unsigned long frameCount,
      const PaStreamCallbackTimeInfo* timeInfo,
      PaStreamCallbackFlags statusFlags);

  static int myPaCallback(
      const void *input, void *output,
      unsigned long frameCount,
      const PaStreamCallbackTimeInfo* timeInfo,
      PaStreamCallbackFlags statusFlags,
      void *userData ) {
      return ((MyPortaudioClass*)userData)
             ->myMemberCallback(input, output, frameCount, timeInfo, statusFlags);
  }
};

Wave convolve (const Wave&, const Wave&, bool wrap_around=false);
Wave deriv (const Wave&);
Wave peaks (const Wave&, size_t);
Wave derivPeaks (const Wave&, size_t);
Wave rms (const Wave&, int);
Wave upsample    (const Wave& orig, size_t factor, int kern_type);
double   gaussianPeak  (double y1, double y2, double y3);
double   power         (fftw_complex coeff);
double complexMag (fftw_complex coeff);
#endif
