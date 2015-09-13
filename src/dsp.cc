#include <cmath>
#include "dsp.h"
#include "gui.h"

bool g_is_pa_initialized = false;

DSP::DSP() :
  cirbuf_(CIRBUF_LEN*2), is_open_(false), t_(0), fshift_(0), sync_window_(WINDOW_HANN511), read_buffer_(nullptr),
  window_(16)
{

  cirbuf_.head = MOMENT_LEN/2;

  window_[WINDOW_HANN95]   = window::Hann(95);
  window_[WINDOW_HANN127]  = window::Hann(127);
  window_[WINDOW_HANN255]  = window::Hann(255);
  window_[WINDOW_HANN511]  = window::Hann(511);
  window_[WINDOW_HANN1023] = window::Hann(1023);
  window_[WINDOW_HANN2047] = window::Hann(2047);
  window_[WINDOW_CHEB47]   = {
    0.0004272315,0.0013212953,0.0032312239,0.0067664313,0.0127521667,0.0222058684,
    0.0363037629,0.0563165400,0.0835138389,0.1190416120,0.1637810511,0.2182020094,
    0.2822270091,0.3551233730,0.4354402894,0.5210045495,0.6089834347,0.6960162864,
    0.7784084484,0.8523735326,0.9143033652,0.9610404797,0.9901263448,1.0000000000,
    0.9901263448,0.9610404797,0.9143033652,0.8523735326,0.7784084484,0.6960162864,
    0.6089834347,0.5210045495,0.4354402894,0.3551233730,0.2822270091,0.2182020094,
    0.1637810511,0.1190416120,0.0835138389,0.0563165400,0.0363037629,0.0222058684,
    0.0127521667,0.0067664313,0.0032312239,0.0013212953,0.0004272315
  };

  fft_inbuf_ = new fftw_complex[FFT_LEN_BIG];//fftw_alloc_complex(FFT_LEN_BIG);
  if (fft_inbuf_ == NULL) {
    perror("unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }
  fft_outbuf_ = new fftw_complex[FFT_LEN_BIG];//fftw_alloc_complex(FFT_LEN_BIG);
  if (fft_outbuf_ == NULL) {
    perror("unable to allocate memory for FFT");
    fftw_free(fft_inbuf_);
    exit(EXIT_FAILURE);
  }
  for (size_t i=0; i<FFT_LEN_BIG; i++) {
    fft_inbuf_[i][0] = fft_inbuf_[i][1] = 0;
    fft_outbuf_[i][0] = fft_outbuf_[i][1] = 0;
  }

  fft_plan_small_ = fftw_plan_dft_1d(FFT_LEN_SMALL, fft_inbuf_, fft_outbuf_, FFTW_FORWARD, FFTW_ESTIMATE);
  fft_plan_big_   = fftw_plan_dft_1d(FFT_LEN_BIG, fft_inbuf_, fft_outbuf_, FFTW_FORWARD, FFTW_ESTIMATE);

}


void DSP::openAudioFile (std::string fname) {

  if (is_open_)
    close();

  fprintf (stderr,"open '%s'\n", fname.c_str()) ;
  file_ = SndfileHandle(fname.c_str()) ;

  if (file_.error()) {
    fprintf(stderr,"sndfile: %s\n", file_.strError());
  } else {
    fprintf (stderr,"  opened @ %d Hz, %d ch\n", file_.samplerate(), file_.channels()) ;

    samplerate_ = file_.samplerate();

    stream_type_ = STREAM_TYPE_FILE;

    num_chans_ = file_.channels();
    read_buffer_ = new float [READ_CHUNK_LEN * num_chans_];
    is_open_ = true;
  }
}

void DSP::openPortAudio (int n) {

  if (!is_open_) {

    if (!g_is_pa_initialized) {
      printf("Pa_Initialize\n");
      PaError err = Pa_Initialize();
      if (err == paNoError)
        g_is_pa_initialized = true;
    }

    PaStreamParameters inputParameters;
    inputParameters.device = (n < 0 ? Pa_GetDefaultInputDevice() : n);
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency ;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    const PaDeviceInfo *devinfo;
    devinfo = Pa_GetDeviceInfo(inputParameters.device);

    printf("Pa_OpenStream\n");
    int err = Pa_OpenStream(
              &pa_stream_,
              &inputParameters,
              NULL,
              44100,
              READ_CHUNK_LEN,
              paClipOff,
              &DSP::PaStaticCallback,
              this );

    if (!err) {
      num_chans_ = 1;
      printf("make readbuffer: %d floats\n",READ_CHUNK_LEN * num_chans_);
      delete read_buffer_;
      read_buffer_ = new float [READ_CHUNK_LEN * num_chans_]();

      err = Pa_StartStream( pa_stream_ );

      const PaStreamInfo *streaminfo;
      streaminfo = Pa_GetStreamInfo(pa_stream_);
      samplerate_ = streaminfo->sampleRate;
      fprintf(stderr,"  opened '%s' @ %.1f\n",devinfo->name,samplerate_);

      stream_type_ = STREAM_TYPE_PA;

      if (err == paNoError) {
        is_open_ = true;
        //readMore();
      } else {
        fprintf(stderr,"  error at Pa_StartStream\n");
      }
    } else {
      fprintf(stderr,"  error\n");
    }
  }
}

void DSP::close () {
  if (is_open_) {
    if (stream_type_ == STREAM_TYPE_PA) {
      printf("Pa_CloseStream\n");
      Pa_CloseStream(pa_stream_);
    }
    is_open_ = false;
  }
}

void DSP::set_fshift (double fshift) {
  fshift_ = fshift;
}

int DSP::freq2bin (double freq, int fft_len) const {
  return (freq / samplerate_ * fft_len);
}

bool DSP::is_open () const {
  return is_open_;
}

double DSP::get_t() const {
  return t_;
}

bool DSP::isLive() const {
  return (stream_type_ == STREAM_TYPE_PA);
}

int DSP::PaCallback(const void *input, void *output,
    unsigned long framesread,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags) {

  //printf("PaCallback\n");
  float* in = (float*)input;

  {
    std::lock_guard<std::mutex> guard(buffer_mutex_);
    for (unsigned i = 0; i<READ_CHUNK_LEN; i++)
      read_buffer_[i] = in[i * num_chans_];
  }

  readBufferTransfer(framesread);

  return 0;
}

void DSP::readMoreFromFile() {
  unsigned long framesread = 0;
  sf_count_t fr = file_.readf(read_buffer_, READ_CHUNK_LEN);
  if (fr < READ_CHUNK_LEN) {
    is_open_ = false;
    if (fr < 0) {
      framesread = 0;
    } else {
      framesread = fr;
    }
  } else {
    framesread = fr;
  }

  if (num_chans_ > 1) {
    for (size_t i=0; i<READ_CHUNK_LEN; i++) {
      read_buffer_[i] = read_buffer_[i*num_chans_];
    }
  }

  readBufferTransfer(framesread);
}


void DSP::readBufferTransfer (unsigned long framesread) {

  {
    std::lock_guard<std::mutex> guard(buffer_mutex_);

    int cirbuf_fits = std::min(CIRBUF_LEN - cirbuf_.head, int(framesread));

    for (size_t i=0; i<cirbuf_fits; i++)
      cirbuf_.data[cirbuf_.head + i] = read_buffer_[i];

    // wrap around
    if (framesread > cirbuf_fits) {
      for (size_t i=0; i<(framesread - cirbuf_fits); i++)
        cirbuf_.data[i] = read_buffer_[cirbuf_fits + i];
    }

    // mirror
    for (size_t i=0; i<CIRBUF_LEN; i++)
      cirbuf_.data[CIRBUF_LEN + i] = cirbuf_.data[i];

    cirbuf_.head = (cirbuf_.head + framesread) % CIRBUF_LEN;
    cirbuf_.fill_count += framesread;
    cirbuf_.fill_count = std::min(int(cirbuf_.fill_count), CIRBUF_LEN);
  }
}

// move processing window
double DSP::forward (unsigned nsamples) {
  for (unsigned i = 0; i < nsamples; i++) {

    {
      std::lock_guard<std::mutex> guard(buffer_mutex_);
      cirbuf_.tail = (cirbuf_.tail + 1) % CIRBUF_LEN;
      cirbuf_.fill_count -= 1;
    }

    if (cirbuf_.fill_count < MOMENT_LEN) {
      while (cirbuf_.fill_count < MOMENT_LEN) {
        if (stream_type_ == STREAM_TYPE_PA) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else if (stream_type_ == STREAM_TYPE_FILE) {
          readMoreFromFile();
        }
      }
    }
  }
  double dt = 1.0 * nsamples / samplerate_;
  t_ += dt;
  return dt;
}
double DSP::forward () {
  return forward(1);
}
double DSP::forward_time(double sec) {
  double start_t = t_;
  while (t_ < start_t + sec)
    forward();
  return t_ - start_t;
}


// the current moment, windowed
// will be written over buf
// which MUST fit FFT_LEN_BIG * fftw_complex
void DSP::windowedMoment (WindowType win_type, fftw_complex* buf) {

  for (size_t i=0; i<FFT_LEN_BIG; i++)
    buf[i][0] = buf[i][1] = 0;

  //double if_phi = 0;
  for (int i = 0; i < MOMENT_LEN; i++) {

    size_t win_i = i - MOMENT_LEN/2 + window_[win_type].size()/2 ;

    if (win_i < window_[win_type].size()) {
      double a;
      //fftw_complex mixed;
      a = cirbuf_.data[cirbuf_.tail + i] * window_[win_type][win_i];

      /*// mix to IF
      mixed[0] = a * cos(if_phi) - a * sin(if_phi);
      mixed[1] = a * cos(if_phi) + a * sin(if_phi);
      if_phi += 2 * M_PI * 10000 / samplerate_;*/

      buf[win_i][0] = buf[win_i][1] = a;
    }
  }

}

double DSP::peakFreq (double minf, double maxf, WindowType wintype) {

  unsigned fft_len = (window_[wintype].size() <= FFT_LEN_SMALL ? FFT_LEN_SMALL : FFT_LEN_BIG);

  windowedMoment(wintype, fft_inbuf_);
  fftw_execute(fft_len == FFT_LEN_BIG ? fft_plan_big_ : fft_plan_small_);

  size_t peakBin = 0;
  unsigned lobin = freq2bin(minf, fft_len);
  unsigned hibin = freq2bin(maxf, fft_len);
  for (size_t i = lobin-1; i <= hibin+1; i++) {
    mag_[i] = complexMag(fft_outbuf_[i]);
    if ( (i >= lobin && i <= hibin && i<(fft_len/2+1) ) &&
         (peakBin == 0 || mag_[i] > mag_[peakBin]))
      peakBin = i;
  }

  double result = peakBin + gaussianPeak(mag_[peakBin-1], mag_[peakBin], mag_[peakBin+1]);

  // In Hertz
  result = result / fft_len * samplerate_ + fshift_;

  // cheb47 @ 44100 can't resolve <1700 Hz
  if (result < 1700 && wintype == WINDOW_CHEB47)
    result = peakFreq (minf, maxf, WINDOW_HANN95);

  return result;

}

Wave DSP::bandPowerPerHz(const std::vector<std::vector<double>>& bands, WindowType wintype) {

  unsigned fft_len = (window_[wintype].size() <= FFT_LEN_SMALL ? FFT_LEN_SMALL : FFT_LEN_BIG);

  windowedMoment(wintype, fft_inbuf_);
  fftw_execute(fft_len == FFT_LEN_BIG ? fft_plan_big_ : fft_plan_small_);

  Wave result;
  for (Wave band : bands) {
    double P = 0;
    double binwidth = 1.0 * samplerate_ / fft_len;
    int nbins = 0;
    for (int i = freq2bin(band[0]+fshift_, fft_len); i <= freq2bin(band[1]+fshift_, fft_len); i++) {
      P += pow(complexMag(fft_outbuf_[i]), 2);
      nbins++;
    }
    P = (binwidth*nbins == 0 ? 0 : P/(binwidth*nbins));
    result.push_back(P);
  }
  return result;
}

WindowType DSP::bestWindowFor(SSTVMode Mode, double SNR) {
  WindowType WinType;

  //double samplesInPixel = 1.0 * samplerate_ * ModeSpec[Mode].tScan / ModeSpec[Mode].ScanPixels;

  if      (SNR >=  23 && Mode != MODE_PD180 && Mode != MODE_SDX)  WinType = WINDOW_CHEB47;
  else if (SNR >=  12)  WinType = WINDOW_HANN95;
  else if (SNR >=   8)  WinType = WINDOW_HANN127;
  else if (SNR >=   5)  WinType = WINDOW_HANN255;
  else if (SNR >=   4)  WinType = WINDOW_HANN511;
  else if (SNR >=  -7)  WinType = WINDOW_HANN1023;
  else                  WinType = WINDOW_HANN2047;

  return WinType;
}

double DSP::videoSNR () {
  if (t_ >= next_snr_time_) {
    std::vector<double> bands = bandPowerPerHz(
        {{FREQ_SYNC-1000,FREQ_SYNC-200},
        {FREQ_BLACK,FREQ_WHITE},
        {FREQ_WHITE+400, FREQ_WHITE+700}}
    );
    double Pvideo_plus_noise = bands[1];
    double Pnoise_only       = (bands[0] + bands[2]) / 2;
    double Psignal = Pvideo_plus_noise - Pnoise_only;

    SNR_ = ((Pnoise_only == 0 || Psignal / Pnoise_only < .01) ?
        -20 : 10 * log10(Psignal / Pnoise_only));

    next_snr_time_ = t_ + 50e-3;
  }

  return SNR_;
}

double DSP::syncPower () {
  std::vector<double> bands = bandPowerPerHz(
      {{FREQ_SYNC-50,FREQ_SYNC+50},
      {FREQ_BLACK,FREQ_WHITE}},
      sync_window_
  );
  double sync;
  if (bands[1] == 0.0 || bands[0] > 4 * bands[1]) {
    sync = 2.0;
  } else {
    sync = bands[0] / (2 * bands[1]);
  }
  return sync;
}

double DSP::lum (SSTVMode mode, bool is_adaptive) {
  WindowType win_type;

  if (is_adaptive) win_type = bestWindowFor(mode, videoSNR());
  else             win_type = bestWindowFor(mode);

  double freq = peakFreq(FREQ_BLACK, FREQ_WHITE, win_type);
  return fclip((freq - FREQ_BLACK) / (FREQ_WHITE - FREQ_BLACK));
}

void DSP::setLatestPixbuf(Glib::RefPtr<Gdk::Pixbuf> pb) {
  Glib::Threads::Mutex::Lock lock (mutex_);
  latest_pixbuf_ = pb;
}
Glib::RefPtr<Gdk::Pixbuf> DSP::getLatestPixbuf() {
  Glib::Threads::Mutex::Lock lock (mutex_);
  return latest_pixbuf_;
}

// param:  y values around peak
// return: peak x position (-1 .. 1)
double gaussianPeak (double y1, double y2, double y3) {
  if (2*y2 - y3 - y1 == 0) {
    return 0;
  } else {
    return ((y3 - y1) / (2 * (2*y2 - y3 - y1)));
  }
}
/*WindowType DSPworker::bestWindowFor(SSTVMode Mode) {
  return bestWindowFor(Mode, 20);
}*/

namespace window {
  Wave Hann (size_t winlen) {
    Wave result(winlen);
    for (size_t i=0; i < winlen; i++)
      result[i] = 0.5 * (1 - cos( (2 * M_PI * i) / (winlen)) );
    return result;
  }

  Wave Blackmann (size_t winlen) {
    Wave result(winlen);
    for (size_t i=0; i < winlen; i++)
      result[i] = 0.42 - 0.5*cos(2*M_PI*i/winlen) - 0.08*cos(4*M_PI*i/winlen);

    return result;
  }

  Wave Rect (size_t winlen) {
    Wave result(winlen);
    double sigma = 0.4;
    for (size_t i=0; i < winlen; i++)
      result[i] = exp(-0.5*((i-(winlen-1)/2)/(sigma*(winlen-1)/2)));

    return result;
  }

  Wave Gauss (size_t winlen) {
    Wave result(winlen);
    for (size_t i=0; i < winlen; i++)
      result[i] = 1;

    return result;
  }
}

double sinc (double x) {
  return (x == 0 ? 1 : sin(M_PI*x) / (M_PI*x));
}

namespace kernel {
  Wave Lanczos (size_t kernel_len, size_t a) {
    Wave kern(kernel_len);
    for (size_t i=0; i<kernel_len; i++) {
      double x_kern = (1.0*i/(kernel_len-1) - .5)*2*a;
      double x_wind = 2.0*i/(kernel_len-1) - 1;
      kern[i] = sinc(x_kern) * sinc(x_wind);
    }
    return kern;
  }

  Wave Tent (size_t kernel_len) {
    Wave kern(kernel_len);
    for (size_t i=0; i<kernel_len; i++) {
      double x = 1.0*i/(kernel_len-1);
      kern[i] = 1-2*fabs(x-0.5);
    }
    return kern;
  }
}

double complexMag (fftw_complex coeff) {
  return sqrt(pow(coeff[0],2) + pow(coeff[1],2));
}


Wave convolve (const Wave& sig, const Wave& kernel, bool wrap_around) {

  assert (kernel.size() % 2 == 1);

  Wave result(sig.size());

  for (size_t i=0; i<sig.size(); i++) {

    for (size_t i_kern=0; i_kern<kernel.size(); i_kern++) {
      int i_new = i - kernel.size()/2  + i_kern;
      if (wrap_around) {
        if (i_new < 0)
          i_new += result.size();
        result[i_new % result.size()] += sig[i] * kernel[i_kern];
      } else {
        if (i_new >= 0 && i_new <= int(result.size()-1))
          result[i_new] += sig[i] * kernel[i_kern];
      }
    }

  }

  return result;
}

Wave upsample (const Wave& orig, size_t factor, int kern_type) {

  Wave kern;
  if (kern_type == KERNEL_LANCZOS2) {
    kern = kernel::Lanczos(factor*2*2 + 1, 2);
  } else if (kern_type == KERNEL_LANCZOS3) {
    kern = kernel::Lanczos(factor*3*2 + 1, 3);
  } else if (kern_type == KERNEL_TENT) {
    kern = kernel::Tent(factor*2 + 1);
  }

  size_t orig_size = orig.size();

  Wave padded(orig_size * factor);
  for (size_t i=0; i<orig_size; i++) {
    padded[i * factor] = orig[i];
  }
  padded.insert(padded.begin(), factor-1, 0);
  padded.insert(padded.begin(), orig[0]);
  padded.push_back(orig[orig_size-1]);

  Wave filtered = convolve(padded, kern);

  filtered.erase(filtered.begin(), filtered.begin()+factor/2);
  filtered.erase(filtered.end()-factor/2, filtered.end());

  return filtered;
}

Wave deriv (const Wave& wave) {
  Wave result;
  for (size_t i=1; i<wave.size(); i++)
    result.push_back(wave[i] - wave[i-1]);
  return result;
}

std::vector<double> peaks (const Wave& wave, size_t n) {
  std::vector<std::pair<double,double> > peaks;
  for (size_t i=0; i<wave.size(); i++) {
    double y1 = (i==0 ? wave[0] : wave[i-1]);
    double y2 = wave[i];
    double y3 = (i==wave.size()-1 ? wave[wave.size()-1] : wave[i+1]);
    if ( fabs(y2) >= fabs(y1) && fabs(y2) >= fabs(y3) )
      peaks.push_back({ i + gaussianPeak(y1, y2, y3), wave[i]});
  }
  std::sort(peaks.begin(), peaks.end(),
    [](std::pair<double,double> a, std::pair<double,double> b) {
      return fabs(b.second) < fabs(a.second);
    });

  Wave result;
  for (size_t i=0;i<n && i<peaks.size(); i++)
    result.push_back(peaks[i].first);

  std::sort(result.begin(), result.end());

  return result;
}


std::vector<double> derivPeaks (const Wave& wave, size_t n) {
  std::vector<double> result = peaks(deriv(wave), n);
  for (size_t i=0; i<result.size(); i++) {
    result[i] += .5;
  }
  return result;
}

/* pass:     wave      FM demodulated signal
 *           melody    array of Tones
 *                     (zero frequency to accept any)
 *           dt        sample delta
 *
 * returns:  (bool)    did we find it
 *           (double)  at which frequency shift
 *           (double)  started how many seconds before the last sample
 */
std::tuple<bool,double,double> findMelody (const Wave& wave, const Melody& melody, double dt, double min_shift, double max_shift) {
  bool   was_found = true;
  int    start_at = 0;
  double avg_fdiff = 0;
  double freq_margin = 25;
  double tshift = 0;
  double t = melody[melody.size()-1].dur;
  std::vector<double> fdiffs;

  for (int i=melody.size()-2; i>=0; i--)  {
    if (melody[i].freq != 0) {
      double delta_f_ref = melody[i].freq - melody[melody.size()-1].freq;
      double delta_f     = wave[wave.size()-1 - (t/dt)] - wave[wave.size()-1];
      double err_f       = delta_f - delta_f_ref;
      was_found = was_found && (fabs(err_f) < freq_margin);
    }
    start_at = wave.size() - (t / dt);
    t += melody[i].dur;
  }

  if (was_found) {

    /* refine fshift */
    int melody_i = 0;
    double next_tone_t = melody[melody_i].dur;
    for (size_t i=start_at; i<wave.size(); i++) {
      double fref = melody[melody_i].freq;
      double fdiff = (wave[i] - fref);
      fdiffs.push_back(fdiff);
      if ( (i-start_at)*dt >= next_tone_t ) {
        melody_i ++;
        next_tone_t += melody[melody_i].dur;
      }
    }
    std::sort(fdiffs.begin(), fdiffs.end());
    avg_fdiff = fdiffs[fdiffs.size()/2];

    /* refine start_at */
    Wave subwave(wave.begin()+start_at, wave.end());
    Wave edges_rx = derivPeaks(subwave, melody.size()-1);
    Wave edges_ref;
    double t = 0;
    for (size_t i=0; i<melody.size()-1; i++) {
      t += melody[i].dur;
      edges_ref.push_back(t);
    }

    tshift = 0;
    if (edges_rx.size() == edges_ref.size()) {
      for (size_t i=0; i<edges_rx.size(); i++) {
        tshift += (edges_rx[i]*dt - edges_ref[i]);
      }
      tshift = start_at*dt + (tshift / edges_rx.size()) - ((wave.size()-1)*dt);
    } else {
      // can't refine
      tshift = start_at*dt - ((wave.size()-1)*dt);
    }

  }

  if (avg_fdiff < min_shift || avg_fdiff > max_shift)
    was_found = false;


  return { was_found, avg_fdiff, tshift };
}

std::vector<std::pair<int,std::string>> listPortaudioDevices() {
  std::vector<std::pair<int,std::string>> result;
  if (!g_is_pa_initialized) {
    printf("Pa_Initialize\n");
    PaError err = Pa_Initialize();
    if (err == paNoError)
      g_is_pa_initialized = true;
  }
  int numDevices = Pa_GetDeviceCount();
  const   PaDeviceInfo *device_info;
  for(int i=0; i<numDevices; i++) {
    device_info = Pa_GetDeviceInfo(i);
    if (device_info->maxInputChannels > 0) {
      result.push_back({i, device_info->name});
    }
  }
  return result;
}

int getDefaultPaDevice() {
  return Pa_GetDefaultInputDevice();
}
