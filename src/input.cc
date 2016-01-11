#include "input.h"
#include <thread>

bool g_is_pa_initialized = false;

Input::Input() : m_cirbuf(CIRBUF_LEN*2), m_read_buffer(nullptr),
  m_read_buffer_s16(nullptr), m_t(0) {

  m_cirbuf.head = MOMENT_LEN/2;
}

void Input::openAudioFile (std::string fname) {

  if (m_is_open)
    close();

  fprintf (stderr,"open '%s'\n", fname.c_str()) ;
  m_file = SndfileHandle(fname.c_str()) ;

  if (m_file.error()) {
    fprintf(stderr,"sndfile: %s\n", m_file.strError());
  } else {
    fprintf (stderr,"  opened @ %d Hz, %d ch\n", m_file.samplerate(), m_file.channels()) ;

    m_samplerate = m_file.samplerate();

    m_stream_type = STREAM_TYPE_FILE;

    m_num_chans = m_file.channels();
    delete m_read_buffer;
    m_read_buffer = new float [READ_CHUNK_LEN * m_num_chans];
    m_is_open = true;
  }
}

void Input::openPortAudio (int n) {

  if (m_is_open)
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
            &m_pa_stream,
            &inputParameters,
            NULL,
            44100,
            READ_CHUNK_LEN,
            paClipOff,
            &Input::PaStaticCallback,
            this );

  if (!err) {
    m_num_chans = 1;
    printf("make readbuffer: %d floats\n",READ_CHUNK_LEN * m_num_chans);
    delete m_read_buffer;
    m_read_buffer = new float [READ_CHUNK_LEN * m_num_chans]();

    err = Pa_StartStream( m_pa_stream );

    const PaStreamInfo *streaminfo;
    streaminfo = Pa_GetStreamInfo(m_pa_stream);
    m_samplerate = streaminfo->sampleRate;
    fprintf(stderr,"  opened '%s' @ %.1f\n",devinfo->name,m_samplerate);

    m_stream_type = STREAM_TYPE_PA;

    if (err == paNoError) {
      m_is_open = true;
      //readMore();
    } else {
      fprintf(stderr,"  error at Pa_StartStream\n");
    }
  } else {
    fprintf(stderr,"  error\n");
  }
}

void Input::openStdin() {

  if (m_is_open)
    close();

  printf("openStdin\n");

#ifdef WINDOWS
  int result = _setmode( _fileno( stdin ), _O_BINARY );
  if( result == -1 )
    perror( "Cannot set mode" );
  else
    printf( "'stdin' successfully changed to binary mode\n" );
#endif

  delete m_read_buffer_s16;
  m_read_buffer_s16 = new int16_t [READ_CHUNK_LEN]();

  m_stream_type = STREAM_TYPE_STDIN;
  m_is_open = true;
}

void Input::close () {
  if (m_is_open) {
    if (m_stream_type == STREAM_TYPE_PA) {
      printf("Pa_CloseStream\n");
      Pa_CloseStream(m_pa_stream);
    }
    m_is_open = false;
  }
}


bool Input::isOpen () const {
  return m_is_open;
}

double Input::get_t() const {
  return m_t;
}

double Input::getSamplerate() const {
  return m_samplerate;
}

bool Input::isLive() const {
  return (m_stream_type == STREAM_TYPE_PA);
}

int Input::PaCallback(const void *input, void *output,
    unsigned long framesread,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags) {

  //printf("PaCallback\n");
  float* in = (float*)input;

  {
    std::lock_guard<std::mutex> guard(m_buffer_mutex);
    for (unsigned i = 0; i<READ_CHUNK_LEN; i++)
      m_read_buffer[i] = in[i * m_num_chans];
  }

  readBufferTransfer(framesread);

  return 0;
}

void Input::readMoreFromFile() {
  unsigned long framesread = 0;
  sf_count_t fr = m_file.readf(m_read_buffer, READ_CHUNK_LEN);
  if (fr < READ_CHUNK_LEN) {
    m_is_open = false;
    if (fr < 0) {
      framesread = 0;
    } else {
      framesread = fr;
    }
  } else {
    framesread = fr;
  }

  if (m_num_chans > 1) {
    for (int i=0; i<READ_CHUNK_LEN; i++) {
      m_read_buffer[i] = m_read_buffer[i*m_num_chans];
    }
  }

  readBufferTransfer(framesread);
}

void Input::readMoreFromStdin() {
  unsigned long framesread = 0;
  ssize_t fr = fread(m_read_buffer_s16, sizeof(uint16_t), READ_CHUNK_LEN, stdin);
  if (fr < READ_CHUNK_LEN) {
    m_is_open = false;
  }
  framesread = fr;

  for (int i=0; i<READ_CHUNK_LEN; i++) {
    m_read_buffer.at(i) = m_read_buffer_s16.at(i) / 32768.0;
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
  const PaDeviceInfo *device_info;
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
    std::lock_guard<std::mutex> guard(m_buffer_mutex);

    int cirbuf_fits = std::min(CIRBUF_LEN - m_cirbuf.head, int(framesread));

    for (int i=0; i<cirbuf_fits; i++)
      m_cirbuf.data[m_cirbuf.head + i] = m_read_buffer[i];

    // wrap around
    if (framesread > cirbuf_fits) {
      for (size_t i=0; i<(framesread - cirbuf_fits); i++)
        m_cirbuf.data[i] = m_read_buffer[cirbuf_fits + i];
    }

    // mirror
    for (size_t i=0; i<CIRBUF_LEN; i++)
      m_cirbuf.data[CIRBUF_LEN + i] = m_cirbuf.data[i];

    m_cirbuf.head = (m_cirbuf.head + framesread) % CIRBUF_LEN;
    m_cirbuf.fill_count += framesread;
    m_cirbuf.fill_count = std::min(int(m_cirbuf.fill_count), CIRBUF_LEN);
  }
}

// move processing window
double Input::forward (unsigned nsamples) {
  for (unsigned i = 0; i < nsamples; i++) {

    {
      std::lock_guard<std::mutex> guard(m_buffer_mutex);
      m_cirbuf.tail = (m_cirbuf.tail + 1) % CIRBUF_LEN;
      m_cirbuf.fill_count -= 1;
    }

    if (m_cirbuf.fill_count < MOMENT_LEN) {
      while (m_cirbuf.fill_count < MOMENT_LEN) {
        if (m_stream_type == STREAM_TYPE_PA) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else if (m_stream_type == STREAM_TYPE_FILE) {
          readMoreFromFile();
        } else if (m_stream_type == STREAM_TYPE_STDIN) {
          readMoreFromStdin();
        }
      }
    }
  }
  double dt = 1.0 * nsamples / m_samplerate;
  m_t += dt;
  return dt;
}
double Input::forward () {
  return forward(1);
}
double Input::forwardTime(double sec) {
  double start_t = m_t;
  while (m_t < start_t + sec)
    forward();
  return m_t - start_t;
}

