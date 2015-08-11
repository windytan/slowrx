#include "common.hh"


int main(int argc, char *argv[]) {
  std::string confpath(std::string(getenv("HOME")) + "/.config/slowrx/slowrx.ini");
  config.load_from_file(confpath);

  DSPworker dsp;

  //dsp.openAudioFile("/Users/windy/Audio/sig/sstv/scottie2-01-noiseonly.wav");
  dsp.openPortAudio();
  GetVideo(GetVIS(&dsp), &dsp);

  //SlowGUI gui = SlowGUI();
  return 0;
}


