#include <getopt.h>
#include "common.hh"


int main(int argc, char *argv[]) {

  std::string confpath(std::string(getenv("HOME")) + "/.config/slowrx/slowrx.ini");
  config.load_from_file(confpath);

  DSPworker dsp;
  
  int opt_char;
  while ((opt_char = getopt (argc, argv, "t:f:")) != EOF)
    switch (opt_char) {
      case 't':
        runTest(optarg);
        return(0);
        break;
      case 'f':
        dsp.openAudioFile(optarg);
        break;
    }

  upsampleLanczos({0},10);


  if (!dsp.is_open())
    dsp.openPortAudio();
  
  GetVideo(GetVIS(&dsp), &dsp);

  //SlowGUI gui = SlowGUI();
  return 0;
}


