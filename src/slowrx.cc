#include "common.hh"


int main(int argc, char *argv[]) {
  string confpath(string(getenv("HOME")) + "/.config/slowrx/slowrx.ini");
  config.load_from_file(confpath);

  PCMworker worker;

  SlowGUI gui = SlowGUI();
  return 0;
}


