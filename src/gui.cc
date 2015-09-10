#include "gui.h"
#include "listener.h"
#include "picture.h"

Glib::RefPtr<Gdk::Pixbuf> empty_pixbuf(int px_width) {
  int px_height = round(px_width / 1.25);
  Glib::RefPtr<Gdk::Pixbuf> pixbuf =
    Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, px_width, px_height);
  pixbuf->fill(0x000000ff);

  return pixbuf;
}

SlowGUI::SlowGUI() : redraw_dispatcher_(), resync_dispatcher_(), worker_thread_(nullptr), worker_() {
  Glib::RefPtr<Gtk::Application> app =
    Gtk::Application::create("com.windytan.slowrx");

  Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
  builder->add_from_file("ui/slowrx.ui");
  builder->add_from_file("ui/aboutdialog.ui");

  builder->get_widget("button_abort",    button_abort);
  builder->get_widget("button_browse",   button_browse);
  builder->get_widget("button_clear",    button_clear);
  builder->get_widget("button_start",    button_start);
  builder->get_widget("combo_card",      combo_card);
  builder->get_widget("combo_mode",      combo_mode);
  builder->get_widget("entry_picdir",    entry_picdir);
  builder->get_widget("eventbox_img",    eventbox_img);
  builder->get_widget("frame_manual",    frame_manual);
  builder->get_widget("frame_slant",     frame_slant);
  builder->get_widget("grid_vu",         grid_vu);
  builder->get_widget("SavedIconView",   iconview);
  builder->get_widget("image_devstatus", image_devstatus);
  builder->get_widget("image_pwr",       image_pwr);
  builder->get_widget("image_rx",        image_rx);
  builder->get_widget("image_snr",       image_snr);
  builder->get_widget("label_fskid",     label_fskid);
  builder->get_widget("label_lastmode",  label_lastmode);
  builder->get_widget("label_utc",       label_utc);
  builder->get_widget("menuitem_quit",   menuitem_quit);
  builder->get_widget("menuitem_about",  menuitem_about);
  builder->get_widget("spin_shift",      spin_shift);
  builder->get_widget("statusbar",       statusbar);
  builder->get_widget("tog_adapt",       tog_adapt);
  builder->get_widget("tog_fsk",         tog_fsk);
  builder->get_widget("tog_rx",          tog_rx);
  builder->get_widget("tog_save",        tog_save);
  builder->get_widget("tog_setedge",     tog_setedge);
  builder->get_widget("tog_slant",       tog_slant);
  builder->get_widget("window_about",    window_about);
  builder->get_widget("window_main",     window_main);

  button_abort->signal_clicked().connect(sigc::ptr_fun(&evt_AbortRx));
  button_abort->signal_clicked().connect(sigc::ptr_fun(&evt_AbortRx));
  button_browse->signal_clicked().connect(sigc::ptr_fun(&evt_chooseDir));
  button_clear->signal_clicked().connect(sigc::ptr_fun(&evt_clearPix));
  button_start->signal_clicked().connect(sigc::ptr_fun(&evt_ManualStart));
  combo_card->signal_changed().connect(sigc::ptr_fun(&evt_changeDevices));
  //eventbox_img->signal_button_press_event().connect(sigc::ptr_fun(&evt_clickimg));
  menuitem_quit->signal_activate().connect(sigc::ptr_fun(&evt_deletewindow));
  menuitem_about->signal_activate().connect(sigc::ptr_fun(&evt_show_about));
  tog_adapt->signal_activate().connect(sigc::ptr_fun(&evt_GetAdaptive));
  //window_main->signal_delete_event().connect(sigc::ptr_fun(&evt_deletewindow));

  //savedstore = iconview.get_model();

  image_rx->set(empty_pixbuf(500));

  //pixbuf_PWR = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 100, 30);
  //pixbuf_SNR = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, 100, 30);

  combo_mode->set_active(0);

  if (config.get_string("slowrx","rxdir") != NULL) {
    entry_picdir->set_text(config.get_string("slowrx","rxdir"));
  } else {
    config.set_string("slowrx","rxdir",g_get_home_dir());
    entry_picdir->set_text(config.get_string("slowrx","rxdir"));
  }

  //setVU(0, 6);
  
  window_main->show_all();

  redraw_dispatcher_.connect(sigc::mem_fun(*this, &SlowGUI::onRedrawNotify));
  resync_dispatcher_.connect(sigc::mem_fun(*this, &SlowGUI::onResyncNotify));

  worker_thread_ = Glib::Threads::Thread::create(
      sigc::bind(sigc::mem_fun(worker_, &Listener::listen), this));

  app->run(*window_main);
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

void SlowGUI::redrawNotify() {
  redraw_dispatcher_.emit();
}
void SlowGUI::onRedrawNotify() {
  Picture* pic = worker_.getCurrentPic();
  label_lastmode->set_text(ModeSpec[pic->getMode()].name);
  image_rx->set(pic->renderPixbuf(500));

}

void SlowGUI::resyncNotify() {
  resync_dispatcher_.emit();
}
void SlowGUI::onResyncNotify() {
  Picture* pic = worker_.getCurrentPic();
  pic->resync();
}

void evt_chooseDir() {
  /*Gtk::FileChooserDialog dialog (*gui.window_main, "Select folder",
                                      Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
  
  if (dialog.run() == Gtk::RESPONSE_ACCEPT) {
    config->set_string("slowrx","rxdir",dialog.get_filename());
    //gui.entry_picdir->set_text(dialog.get_filename());
  }

  //dialog.destroy_widget();*/
}

void evt_show_about() {
  //gui.window_about->show();
  //gui.window_about->hide();
}
