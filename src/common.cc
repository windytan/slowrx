#include "common.hh"
#include <thread>

bool     Abort           = false;
bool     Adaptive        = true;
bool    *HasSync         = NULL;
uint16_t HedrShift       = 0;
bool     ManualActivated = false;
bool     ManualResync    = false;
int      *StoredLum      = NULL;

Glib::RefPtr<Gdk::Pixbuf> pixbuf_PWR;
Glib::RefPtr<Gdk::Pixbuf> pixbuf_SNR;
Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
Glib::RefPtr<Gdk::Pixbuf> pixbuf_disp;

Glib::KeyFile config;

vector<thread> threads(2);

FFTStuff     fft;
PicMeta      CurrentPic;
PcmData      pcm;

// Return the FFT bin index matching the given frequency
int GetBin (double Freq, int FFTLen) {
  return (Freq / 44100 * FFTLen);
}

// Sinusoid power from complex DFT coefficients
double power (fftw_complex coeff) {
  return pow(coeff[0],2) + pow(coeff[1],2);
}

// Clip to [0..255]
int clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  round(a);
}

// Convert degrees -> radians
double deg2rad (double Deg) {
  return (Deg / 180) * M_PI;
}

// Convert radians -> degrees
double rad2deg (double rad) {
  return (180 / M_PI) * rad;
}

void ensure_dir_exists(string dir) {
  struct stat buf;

  int i = stat(dir.c_str(), &buf);
  if (i != 0) {
    if (mkdir(dir.c_str(), 0777) != 0) {
      perror("Unable to create directory for output file");
      exit(EXIT_FAILURE);
    }
  }
}

// Save current picture as PNG
void saveCurrentPic() {
  Glib::RefPtr<Gdk::Pixbuf> scaledpb;

  stringstream ss;
  string basename = config.get_string("slowrx","rxdir");

  ss << basename << "/" << CurrentPic.timestr << "_" << ModeSpec[CurrentPic.Mode].ShortName;

  string pngfilename = ss.str();
  cout << "  Saving to " << pngfilename << "\n";

  scaledpb = pixbuf_rx->scale_simple(ModeSpec[CurrentPic.Mode].ImgWidth,
    ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight, Gdk::INTERP_HYPER);

  ensure_dir_exists(config.get_string("slowrx","rxdir"));
  scaledpb->save(pngfilename, "png");
  //g_object_unref(scaledpb);
  //g_string_free(pngfilename, true);
}


/*** Gtk+ event handlers ***/


// Quit
void evt_deletewindow() {
  gtk_main_quit ();
}

// Transform the NoiseAdapt toggle state into a variable
void evt_GetAdaptive() {
  //Adaptive = gui.tog_adapt->get_active();
}

// Manual Start clicked
void evt_ManualStart() {
  ManualActivated = true;
}

// Abort clicked during rx
void evt_AbortRx() {
  Abort = true;
}

// Another device selected from list
void evt_changeDevices() {

  int status;

  pcm.BufferDrop = false;
  Abort = true;

  static int init;
  /*if (init)
    threads[0].join;*/
  init = 1;

//  if (pcm.handle != NULL) snd_pcm_close(pcm.handle);

  //status = initPcmDevice(gui.combo_card->get_active_text());


  switch(status) {
    case 0:
      //gui.image_devstatus->set(Gtk::Stock::YES,Gtk::ICON_SIZE_SMALL_TOOLBAR);
      //gui.image_devstatus->set_tooltip_text("Device successfully opened");
      break;
    case -1:
      //gui.image_devstatus->set(Gtk::Stock::DIALOG_WARNING,Gtk::ICON_SIZE_SMALL_TOOLBAR);
      //gui.image_devstatus->set_tooltip_text("Device was opened, but doesn't support 44100 Hz");
      break;
    case -2:
      //gui.image_devstatus->set(Gtk::Stock::DIALOG_ERROR,Gtk::ICON_SIZE_SMALL_TOOLBAR);
      //gui.image_devstatus->set_tooltip_text("Failed to open device");
      break;
  }

  //config->set_string("slowrx","device",gui.combo_card->get_active_text());

  //threads[0] =  thread(Listen);

}

// Clear received picture & metadata
void evt_clearPix() {
  pixbuf_disp->fill(0);
  /*gui.image_rx->set(pixbuf_disp);
  gui.label_fskid->set_markup("");
  gui.label_utc->set_markup("");
  gui.label_lastmode->set_markup("");*/
}

// Manual slant adjust
void evt_clickimg(Gtk::Widget *widget, GdkEventButton* event, Gdk::WindowEdge edge) {
  static double prevx=0,prevy=0,newrate;
  static bool   secondpress=false;
  double        x,y,dx,dy,xic;

  (void)widget;
  (void)edge;
/*
  if (event->type == Gdk::BUTTON_PRESS && event->button == 1 && gui.tog_setedge->get_active()) {

    x = event->x * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0);
    y = event->y * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0) / ModeSpec[CurrentPic.Mode].LineHeight;

    if (secondpress) {
      secondpress=false;

      dx = x - prevx;
      dy = y - prevy;

      //gui.tog_setedge->set_active(false);

      // Adjust sample rate, if in sensible limits
      newrate = CurrentPic.Rate + CurrentPic.Rate * (dx * ModeSpec[CurrentPic.Mode].PixelTime) / (dy * ModeSpec[CurrentPic.Mode].LineHeight * ModeSpec[CurrentPic.Mode].LineTime);
      if (newrate > 32000 && newrate < 56000) {
        CurrentPic.Rate = newrate;

        // Find x-intercept and adjust skip
        xic = fmod( (x - (y / (dy/dx))), ModeSpec[CurrentPic.Mode].ImgWidth);
        if (xic < 0) xic = ModeSpec[CurrentPic.Mode].ImgWidth - xic;
        CurrentPic.Skip = fmod(CurrentPic.Skip + xic * ModeSpec[CurrentPic.Mode].PixelTime * CurrentPic.Rate,
          ModeSpec[CurrentPic.Mode].LineTime * CurrentPic.Rate);
        if (CurrentPic.Skip > ModeSpec[CurrentPic.Mode].LineTime * CurrentPic.Rate / 2.0)
          CurrentPic.Skip -= ModeSpec[CurrentPic.Mode].LineTime * CurrentPic.Rate;

        // Signal the listener to exit from GetVIS() and re-process the pic
        ManualResync = true;
      }

    } else {
      secondpress = true;
      prevx = x;
      prevy = y;
    }
  } else {
    secondpress=false;
    //gui.tog_setedge->set_active(false);
  }*/
}
