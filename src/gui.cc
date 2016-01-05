#include "gui.h"
#include "listener.h"
#include "picture.h"
#include "input.h"

Glib::RefPtr<Gdk::Pixbuf> empty_pixbuf(int px_width) {
  int px_height = round(px_width / 1.25);
  Glib::RefPtr<Gdk::Pixbuf> pixbuf =
    Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, px_width, px_height);
  pixbuf->fill(0x000000ff);

  return pixbuf;
}

GUI::GUI() : m_is_aborted_by_user(false), redraw_dispatcher_(), resync_dispatcher_(),
  listener_worker_thread_(nullptr), listener_worker_() {
  m_app = Gtk::Application::create("com.windytan.slowrx");

  Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
  builder->add_from_file("ui/slowrx.ui");
  builder->add_from_file("ui/aboutdialog.ui");

  //savedstore = iconview.get_model();

  builder->get_widget("label_lasttime",  label_lasttime_);
  builder->get_widget("window_about",    window_about_);
  builder->get_widget("window_main",     window_main_);
  builder->get_widget("image_rx",        image_rx_);
  builder->get_widget("button_abort",    button_abort_);
  builder->get_widget("button_clear",    button_clear_);
  builder->get_widget("button_manualstart",    button_manualstart_);
  builder->get_widget("combo_manualmode",    combo_manualmode_);
  builder->get_widget("combo_portaudio",    combo_portaudio_);

  builder->get_widget("switch_rx",        switch_rx_);
  builder->get_widget("switch_sync",        switch_sync_);
  builder->get_widget("switch_denoise",        switch_denoise_);
  builder->get_widget("switch_fskid",        switch_fskid_);

  builder->get_widget("check_save",        check_save_);

  builder->get_widget("radio_input_portaudio",        radio_input_portaudio_);
  builder->get_widget("radio_input_file",        radio_input_file_);
  builder->get_widget("radio_input_stdin",       radio_input_stdin_);

  builder->get_widget("button_audiofilechooser",       button_audiofilechooser_);

  builder->get_widget("save_dir_chooser",       button_savedirchooser_);

  builder->get_widget("box_input",       box_input_);

  imageReset();

  /*if (config.get_string("slowrx","rxdir") != NULL) {
    entry_picdir->set_text(config.get_string("slowrx","rxdir"));
  } else {
    config.set_string("slowrx","rxdir",g_get_home_dir());
    entry_picdir->set_text(config.get_string("slowrx","rxdir"));
  }*/

  window_main_->show_all();

}

void GUI::start() {

  switch_denoise_->signal_state_flags_changed().connect(
      sigc::mem_fun(this, &GUI::autoSettingsChanged)
  );
  switch_rx_->signal_state_flags_changed().connect(
      sigc::mem_fun(this, &GUI::autoSettingsChanged)
  );
  switch_rx_->signal_state_flags_changed().connect(
      sigc::mem_fun(this, &GUI::autoSettingsChanged)
  );
  button_abort_->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::abortedByUser)
  );
  button_clear_->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::imageReset)
  );

  redraw_dispatcher_.connect(sigc::mem_fun(*this, &GUI::onRedrawNotify));
  resync_dispatcher_.connect(sigc::mem_fun(*this, &GUI::onResyncNotify));

  std::vector<std::pair<int,std::string>> pa_devs = listPortaudioDevices();

  for (std::pair<int,std::string> dev : pa_devs) {
    combo_portaudio_->append(dev.second);
    if (dev.first == getDefaultPaDevice()) {
      combo_portaudio_->set_active(combo_portaudio_->get_children().size()-1);
    }
  }

  radio_input_portaudio_->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  radio_input_file_->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  radio_input_stdin_->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  combo_portaudio_->signal_changed().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  button_audiofilechooser_->signal_file_set().connect(
      sigc::mem_fun(this, &GUI::audioFileSelected)
  );


  listener_worker_thread_ = Glib::Threads::Thread::create(
      sigc::bind(sigc::mem_fun(listener_worker_, &Listener::listen), this));

  notReceiving();
  fetchAutoState();
  inputDeviceChanged();

  m_app->run(*window_main_);

}

void GUI::imageReset() {
  image_rx_->set(empty_pixbuf(500));
  label_lasttime_->set_text("");
}


// Draw signal level meters according to given values
#if 0
void setVU (double *Power, int FFTLen, int WinIdx, bool ShowWin) {
  int          x,y, W=100, H=30;
  guchar       *pixelsPWR, *pixelsSNR, *pPWR, *pSNR;
  unsigned int rowstridePWR,rowstrideSNR, LoBin, HiBin, i;
  double       logpow,p;

  rowstridePWR = pixbuf_PWR->get_rowstride();
  pixelsPWR    = pixbuf_PWR->get_pixels();

  rowstrideSNR = pixbuf_SNR->get_rowstride();
  pixelsSNR    = pixbuf_SNR->get_pixels();

  for (y=0; y<H; y++) {
    for (x=0; x<W; x++) {

      pPWR = pixelsPWR + y * rowstridePWR + (W-1-x) * 3;
      pSNR = pixelsSNR + y * rowstrideSNR + (W-1-x) * 3;

      if ((5-WinIdx)  >= (W-1-x)/16) {
        if (y > 3 && y < H-3 && (W-1-x) % 16 >3 && x % 2 == 0) {
            pSNR[0] = 0x34;
            pSNR[1] = 0xe4;
            pSNR[2] = 0x84;
        } else {
          pSNR[0] = 0x20;
          pSNR[1] = 0x20;
          pSNR[2] = 0x20;
        }
      } else {
        if (y > 3 && y < H-3 && (W-1-x) % 16 >3 && x % 2 == 0) {
          pSNR[0] = pSNR[1] = pSNR[2] = 0x40;
        } else {
          pSNR[0] = pSNR[1] = pSNR[2] = 0x20;
        }
      }

      LoBin = (int)((W-1-x)*(6000/W)/44100.0 * FFTLen);
      HiBin = (int)((W  -x)*(6000/W)/44100.0 * FFTLen);

      logpow = 0;
      for (i=LoBin; i<HiBin; i++) logpow += log(850*Power[i]) / 2;

      p = (H-1-y)/(H/23.0);

      pPWR[0] = pPWR[1] = pPWR[2] = 0;

      if (logpow > p) {
        pPWR[0] = 0;//clip(pPWR[0] + 0x22 * (logpow-p) / (HiBin-LoBin+1));
        pPWR[1] = 192;//clip(pPWR[1] + 0x66 * (logpow-p) / (HiBin-LoBin+1));
        pPWR[2] = 64;//clip(pPWR[2] + 0x22 * (logpow-p) / (HiBin-LoBin+1));
      }

      /*if (ShowWin && LoBin >= GetBin(1200+CurrentPic.HedrShift, FFTLen) &&
          HiBin <= GetBin(2300+CurrentPic.HedrShift, FFTLen)) {
        pPWR[0] = clip(pPWR[0] + 0x66);
        pPWR[1] = clip(pPWR[1] + 0x11);
        pPWR[2] = clip(pPWR[2] + 0x11);
      }*/

    }
  }

  //gui.image_pwr->set(pixbuf_PWR);
  //gui.image_snr->set(pixbuf_SNR);

}
#endif

bool GUI::isRxEnabled() {
  return m_is_rx_enabled;
}
bool GUI::isDenoiseEnabled() {
  return m_is_denoise_enabled;
}
bool GUI::isSyncEnabled() {
  return m_is_sync_enabled;
}
bool GUI::isAbortedByUser() {
  return m_is_aborted_by_user;
}
bool GUI::isSaveEnabled() {
  return check_save_->get_active();
}

void GUI::receiving() {
  button_abort_->set_sensitive(true);
  button_clear_->set_sensitive(false);
  button_manualstart_->set_sensitive(false);
  combo_manualmode_->set_sensitive(false);
  box_input_->set_sensitive(false);
}
void GUI::notReceiving() {
  button_abort_->set_sensitive(false);
  button_clear_->set_sensitive(true);
  button_manualstart_->set_sensitive(true);
  combo_manualmode_->set_sensitive(true);
  box_input_->set_sensitive(true);
}

void GUI::fetchAutoState() {
  m_is_denoise_enabled = switch_denoise_->get_active();
  m_is_rx_enabled      = switch_rx_->get_active();
  m_is_sync_enabled    = switch_sync_->get_active();
  m_is_fskid_enabled   = switch_sync_->get_active();
}

void GUI::abortedByUser() {
  m_is_aborted_by_user = true;
}
void GUI::ackAbortedByUser() {
  m_is_aborted_by_user = false;
}

void GUI::autoSettingsChanged(Gtk::StateFlags flags) {
  (void)flags;
  fetchAutoState();
}

void GUI::inputDeviceChanged() {
  printf("inputDeviceChanged\n");
  listener_worker_.close();
  if (radio_input_portaudio_->get_active()) {
    button_audiofilechooser_->set_sensitive(false);
    combo_portaudio_->set_sensitive(true);
    if (combo_portaudio_->get_active_row_number() >= 0) {
      listener_worker_.openPortAudioDev(combo_portaudio_->get_active_row_number());
    }
  } else if (radio_input_file_->get_active()) {
    button_audiofilechooser_->set_sensitive(true);
    combo_portaudio_->set_sensitive(false);
  } else if (radio_input_stdin_->get_active()) {
    button_audiofilechooser_->set_sensitive(false);
    combo_portaudio_->set_sensitive(false);
    listener_worker_.openStdin();
  }
}

int GUI::getSelectedPaDevice() {
  return combo_portaudio_->get_active_row_number();
}

eStreamType GUI::getSelectedStreamType() {
  eStreamType result;
  if (radio_input_portaudio_->get_active()) {
    result = STREAM_TYPE_PA;
  } else if (radio_input_file_->get_active()) {
    result = STREAM_TYPE_FILE;
  } else {
    result = STREAM_TYPE_STDIN;
  }
  return result;
}

std::string GUI::getSavedPictureLocation() {
  return button_savedirchooser_->get_filename();
}

void GUI::audioFileSelected() {
  listener_worker_.openFileStream(button_audiofilechooser_->get_filename());
}

void GUI::redrawNotify() {
  redraw_dispatcher_.emit();
}
void GUI::onRedrawNotify() {
  std::shared_ptr<Picture> pic = listener_worker_.getCurrentPic();
  label_lasttime_->set_text(pic->getTimestamp() + " / " + getModeSpec(pic->getMode()).name + " ");
  image_rx_->set(pic->renderPixbuf(500));

}

void GUI::resyncNotify() {
  resync_dispatcher_.emit();
}
void GUI::onResyncNotify() {
  if (switch_sync_->get_active()) {
    std::shared_ptr<Picture> pic = listener_worker_.getCurrentPic();
    pic->resync();
  }
}

