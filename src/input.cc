#include "input.h"
#include <thread>

bool g_is_pa_initialized = false;

Input::Input() : cirbuf_(CIRBUF_LEN*2), read_buffer_(nullptr), read_buffer_s16_(nullptr), t_(0) {

  cirbuf_.head = MOMENT_LEN/2;
}

void Input::openAudioFile (std::string fname) {

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
    delete read_buffer_;
    read_buffer_ = new float [READ_CHUNK_LEN * num_chans_];
    is_open_ = true;
  }
}

void Input::openPortAudio (int n) {

  if (is_open_)
    close();

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
            &Input::PaStaticCallback,
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

void Input::openStdin() {

  if (is_open_)
    close();

  printf("openStdin\n");

#ifdef WINDOWS
  int result = _setmode( _fileno( stdin ), _O_BINARY );
  if( result == -1 )
    perror( "Cannot set mode" );
  else
    printf( "'stdin' successfully changed to binary mode\n" );
#endif

  delete read_buffer_s16_;
  read_buffer_s16_ = new int16_t [READ_CHUNK_LEN]();
  
  stream_type_ = STREAM_TYPE_STDIN;
  is_open_ = true;
}

void Input::close () {
  if (is_open_) {
    if (stream_type_ == STREAM_TYPE_PA) {
      printf("Pa_CloseStream\n");
      Pa_CloseStream(pa_stream_);
    }
    is_open_ = false;
  }
}


bool Input::isOpen () const {
  return is_open_;
}

double Input::get_t() const {
  return t_;
}

double Input::getSamplerate() const {
  return samplerate_;
}

bool Input::isLive() const {
  return (stream_type_ == STREAM_TYPE_PA);
}

int Input::PaCallback(const void *input, void *output,
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

void Input::readMoreFromFile() {
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

void Input::readMoreFromStdin() {
  unsigned long framesread = 0;
  ssize_t fr = fread(read_buffer_s16_, sizeof(uint16_t), READ_CHUNK_LEN, stdin);
  if (fr < READ_CHUNK_LEN) {
    is_open_ = false;
  }
  framesread = fr;

  for (size_t i=0; i<READ_CHUNK_LEN; i++) {
    read_buffer_[i] = read_buffer_s16_[i] / 32768.0;
  }

  readBufferTransfer(framesread);
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

void Input::readBufferTransfer (int framesread) {

  {
    std::lock_guard<std::mutex> guard(buffer_mutex_);

    int cirbuf_fits = std::min(CIRBUF_LEN - cirbuf_.head, int(framesread));

    for (int i=0; i<cirbuf_fits; i++)
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
double Input::forward (unsigned nsamples) {
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
        } else if (stream_type_ == STREAM_TYPE_STDIN) {
          readMoreFromStdin();
        }
      }
    }
  }
  double dt = 1.0 * nsamples / samplerate_;
  t_ += dt;
  return dt;
}
double Input::forward () {
  return forward(1);
}
double Input::forwardTime(double sec) {
  double start_t = t_;
  while (t_ < start_t + sec)
    forward();
  return t_ - start_t;
}

