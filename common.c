#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#ifdef GPL
#include <fftw3.h>
#endif

#include "common.h"

gint16      *PcmBuffer       = NULL;
int          PcmPointer      = 0;
int          MaxPcm          = 0;
guchar      *StoredLum       = NULL;
bool        *HasSync         = NULL;
double      *in              = NULL;
double      *out             = NULL;
gshort       HedrShift       = 0;
int          PWRdBthresh[10] = {-3, -5, -10, -15, -20, -25, -30, -40, -50, -60};
int          SNRdBthresh[10] = {30, 15, 10,   5,   3,   0,  -3,  -5, -10, -15};
bool         Adaptive        = true;
bool         ManualActivated = false;
bool         Abort           = false;

pthread_t    thread1;

GtkWidget   *RxImage         = NULL;
GtkWidget   *statusbar       = NULL;
GtkWidget   *vugrid          = NULL;
GtkWidget   *utclabel       = NULL;
GtkWidget   *lastmodelabel       = NULL;
GtkWidget   *cardcombo       = NULL;
GtkWidget   *modecombo       = NULL;
GtkWidget   *togslant        = NULL;
GtkWidget   *togsave         = NULL;
GtkWidget   *togadapt        = NULL;
GtkWidget   *togrx           = NULL;
GtkWidget   *togfsk          = NULL;
GtkWidget   *btnabort        = NULL;
GtkWidget   *btnstart        = NULL;
GtkWidget   *manualframe     = NULL;
GtkWidget   *shiftspin       = NULL;
GtkWidget   *pwrimage        = NULL;
GtkWidget   *snrimage        = NULL;
GtkWidget   *idlabel         = NULL;
GtkWidget   *devstatusicon   = NULL;

GdkPixbuf   *RxPixbuf        = NULL;
GdkPixbuf   *DispPixbuf      = NULL;
GdkPixbuf   *pixbufPWR       = NULL;
GdkPixbuf   *pixbufSNR       = NULL;

GtkListStore *savedstore     = NULL;

GKeyFile    *keyfile         = NULL;

snd_pcm_t   *pcm_handle      = NULL;

fftw_plan    Plan1024        = NULL;
fftw_plan    Plan2048        = NULL;

// Return the FFT bin index matching the given frequency
guint GetBin (double Freq, guint FFTLen) {
  return (Freq / 44100 * FFTLen);
}

// Clip to [0..255]
guchar clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  (guchar)round(a);
}

// Convert degrees -> radians
double deg2rad (double Deg) {
  return (Deg / 180) * M_PI;
}

// Quit
void delete_event() {
  gtk_main_quit ();
}

// Transform the NoiseAdapt toggle state into a variable
void GetAdaptive() {
  Adaptive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(togadapt));
}

// Manual Start clicked
void ManualStart() {
  ManualActivated = true;
}

// Abort clicked during rx
void AbortRx() {
  Abort = true;
}

// Another device selected from list
void changeDevices() {

  int status;

  Abort = true;

  pthread_join(thread1, NULL);

  if (pcm_handle != NULL) snd_pcm_close(pcm_handle);

  status = initPcmDevice(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(cardcombo)));


  switch(status) {
    case 0:
      gtk_image_set_from_stock(GTK_IMAGE(devstatusicon),GTK_STOCK_YES,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(devstatusicon, "Device successfully opened");
      break;
    case -1:
      gtk_image_set_from_stock(GTK_IMAGE(devstatusicon),GTK_STOCK_DIALOG_WARNING,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(devstatusicon, "Device was opened, but doesn't support 44100 Hz");
      break;
    case -2:
      gtk_image_set_from_stock(GTK_IMAGE(devstatusicon),GTK_STOCK_NO,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(devstatusicon, "Failed to open device");
      break;
  }

  g_key_file_set_string(keyfile,"slowrx","device",gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(cardcombo)));

  pthread_create (&thread1, NULL, Listen, NULL);

}