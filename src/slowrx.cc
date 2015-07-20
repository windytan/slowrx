#include <gtkmm.h>
#include <fftw3.h>
#include "common.h"

int startGui (int, char**);

int main(int argc, char *argv[]) {
  return startGui(argc, argv);
}

int startGui(int argc, char *argv[]) {
  Glib::RefPtr<Gtk::Application> app =
    Gtk::Application::create(argc, argv,
      "com.windytan.slowrx");

  Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create_from_file("ui/slowrx.ui");
  Gtk::Window *pWindow;
  builder->get_widget("window_main", pWindow);

  return app->run(*pWindow);
}
