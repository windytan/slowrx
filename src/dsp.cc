#include "common.hh"
#include "portaudio.h"

DSPworker::DSPworker() : Mutex(), please_stop_(false) {

  std::vector<double> cheb47 = {
    0.0004272315,0.0013212953,0.0032312239,0.0067664313,0.0127521667,0.0222058684,
    0.0363037629,0.0563165400,0.0835138389,0.1190416120,0.1637810511,0.2182020094,
    0.2822270091,0.3551233730,0.4354402894,0.5210045495,0.6089834347,0.6960162864,
    0.7784084484,0.8523735326,0.9143033652,0.9610404797,0.9901263448,1.0000000000,
    0.9901263448,0.9610404797,0.9143033652,0.8523735326,0.7784084484,0.6960162864,
    0.6089834347,0.5210045495,0.4354402894,0.3551233730,0.2822270091,0.2182020094,
    0.1637810511,0.1190416120,0.0835138389,0.0563165400,0.0363037629,0.0222058684,
    0.0127521667,0.0067664313,0.0032312239,0.0013212953,0.0004272315
  };
  std::vector<double> sq47 = {
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1
  };
  //window_[WINDOW_HANN31]   = Hann(31);
  //window_[WINDOW_HANN47]   = Hann(47);
  //window_[WINDOW_HANN63]   = Hann(63);
  window_[WINDOW_HANN95]   = Hann(95);
  window_[WINDOW_HANN127]  = Hann(127);
  window_[WINDOW_HANN255]  = Hann(255);
  window_[WINDOW_HANN511]  = Hann(511);
  window_[WINDOW_HANN1023] = Hann(1023);
  window_[WINDOW_HANN2047] = Hann(2047);
  window_[WINDOW_CHEB47]   = cheb47;
  //window_[WINDOW_SQUARE47] = sq47;
  
  fft_inbuf_ = (fftw_complex*) fftw_alloc_complex(sizeof(fftw_complex) * FFT_LEN_BIG);
  if (fft_inbuf_ == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }
  fft_outbuf_ = (fftw_complex*) fftw_alloc_complex(sizeof(fftw_complex) * FFT_LEN_BIG);
  if (fft_outbuf_ == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(fft_inbuf_);
    exit(EXIT_FAILURE);
  }
  memset(fft_inbuf_,  0, sizeof(fftw_complex) * FFT_LEN_BIG);

  fft_plan_small_ = fftw_plan_dft_1d(FFT_LEN_SMALL, fft_inbuf_, fft_outbuf_, FFTW_FORWARD, FFTW_ESTIMATE);
  fft_plan_big_   = fftw_plan_dft_1d(FFT_LEN_BIG, fft_inbuf_, fft_outbuf_, FFTW_FORWARD, FFTW_ESTIMATE);

  cirbuf_tail_ = 0;
  cirbuf_head_ = 0;
  cirbuf_fill_count_ = 0;
  is_open_ = false;
  t_ = 0;

}

void DSPworker::openAudioFile (std::string fname) {

  if (!is_open_) {

    fprintf (stderr,"Open '%s'\n", fname.c_str()) ;
    file_ = SndfileHandle(fname.c_str()) ;

    if (file_.error()) {
      fprintf(stderr,"(sndfile) %s\n", file_.strError());
    } else {
      fprintf (stderr,"    Sample rate : %d\n", file_.samplerate ()) ;
      fprintf (stderr,"    Channels    : %d\n", file_.channels ()) ;

      samplerate_ = file_.samplerate();

      stream_type_ = STREAM_TYPE_FILE;

      is_open_ = true;
      readMore();

      /* RAII takes care of destroying SndfileHandle object. */
    }
  }
}

void DSPworker::openPortAudio () {

  if (!is_open_) {
    Pa_Initialize();

    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency ;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    const PaDeviceInfo *devinfo;
    devinfo = Pa_GetDeviceInfo(inputParameters.device);

    int err = Pa_OpenStream(
              &pa_stream_,
              &inputParameters,
              NULL,
              44100,
              READ_CHUNK_LEN,
              paClipOff,
              NULL, /* no callback, use blocking API */
              NULL ); /* no callback, so no callback userData */

    if (err == paNoError)
      printf("opened %s\n",devinfo->name);
    err = Pa_StartStream( pa_stream_ );
    if (err == paNoError)
      printf("stream started\n");

    const PaStreamInfo *streaminfo;
    streaminfo = Pa_GetStreamInfo(pa_stream_);
    samplerate_ = streaminfo->sampleRate;
    printf("%f\n",samplerate_);

    stream_type_ = STREAM_TYPE_PA;

    is_open_ = true;
    readMore();
  }
}


int DSPworker::freq2bin (double freq, int fft_len) {
  return (freq / samplerate_ * fft_len);
}

bool DSPworker::is_open () {
  return is_open_;
}

double DSPworker::get_t() {
  return t_;
}

void DSPworker::readMore () {
  short read_buffer[READ_CHUNK_LEN];
  sf_count_t samplesread = 0;

  if (is_open_) {
    if (stream_type_ == STREAM_TYPE_FILE) {

      samplesread = file_.read(read_buffer, READ_CHUNK_LEN);
      if (samplesread < READ_CHUNK_LEN)
        is_open_ = false;

    } else if (stream_type_ == STREAM_TYPE_PA) {

      samplesread = READ_CHUNK_LEN;
      int err = Pa_ReadStream( pa_stream_, read_buffer, READ_CHUNK_LEN );
      if (err != paNoError)
        is_open_ = false;
    }
  }

  int cirbuf_fits = std::min(CIRBUF_LEN - cirbuf_head_, (int)samplesread);

  memcpy(&cirbuf_[cirbuf_head_], read_buffer, cirbuf_fits * sizeof(read_buffer[0]));

  // wrapped around
  if (samplesread > cirbuf_fits) {
    memcpy(&cirbuf_[0], &read_buffer[cirbuf_fits], (samplesread - cirbuf_fits) * sizeof(read_buffer[0]));
  }

  // mirror
  memcpy(&cirbuf_[CIRBUF_LEN], &cirbuf_[0], CIRBUF_LEN);
  
  cirbuf_head_ = (cirbuf_head_ + samplesread) % CIRBUF_LEN;
  cirbuf_fill_count_ += samplesread;
  cirbuf_fill_count_ = std::min(cirbuf_fill_count_, CIRBUF_LEN);

}

// move processing window
double DSPworker::forward (unsigned nsamples) {
  for (unsigned i = 0; i < nsamples; i++) {
    cirbuf_tail_ = (cirbuf_tail_ + 1) % CIRBUF_LEN;
    cirbuf_fill_count_ -= 1;
    if (cirbuf_fill_count_ < MOMENT_LEN) {
      readMore();
    }
  }
  double dt = 1.0 * nsamples / samplerate_;
  t_ += dt;
  return dt;
}
double DSPworker::forward () {
  return forward(1);
}
double DSPworker::forward_time(double sec) {
  double start_t = t_;
  while (t_ < start_t + sec)
    forward();
  return t_ - start_t;
}
void DSPworker::forward_to_time(double sec) {
  while (t_ < sec)
    forward();
}


// the current moment, windowed
void DSPworker::windowedMoment (WindowType win_type, fftw_complex *result) {

  //double if_phi = 0;
  for (int i = 0; i < MOMENT_LEN; i++) {

    int win_i = i - MOMENT_LEN/2 + window_[win_type].size()/2 ;

    if (win_i >= 0 && win_i < window_[win_type].size()) {
      double a;
      //fftw_complex mixed;
      a = cirbuf_[cirbuf_tail_ + i] * window_[win_type][win_i];
      
      /*// mix to IF
      mixed[0] = a * cos(if_phi) - a * sin(if_phi);
      mixed[1] = a * cos(if_phi) + a * sin(if_phi);
      if_phi += 2 * M_PI * 10000 / samplerate_;*/

      result[win_i][0] = result[win_i][1] = a;
      //printf("%f\n",a);
      //std::cout<<result[win_i]<<",";
    }
  }
 // exit(0);
  //std::cout<<"\n";

}

double DSPworker::peakFreq (double minf, double maxf, WindowType wintype) {

  int fft_len = (window_[wintype].size() <= FFT_LEN_SMALL ? FFT_LEN_SMALL : FFT_LEN_BIG);

  fftw_complex windowed[window_[wintype].size()];
  double Mag[fft_len/2 + 1];

  windowedMoment(wintype, windowed);
  memset(fft_inbuf_, 0, fft_len * sizeof(windowed[0]));
  memcpy(fft_inbuf_, windowed, window_[wintype].size() * sizeof(windowed[0]));
  fftw_execute(fft_len == FFT_LEN_BIG ? fft_plan_big_ : fft_plan_small_);
  
  int peakBin = 0;
  int lobin = freq2bin(minf, fft_len);
  int hibin = freq2bin(maxf, fft_len);
  for (int i = lobin-1; i <= hibin+1; i++) {
    Mag[i] = complexMag(fft_outbuf_[i]);
    if ( (i >= lobin && i < hibin) &&
         (peakBin == 0 || Mag[i] > Mag[peakBin]))
      peakBin = i;
  }

  // Find the peak frequency by Gaussian interpolation
  //double result = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
  //                         (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
  
  double y1 = Mag[peakBin-1], y2 = Mag[peakBin], y3 = Mag[peakBin+1];
  double result = peakBin + (y3 - y1) / (2 * (2*y2 - y3 - y1));

  // In Hertz
  result = result / fft_len * samplerate_;

  // cheb47 @ 44100 can't resolve <1800 Hz
  if (wintype == WINDOW_CHEB47 && result < 1800)
    result = peakFreq (minf, maxf, WINDOW_HANN95);

  return result;

}

std::vector<double> DSPworker::bandPowerPerHz(std::vector<std::vector<double> > bands) {

  int fft_len = FFT_LEN_BIG;
  WindowType wintype = WINDOW_HANN2047;
  fftw_complex windowed[window_[wintype].size()];

  windowedMoment(wintype, windowed);
  memset(fft_inbuf_, 0, FFT_LEN_BIG * sizeof(fft_inbuf_[0]));
  memcpy(fft_inbuf_, windowed, window_[wintype].size() * sizeof(windowed[0]));
  fftw_execute(fft_len == FFT_LEN_BIG ? fft_plan_big_ : fft_plan_small_);

  std::vector<double> result;
  for (std::vector<double> band : bands) {
    double P = 0;
    double binwidth = 1.0 * samplerate_ / fft_len;
    int nbins = 0;
    for (int i = freq2bin(band[0], fft_len); i <= freq2bin(band[1], fft_len); i++) {
      P += pow(complexMag(fft_outbuf_[i]), 2);
      nbins++;
    }
    P = P/(binwidth*nbins);
    result.push_back(P);
  }
  return result;
}

WindowType DSPworker::bestWindowFor(SSTVMode Mode, double SNR) {
  WindowType WinType;

  double samplesInPixel = 1.0 * samplerate_ * ModeSpec[Mode].tScan / ModeSpec[Mode].ScanPixels;

  if      (SNR >=  23 && Mode != MODE_PD180 && Mode != MODE_SDX)  WinType = WINDOW_CHEB47;
  else if (SNR >=  12)  WinType = WINDOW_HANN95;
  else if (SNR >=   8)  WinType = WINDOW_HANN127;
  else if (SNR >=   5)  WinType = WINDOW_HANN255;
  else if (SNR >=   4)  WinType = WINDOW_HANN511;
  else if (SNR >=  -7)  WinType = WINDOW_HANN1023;
  else                  WinType = WINDOW_HANN2047;

  return WinType;
}
/*WindowType DSPworker::bestWindowFor(SSTVMode Mode) {
  return bestWindowFor(Mode, 20);
}*/

/*void populateDeviceList() {
  int                  card;
  char                *cardname;
  int                  numcards, row;

  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.combo_card), "default");

  numcards = 0;
  card     = -1;
  row      = 0;
  do {
    //snd_card_next(&card);
    if (card != -1) {
      row++;
      //snd_card_get_name(card,&cardname);
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.combo_card), cardname);
      char *dev = g_key_file_get_string(config,"slowrx","device",NULL);
      if (dev == NULL || strcmp(cardname, dev) == 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(gui.combo_card), row);

      numcards++;
    }
  } while (card != -1);

  if (numcards == 0) {
    perror("No sound devices found!\n");
    exit(EXIT_FAILURE);
  }
  
}*/

std::vector<double> Hann (std::size_t winlen) {
  std::vector<double> result(winlen);
  for (std::size_t i=0; i < winlen; i++)
    result[i] = 0.5 * (1 - cos( (2 * M_PI * i) / (winlen)) );
  return result;
}

std::vector<double> Blackmann (std::size_t winlen) {
  std::vector<double> result(winlen);
  for (std::size_t i=0; i < winlen; i++)
    result[i] = 0.42 - 0.5*cos(2*M_PI*i/winlen) - 0.08*cos(4*M_PI*i/winlen);

  return result;
}

std::vector<double> Rect (std::size_t winlen) {
  std::vector<double> result(winlen);
  double sigma = 0.4;
  for (std::size_t i=0; i < winlen; i++)
    result[i] = exp(-0.5*((i-(winlen-1)/2)/(sigma*(winlen-1)/2)));

  return result;
}

std::vector<double> Gauss (std::size_t winlen) {
  std::vector<double> result(winlen);
  for (std::size_t i=0; i < winlen; i++)
    result[i] = 1;

  return result;
}

double complexMag (fftw_complex coeff) {
  return sqrt(pow(coeff[0],2) + pow(coeff[1],2));
}

guint8 freq2lum (double freq) {
  return clip((freq - 1500.0) / (2300.0-1500.0) * 255 + .5);
}

std::vector<double> upsampleLanczos(std::vector<double> orig, int factor) {
  std::vector<double> result(orig.size()*factor);
  int alpha = 4;
  int sinclen = (factor*alpha % 2 == 0 ? factor*alpha+1 : factor*alpha);
  double middle = 1500;

  std::vector<double> sinc(sinclen);
  for (int i=0; i<sinclen; i++) {
    double x1 = 1.0*i/factor - alpha/2;
    double x2 = 2.0*i/sinclen - 1;
    sinc[i] = (x1 == 0 ? 1 : sin(M_PI*x1) / (M_PI*x1)) *
              (x2 == 0 ? 1 : sin(M_PI*x2) / (M_PI*x2));
  }

  for (int i=0; i<orig.size(); i++) {
    if (orig[i] != 0) {
      for (int j=0; j<sinclen; j++) {
        int i_new = i*factor + (j-sinclen/2);
        if (i_new > 0 && i_new < result.size())
          result[i_new] += (orig[i] - middle) * sinc[j];
      }
    }
  }

  for (int i=0; i<result.size(); i++)
    result[i] += middle;
  
  return result;
}
