#ifndef GUI_H_
#define GUI_H_

#include <gtkmm.h>
#include "common.h"
#include "listener.h"

class SlowGUI {

  public:

    SlowGUI();

    void start();

    void imageReset();

    void abortedByUser();
    void ackAbortedByUser();
    void receiving();
    void notReceiving();

    bool isRxEnabled();
    bool isDenoiseEnabled();
    bool isSyncEnabled();
    bool isAbortedByUser();

    void redrawNotify();
    void onRedrawNotify();
    void resyncNotify();
    void onResyncNotify();

    void fetchAutoState();
    void autoChanged(Gtk::StateFlags);

    void inputDeviceChanged();
    void audioFileSelected();

    eStreamType getSelectedStreamType();
    int getSelectedPaDevice();
    std::string getSelectedAudioFileName();

  private:

    bool is_rx_enabled_;
    bool is_sync_enabled_;
    bool is_fskid_enabled_;
    bool is_denoise_enabled_;
    bool is_aborted_by_user_;

    Glib::RefPtr<Gtk::Application> app_;

    Gtk::Label  *label_lasttime_;
    Gtk::Window *window_about_;
    Gtk::Window *window_main_;
    Gtk::Image  *image_rx_;
    Gtk::ToggleButton *switch_rx_;
    Gtk::ToggleButton *switch_sync_;
    Gtk::ToggleButton *switch_denoise_;
    Gtk::ToggleButton *switch_fskid_;
    Gtk::Button *button_abort_;
    Gtk::Button *button_clear_;
    Gtk::Button *button_manualstart_;
    Gtk::ComboBoxText *combo_manualmode_;
    Gtk::ComboBoxText *combo_portaudio_;
    Gtk::RadioButton *radio_input_portaudio_;
    Gtk::RadioButton *radio_input_file_;
    Gtk::RadioButton *radio_input_stdin_;
    Gtk::FileChooserButton *button_audiofilechooser_;
    Gtk::Frame *frame_input_;

    Glib::Dispatcher redraw_dispatcher_;
    Glib::Dispatcher resync_dispatcher_;
    Glib::Threads::Thread* listener_worker_thread_;
    Listener listener_worker_;
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
