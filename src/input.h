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

    void   readBufferTransfer (int);
    double forward (unsigned);
    double forward ();
    double forwardTime (double);
    double get_t     () const;
    double getSamplerate () const;


    bool   isOpen   () const;
    bool   isLive    () const;

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

    CirBuffer     cirbuf_;

  private:

    float*        read_buffer_;
    int16_t*      read_buffer_s16_;
    SndfileHandle file_;
    PaStream*     pa_stream_;
    eStreamType   stream_type_;
    bool          is_open_;
    int           num_chans_;
    double        samplerate_;
    double        t_;
    std::mutex    buffer_mutex_;

};

std::vector<std::pair<int,std::string>> listPortaudioDevices();
int getDefaultPaDevice();

#endif
