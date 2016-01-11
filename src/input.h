#ifndef INPUT_H_
#define INPUT_H_

#define READ_CHUNK_LEN 4096
#define MOMENT_LEN     2047
#define CIRBUF_LEN_FACTOR 8
#define CIRBUF_LEN ((MOMENT_LEN+1)*CIRBUF_LEN_FACTOR)

#include "common.h"
#include "portaudio.h"
#include <sndfile.hh>

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

    CirBuffer<float>     m_cirbuf;

  private:

    float*   m_read_buffer;
    int16_t* m_read_buffer_s16;
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
