#ifndef INPUT_H_
#define INPUT_H_

#include "portaudio.h"
#include <sndfile.hh>
#include "common.h"

constexpr int kReadChunkLen = 4096;

class Input {
  public:

    Input();

    void   openAudioFile (std::string);
    void   openPortAudio (int);
    void   openStdin();
    void   close ();
    void   readMoreFromFile();
    void   readMoreFromStdin();

    double forward (int);
    double forward ();
    double forwardTime (double);
    double get_t     () const;
    double getSamplerate () const;

    bool   isOpen   () const;
    bool   isLive    () const;

    std::mutex    m_mutex;

    int PaCallback(const void *input, void *output,
      unsigned long frameCount,
      const PaStreamCallbackTimeInfo* timeInfo,
      PaStreamCallbackFlags statusFlags);
    static int PaStaticCallback(
      const void *input, void *output,
      unsigned long frameCount,
      const PaStreamCallbackTimeInfo* timeInfo,
      PaStreamCallbackFlags statusFlags,
      void *userData ) {
        return ((Input*)userData)
          ->PaCallback(input, output, frameCount, timeInfo, statusFlags);
      }

    std::shared_ptr<CirBuffer<std::complex<double>>>     m_cirbuf;

  private:

    std::vector<float> m_read_buffer;
    std::vector<int16_t> m_read_buffer_s16;
    std::vector<std::complex<double>> m_mixed_buffer;
    SndfileHandle m_file;
    PaStream*     m_pa_stream;
    eStreamType   m_stream_type;
    bool          m_is_open;
    int           m_num_chans;
    double        m_samplerate;
    double        m_t;
    std::mutex    m_buffer_mutex;

};

std::vector<std::pair<int,std::string>> listPortaudioDevices();
int getDefaultPaDevice();

#endif
