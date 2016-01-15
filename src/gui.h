#ifndef GUI_H_
#define GUI_H_

#include "common.h"
#include "listener.h"
#include <gtkmm.h>

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

    bool m_is_rx_enabled;
    bool m_is_sync_enabled;
    bool m_is_fskid_enabled;
    bool m_is_denoise_enabled;
    bool m_is_aborted_by_user;

    Glib::RefPtr<Gtk::Application> m_app;

    Gtk::Label  *m_label_lasttime;
    Gtk::Window *m_window_about;
    Gtk::Window *m_window_main;
    Gtk::Image  *m_image_rx;
    Gtk::Image  *m_image_sync;
    Gtk::ToggleButton *m_switch_rx;
    Gtk::ToggleButton *m_switch_sync;
    Gtk::ToggleButton *m_switch_denoise;
    Gtk::ToggleButton *m_switch_fskid;
    Gtk::CheckButton *m_check_save;
    Gtk::Button *m_button_abort;
    Gtk::Button *m_button_clear;
    Gtk::Button *m_button_manualstart;
    Gtk::ComboBoxText *m_combo_manualmode;
    Gtk::ComboBoxText *m_combo_portaudio;
    Gtk::RadioButton *m_radio_input_portaudio;
    Gtk::RadioButton *m_radio_input_file;
    Gtk::RadioButton *m_radio_input_stdin;
    Gtk::FileChooserButton *m_button_audiofilechooser;
    Gtk::FileChooserButton *m_button_savedirchooser;
    Gtk::Box *m_box_input;

    Glib::Dispatcher m_dispatcher_redraw;
    Glib::Dispatcher m_dispatcher_resync;
    Glib::Threads::Thread* m_thr_listener_worker;
    Listener m_listener;
};

extern Gtk::ListStore *savedstore;

extern Glib::KeyFile config;

#endif // GUI_H
