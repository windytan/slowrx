#include "common.hh"


int main(int argc, char *argv[]) {
  string confpath(string(getenv("HOME")) + "/.config/slowrx/slowrx.ini");
  config.load_from_file(confpath);

  DSPworker dsp;

  GetVideo(GetVIS(&dsp), 44100, &dsp, false);

  //SlowGUI gui = SlowGUI();
  return 0;
}


