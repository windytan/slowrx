#include <getopt.h>
#include "common.h"
#include "gui.h"
#include "dsp.h"

int main(int argc, char *argv[]) {

  std::string confpath(std::string(getenv("HOME")) + "/.config/slowrx/slowrx.ini");
  config.load_from_file(confpath);

  int opt_char;
  while ((opt_char = getopt (argc, argv, "t:f:")) != EOF)
    switch (opt_char) {
      case 't':
        return(0);
        break;
      case 'f':
        //dsp.openAudioFile(optarg);
        break;
    }

  //Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "com.windytan.slowrx");

  SlowGUI gui;

  return 0;
}
