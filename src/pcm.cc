#include "common.hh"

DSPworker::DSPworker() : Mutex(), please_stop_(false) {

  fft_len_ = 2048;

  for (int j = 0; j < 7; j++)
    for (int i = 0; i < win_lens_[j]; i++)
      window_[j][i] = 0.5 * (1 - cos( (2 * M_PI * i) / (win_lens_[j] - 1)) );

  vector<double> cheb = {
    0.0004272315,0.0013212953,0.0032312239,0.0067664313,0.0127521667,0.0222058684,
    0.0363037629,0.0563165400,0.0835138389,0.1190416120,0.1637810511,0.2182020094,
    0.2822270091,0.3551233730,0.4354402894,0.5210045495,0.6089834347,0.6960162864,
    0.7784084484,0.8523735326,0.9143033652,0.9610404797,0.9901263448,1.0000000000,
    0.9901263448,0.9610404797,0.9143033652,0.8523735326,0.7784084484,0.6960162864,
    0.6089834347,0.5210045495,0.4354402894,0.3551233730,0.2822270091,0.2182020094,
    0.1637810511,0.1190416120,0.0835138389,0.0563165400,0.0363037629,0.0222058684,
    0.0127521667,0.0067664313,0.0032312239,0.0013212953,0.0004272315
  };

  for (int i = 0; i < win_lens_[WINDOW_CHEB47]; i++)
    window_[WINDOW_CHEB47][i] = cheb[i];
  
  fft_inbuf_ = (double*) fftw_alloc_real(sizeof(double) * fft_len_);
  if (fft_inbuf_ == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }
  fft_outbuf_ = (fftw_complex*) fftw_alloc_complex(sizeof(fftw_complex) * (fft_len_));
  if (fft_outbuf_ == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(fft_inbuf_);
    exit(EXIT_FAILURE);
  }
  memset(fft_inbuf_,  0, sizeof(double) * fft_len_);

  fft_plan_ = fftw_plan_dft_r2c_1d(fft_len_, fft_inbuf_, fft_outbuf_, FFTW_ESTIMATE);

  cirbuf_tail_ = 0;
  cirbuf_head_ = 0;
  cirbuf_fill_count_ = 0;
  is_still_listening_ = true;

  printf("DSPworker created\n");

  //open_audio_file("/Users/windy/Audio/sig/sstv/robot72-02.wav");
  open_audio_file("/Users/windy/Movies/sstv-iss.wav");
  //open_audio_file("/Users/windy/Audio/sig/1000Hz-800Hz.wav");
}

void DSPworker::open_audio_file (string fname) {

  file_ = SndfileHandle(fname.c_str()) ;

  printf ("Opened file '%s'\n", fname.c_str()) ;
  printf ("    Sample rate : %d\n", file_.samplerate ()) ;
  printf ("    Channels    : %d\n", file_.channels ()) ;

  samplerate_ = file_.samplerate();

  //puts ("") ;

  /* RAII takes care of destroying SndfileHandle object. */
}

int DSPworker::GetBin (double freq) {
  return (freq / samplerate_ * fft_len_);
}

double DSPworker::FourierPower (fftw_complex coeff) {
  return pow(coeff[0],2) + pow(coeff[1],2);
}

bool DSPworker::is_still_listening () {
  return is_still_listening_;
}


void DSPworker::read_more () {
  short read_buffer[READ_CHUNK_LEN];

  sf_count_t samplesread = file_.read(read_buffer, READ_CHUNK_LEN);
  if (samplesread < READ_CHUNK_LEN)
    is_still_listening_ = false;

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
      read_more();
    }
  }
  return (1.0 * nsamples / samplerate_);
}
double DSPworker::forward() {
  return forward(1);
}
double DSPworker::forward_ms(double ms) {
  return forward(ms / 1000 * samplerate_);
}

// the current moment, windowed
void DSPworker::get_windowed_moment (WindowType win_type, double *result) {
  for (int i = 0; i < MOMENT_LEN; i++) {

    int win_i = i - MOMENT_LEN/2 + win_lens_[win_type]/2 ;

    if (win_i >= 0 && win_i < win_lens_[win_type]) {
      result[win_i] = cirbuf_[cirbuf_tail_ + i] * window_[win_type][win_i];
    }
  }

}

double DSPworker::get_peak_freq (double minf, double maxf, WindowType wintype) {

  double windowed[win_lens_[wintype]];
  double Power[fft_len_];

  get_windowed_moment(wintype, windowed);
  //for (int i=0;i<win_lens_[wintype];i++)
  //  printf("g %f\n",windowed[i]);
  memset (fft_inbuf_, 0, fft_len_ * sizeof(double));
  memcpy(fft_inbuf_, windowed, win_lens_[wintype] * sizeof(double));
  fftw_execute(fft_plan_);
  // Find the bin with most power
  int MaxBin = 0;
  for (int i = GetBin(minf)-1; i <= GetBin(maxf)+1; i++) {
    Power[i] = FourierPower(fft_outbuf_[i]);
    if ( (i >= GetBin(minf) && i < GetBin(maxf)) &&
         (MaxBin == 0 || Power[i] > Power[MaxBin]))
      MaxBin = i;
  }

  // Find the peak frequency by Gaussian interpolation
  double result = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                           (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));

  // In Hertz
  result = result / fft_len_ * samplerate_;

  return result;

}

/*
// Capture fresh PCM data to buffer
void readPcm(int numsamples) {

  int    samplesread, i;
  gint32 tmp[BUFLEN];  // Holds one or two 16-bit channels, will be ANDed to single channel

  //samplesread = snd_pcm_readi(pcm.handle, tmp, (pcm.WindowPtr == 0 ? BUFLEN : numsamples));

  if (samplesread < numsamples) {
    
    if      (samplesread == -EPIPE)
      printf("ALSA: buffer overrun\n");
    else if (samplesread < 0) {
      //printf("ALSA error %d (%s)\n", samplesread, snd_strerror(samplesread));
      gtk_widget_set_tooltip_text(gui.image_devstatus, "ALSA error");
      Abort = TRUE;
      pthread_exit(NULL);
    }
    else
      printf("Can't read %d samples\n", numsamples);
    
    // On first appearance of error, update the status icon
    if (!pcm.BufferDrop) {
      gtk_image_set_from_stock(GTK_IMAGE(gui.image_devstatus),GTK_STOCK_DIALOG_WARNING,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(gui.image_devstatus, "Device is dropping samples");
      pcm.BufferDrop = TRUE;
    }

  }

  if (pcm.WindowPtr == 0) {
    // Fill buffer on first run
    for (i=0; i<BUFLEN; i++)
      pcm.Buffer[i] = tmp[i] & 0xffff;
    pcm.WindowPtr = BUFLEN/2;
  } else {

    // Move buffer and push samples
    for (i=0; i<BUFLEN-numsamples;      i++) pcm.Buffer[i] = pcm.Buffer[i+numsamples];
    for (i=BUFLEN-numsamples; i<BUFLEN; i++) pcm.Buffer[i] = tmp[i-(BUFLEN-numsamples)] & 0xffff;

    pcm.WindowPtr -= numsamples;
  }

}

void populateDeviceList() {
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

// Initialize sound card
// Return value:
//   0 = opened ok
//  -1 = opened, but suboptimal
//  -2 = couldn't be opened
int initPcmDevice(string wanted_dev_name) {

  //snd_pcm_hw_params_t *hwparams;
  void         *hwparams;
  char          pcm_name[30];
  unsigned int  exact_rate = 44100;
  int           card;
  bool          found;
  char         *cardname;

  //pcm.BufferDrop = false;

  //snd_pcm_hw_params_alloca(&hwparams);

  card  = -1;
  found = false;
  if (wanted_dev_name.compare("default") == 0) {
    found=true;
  } else {
    do {
      //snd_card_next(&card);
      if (card != -1) {
        //snd_card_get_name(card,&cardname);
        if (strcmp(cardname, wanted_dev_name.c_str()) == 0) {
          found=true;
          break;
        }
      }
    } while (card != -1);
  }

  if (!found) {
    perror("Device disconnected?\n");
    return(-2);
  }

  if (wanted_dev_name.compare("default") == 0) {
    sprintf(pcm_name,"default");
  } else {
    sprintf(pcm_name,"hw:%d",card);
  }
  
  /*if (snd_pcm_open(&pcm.handle, pcm_name, SND_PCM_STREAM_CAPTURE, 0) < 0) {
    perror("ALSA: Error opening PCM device");
    return(-2);
  }*/

  /* Init hwparams with full configuration space */
  /*if (snd_pcm_hw_params_any(pcm.handle, hwparams) < 0) {
    perror("ALSA: Can not configure this PCM device.");
    return(-2);
  }

  if (snd_pcm_hw_params_set_access(pcm.handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    perror("ALSA: Error setting interleaved access.");
    return(-2);
  }
  if (snd_pcm_hw_params_set_format(pcm.handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    perror("ALSA: Error setting format S16_LE.");
    return(-2);
  }
  if (snd_pcm_hw_params_set_rate_near(pcm.handle, hwparams, &exact_rate, 0) < 0) {
    perror("ALSA: Error setting sample rate.");
    return(-2);
  }*/

  // Try stereo first
  /*if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 2) < 0) {
    // Fall back to mono
    if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 1) < 0) {
      perror("ALSA: Error setting channels.");
      return(-2);
    }
  }
  if (snd_pcm_hw_params(pcm.handle, hwparams) < 0) {
    perror("ALSA: Error setting HW params.");
    return(-2);
  }*/

  /*pcm.Buffer = calloc( BUFLEN, sizeof(gint16));
  memset(pcm.Buffer, 0, BUFLEN);*/
  
  if (exact_rate != 44100) {
    fprintf(stderr, "ALSA: Got %d Hz instead of 44100. Expect artifacts.\n", exact_rate);
    return(-1);
  }

  return(0);

}
