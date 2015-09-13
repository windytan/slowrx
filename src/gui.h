#ifndef GUI_H_
#define GUI_H_

#include <gtkmm.h>
#include "common.h"
#include "listener.h"

class SlowGUI {

  public:

    SlowGUI();

    void redrawNotify();
    void onRedrawNotify();
    void resyncNotify();
    void onResyncNotify();

  private:

    Gtk::Button *button_abort;
    Gtk::Button *button_browse;
    Gtk::Button *button_clear;
    Gtk::Button *button_start;
    Gtk::ComboBoxText *combo_card;
    Gtk::ComboBox *combo_mode;
    Gtk::Entry *entry_picdir;
    Gtk::EventBox *eventbox_img;
    Gtk::Frame *frame_manual;
    Gtk::Frame *frame_slant;
    Gtk::Grid *grid_vu;
    Gtk::IconView *iconview;
    Gtk::Image *image_devstatus;
    Gtk::Image *image_pwr;
    Gtk::Image *image_rx;
    Gtk::Image *image_snr;
    Gtk::Label *label_fskid;
    Gtk::Label *label_lastmode;
    Gtk::Label *label_utc;
    Gtk::MenuItem *menuitem_about;
    Gtk::MenuItem *menuitem_quit;
    Gtk::SpinButton *spin_shift;
    Gtk::Widget *statusbar;
    Gtk::ToggleButton *tog_adapt;
    Gtk::ToggleButton *tog_fsk;
    Gtk::ToggleButton *tog_rx;
    Gtk::ToggleButton *tog_save;
    Gtk::ToggleButton *tog_setedge;
    Gtk::ToggleButton *tog_slant;
    Gtk::Window *window_about;
    Gtk::Window *window_main;

    Glib::Dispatcher redraw_dispatcher_;
    Glib::Dispatcher resync_dispatcher_;
    Glib::Threads::Thread* worker_thread_;
    Listener worker_;
};

extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_PWR;
extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_SNR;
extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_disp;

extern Gtk::ListStore *savedstore;

extern Glib::KeyFile config;

void     evt_chooseDir     ();
void     evt_clickimg      (Gtk::Widget*, GdkEventButton*, Gdk::WindowEdge);
void     evt_deletewindow  ();
void     evt_ManualStart   ();
void     evt_show_about    ();

#endif // GUI_H
