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

GUI::GUI() : m_is_aborted_by_user(false), m_dispatcher_redraw(), m_dispatcher_resync(),
  m_thr_listener_worker(nullptr), m_listener() {
  m_app = Gtk::Application::create("com.windytan.slowrx");

  Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
  builder->add_from_file("ui/slowrx.ui");
  builder->add_from_file("ui/aboutdialog.ui");

  //savedstore = iconview.get_model();

  builder->get_widget("label_lasttime",  m_label_lasttime);
  builder->get_widget("window_about",    m_window_about);
  builder->get_widget("window_main",     m_window_main);
  builder->get_widget("image_rx",        m_image_rx);
  builder->get_widget("button_abort",    m_button_abort);
  builder->get_widget("button_clear",    m_button_clear);
  builder->get_widget("button_manualstart",    m_button_manualstart);
  builder->get_widget("combo_manualmode",    m_combo_manualmode);
  builder->get_widget("combo_portaudio",    m_combo_portaudio);

  builder->get_widget("switch_rx",        m_switch_rx);
  builder->get_widget("switch_sync",        m_switch_sync);
  builder->get_widget("switch_denoise",        m_switch_denoise);
  builder->get_widget("switch_fskid",        m_switch_fskid);

  builder->get_widget("check_save",        m_check_save);

  builder->get_widget("radio_input_portaudio",        m_radio_input_portaudio);
  builder->get_widget("radio_input_file",        m_radio_input_file);
  builder->get_widget("radio_input_stdin",       m_radio_input_stdin);

  builder->get_widget("button_audiofilechooser",       m_button_audiofilechooser);

  builder->get_widget("save_dir_chooser",       m_button_savedirchooser);

  builder->get_widget("box_input",       m_box_input);

  imageReset();

  /*if (config.get_string("slowrx","rxdir") != NULL) {
    entry_picdir->set_text(config.get_string("slowrx","rxdir"));
  } else {
    config.set_string("slowrx","rxdir",g_get_home_dir());
    entry_picdir->set_text(config.get_string("slowrx","rxdir"));
  }*/

  m_window_main->show_all();

}

void GUI::start() {

  m_switch_denoise->signal_state_flags_changed().connect(
      sigc::mem_fun(this, &GUI::autoSettingsChanged)
  );
  m_switch_rx->signal_state_flags_changed().connect(
      sigc::mem_fun(this, &GUI::autoSettingsChanged)
  );
  m_switch_rx->signal_state_flags_changed().connect(
      sigc::mem_fun(this, &GUI::autoSettingsChanged)
  );
  m_button_abort->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::abortedByUser)
  );
  m_button_clear->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::imageReset)
  );

  m_dispatcher_redraw.connect(sigc::mem_fun(*this, &GUI::onRedrawNotify));
  m_dispatcher_resync.connect(sigc::mem_fun(*this, &GUI::onResyncNotify));

  std::vector<std::pair<int,std::string>> pa_devs = listPortaudioDevices();

  for (std::pair<int,std::string> dev : pa_devs) {
    m_combo_portaudio->append(dev.second);
    if (dev.first == getDefaultPaDevice()) {
      m_combo_portaudio->set_active(m_combo_portaudio->get_children().size()-1);
    }
  }

  m_radio_input_portaudio->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  m_radio_input_file->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  m_radio_input_stdin->signal_clicked().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  m_combo_portaudio->signal_changed().connect(
      sigc::mem_fun(this, &GUI::inputDeviceChanged)
  );
  m_button_audiofilechooser->signal_file_set().connect(
      sigc::mem_fun(this, &GUI::audioFileSelected)
  );


  m_thr_listener_worker = Glib::Threads::Thread::create(
      sigc::bind(sigc::mem_fun(m_listener, &Listener::listen), this));

  notReceiving();
  fetchAutoState();
  inputDeviceChanged();

  m_radio_input_stdin->set_sensitive(false);

  m_app->run(*m_window_main);

}

void GUI::imageReset() {
  m_image_rx->set(empty_pixbuf(640));
  m_label_lasttime->set_text("");
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
  return m_check_save->get_active();
}

void GUI::receiving() {
  m_button_abort->set_sensitive(true);
  m_button_clear->set_sensitive(false);
  m_button_manualstart->set_sensitive(false);
  m_combo_manualmode->set_sensitive(false);
  m_box_input->set_sensitive(false);
}
void GUI::notReceiving() {
  m_button_abort->set_sensitive(false);
  m_button_clear->set_sensitive(true);
  m_button_manualstart->set_sensitive(true);
  m_combo_manualmode->set_sensitive(true);
  m_box_input->set_sensitive(true);
}

void GUI::fetchAutoState() {
  m_is_denoise_enabled = m_switch_denoise->get_active();
  m_is_rx_enabled      = m_switch_rx->get_active();
  m_is_sync_enabled    = m_switch_sync->get_active();
  m_is_fskid_enabled   = m_switch_sync->get_active();
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
  m_listener.close();
  if (m_radio_input_portaudio->get_active()) {
    m_button_audiofilechooser->set_sensitive(false);
    m_combo_portaudio->set_sensitive(true);
    if (m_combo_portaudio->get_active_row_number() >= 0) {
      m_listener.openPortAudioDev(m_combo_portaudio->get_active_row_number());
    }
  } else if (m_radio_input_file->get_active()) {
    m_button_audiofilechooser->set_sensitive(true);
    m_combo_portaudio->set_sensitive(false);
  } else if (m_radio_input_stdin->get_active()) {
    m_button_audiofilechooser->set_sensitive(false);
    m_combo_portaudio->set_sensitive(false);
    m_listener.openStdin();
  }
}

int GUI::getSelectedPaDevice() {
  return m_combo_portaudio->get_active_row_number();
}

eStreamType GUI::getSelectedStreamType() {
  eStreamType result;
  if (m_radio_input_portaudio->get_active()) {
    result = STREAM_TYPE_PA;
  } else if (m_radio_input_file->get_active()) {
    result = STREAM_TYPE_FILE;
  } else {
    result = STREAM_TYPE_STDIN;
  }
  return result;
}

std::string GUI::getSavedPictureLocation() {
  return m_button_savedirchooser->get_filename();
}

void GUI::audioFileSelected() {
  m_listener.openFileStream(m_button_audiofilechooser->get_filename());
}

void GUI::redrawNotify() {
  m_dispatcher_redraw.emit();
}
void GUI::onRedrawNotify() {
  std::shared_ptr<Picture> pic = m_listener.getCurrentPic();
  m_label_lasttime->set_text(pic->getTimestamp() + " / " + getModeSpec(pic->getMode()).name + " ");
  m_image_rx->set(pic->renderPixbuf(500));

}

void GUI::resyncNotify() {
  m_dispatcher_resync.emit();
}
void GUI::onResyncNotify() {
  if (m_switch_sync->get_active()) {
    std::shared_ptr<Picture> pic = m_listener.getCurrentPic();
    pic->resync();
  }
}

