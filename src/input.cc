#include <thread>
#include "input.h"
#include "dsp.h"

bool g_is_pa_initialized = false;

Input::Input() : m_is_open(false), m_t(0), m_if_freq(1900.0), m_if_ph(0.0) {
  m_cirbuf = std::make_shared<CirBuffer<std::complex<double>>>(kReadChunkLen * 3);

  m_decimator_lpf_coeffs =
    {0.000356,  0.000657,  0.000754,  0.000299, -0.001054, -0.003469, -0.006727,
    -0.010087, -0.012264, -0.011603, -0.006419,  0.004536,  0.021609,  0.043929,
     0.069344,  0.094687,  0.116329,  0.130909,  0.136054,  0.130909,  0.116329,
     0.094687,  0.069344,  0.043929,  0.021609,  0.004536, -0.006419, -0.011603,
    -0.012264, -0.010087, -0.006727, -0.003469, -0.001054,  0.000299,  0.000754,
     0.000657,  0.000356};

}

void Input::openAudioFile (std::string fname) {

  close();

  printf("open file\n=========\n");
  std::cout << "│  " << fname << "\n";
  m_file = SndfileHandle(fname.c_str()) ;

  if (m_file.error()) {
    fprintf(stderr,"sndfile: %s\n", m_file.strError());
  } else {
    printf("│  %d Hz\n│  %d channels\n", m_file.samplerate(), m_file.channels()) ;

    m_samplerate = m_file.samplerate();

    m_stream_type = STREAM_TYPE_FILE;

    m_num_chans = m_file.channels();
    m_read_buffer = std::vector<float>(kReadChunkLen * m_num_chans);
    m_is_open = true;
  }
  printf("└──\n");
}

void Input::openPortAudio (int n) {

  close();

  printf("open PA\n=======\n");

  if (!g_is_pa_initialized) {
    printf("│  Pa_Initialize\n");
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

  printf("│  Pa_OpenStream\n");
  int err = Pa_OpenStream(
            &m_pa_stream,
            &inputParameters,
            NULL,
            44100,
            kReadChunkLen,
            paClipOff,
            &Input::PaStaticCallback,
            this );

  if (!err) {
    m_num_chans = 1;
    m_read_buffer = std::vector<float>(kReadChunkLen * m_num_chans);

    err = Pa_StartStream( m_pa_stream );

    const PaStreamInfo *streaminfo;
    streaminfo = Pa_GetStreamInfo(m_pa_stream);
    m_samplerate = streaminfo->sampleRate;
    printf("|  opened '%s' @ %.1f\n",devinfo->name,m_samplerate);

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
  printf("└──\n");
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

  m_read_buffer_s16 = std::vector<int16_t>(kReadChunkLen);

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

  std::lock_guard<std::mutex> guard(m_buffer_mutex);

  //printf("PaCallback\n");
  float* in = (float*)input;


  for (int i = 0; i<READ_CHUNK_LEN; i++)
    m_read_buffer[i] = in[i * m_num_chans];

  m_cirbuf.append(m_read_buffer, framesread);


  return 0;
}

void Input::readMoreFromFile() {

  std::lock_guard<std::mutex> guard(m_buffer_mutex);

  int framesread = 0;
  sf_count_t fr = m_file.readf(m_read_buffer.data(), kReadChunkLen);
  if (fr < kReadChunkLen) {
    printf("[input] end of file\n");
    m_is_open = false;
    if (fr < 0) {
      framesread = 0;
    } else {
      framesread = fr;
    }
  } else {
    framesread = fr;
  }

  {
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_num_chans > 1) {
      for (int i=0; i<kReadChunkLen; i++) {
        m_read_buffer[i] = m_read_buffer[i*m_num_chans];
      }
    }

  m_cirbuf.append(m_read_buffer, framesread);

}

void Input::readMoreFromStdin() {

  std::lock_guard<std::mutex> guard(m_buffer_mutex);

  int framesread = 0;
  ssize_t fr = fread(m_read_buffer_s16.data(), sizeof(int16_t), kReadChunkLen, stdin);
  if (fr < kReadChunkLen) {
    m_is_open = false;
  }
  framesread = fr;

  {
    std::lock_guard<std::mutex> guard(m_mutex);
    for (int i=0; i<kReadChunkLen; i++) {
      m_read_buffer.at(i) = m_read_buffer_s16.at(i) / 32768.0;
    }

  m_cirbuf.append(m_read_buffer, framesread);
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

// move processing window
double Input::forward (int nsamples) {

  for (int i = 0; i < nsamples; i++) {

    {
      std::lock_guard<std::mutex> guard(m_buffer_mutex);
      m_cirbuf.forward(1);
    }

    while (m_cirbuf->getFillCount() + kReadChunkLen <= m_cirbuf->size()) {
      if (m_stream_type == STREAM_TYPE_PA) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      } else if (m_stream_type == STREAM_TYPE_FILE) {
        readMoreFromFile();
      } else if (m_stream_type == STREAM_TYPE_STDIN) {
        readMoreFromStdin();
      }

      if (!m_is_open)
        break;
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

