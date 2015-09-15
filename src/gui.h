#ifndef GUI_H_
#define GUI_H_

#include <gtkmm.h>
#include "common.h"
#include "listener.h"

class GUI {

  public:

    GUI();

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
    bool isSaveEnabled();

    void redrawNotify();
    void onRedrawNotify();
    void resyncNotify();
    void onResyncNotify();

    void fetchAutoState();
    void autoSettingsChanged(Gtk::StateFlags);

    void inputDeviceChanged();
    void audioFileSelected();

    eStreamType getSelectedStreamType();
    int getSelectedPaDevice();
    std::string getSelectedAudioFileName();
    std::string getSavedPictureLocation();

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
    Gtk::CheckButton *check_save_;
    Gtk::Button *button_abort_;
    Gtk::Button *button_clear_;
    Gtk::Button *button_manualstart_;
    Gtk::ComboBoxText *combo_manualmode_;
    Gtk::ComboBoxText *combo_portaudio_;
    Gtk::RadioButton *radio_input_portaudio_;
    Gtk::RadioButton *radio_input_file_;
    Gtk::RadioButton *radio_input_stdin_;
    Gtk::FileChooserButton *button_audiofilechooser_;
    Gtk::FileChooserButton *button_savedirchooser_;
    Gtk::Frame *frame_input_;

    Glib::Dispatcher redraw_dispatcher_;
    Glib::Dispatcher resync_dispatcher_;
    Glib::Threads::Thread* listener_worker_thread_;
    Listener listener_worker_;
};

extern Gtk::ListStore *savedstore;

extern Glib::KeyFile config;

#endif // GUI_H
