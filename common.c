#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"

gint16      *PcmBuffer       = NULL;
int          PcmPointer      = 0;
int          MaxPcm          = 0;
guchar      *StoredLum       = NULL;
bool        *HasSync         = NULL;
double      *in              = NULL;
double      *out             = NULL;
gshort       HedrShift       = 0;
bool         Adaptive        = true;
bool         ManualActivated = false;
bool         Abort           = false;
bool         BufferDrop      = false;

pthread_t    thread1;

GuiObjs      gui;

GdkPixbuf   *pixbuf_rx        = NULL;
GdkPixbuf   *pixbuf_disp      = NULL;
GdkPixbuf   *pixbuf_PWR       = NULL;
GdkPixbuf   *pixbuf_SNR       = NULL;

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

// Convert radians -> degrees
double rad2deg (double rad) {
  return (180 / M_PI) * rad;
}


/*** Gtk+ event handlers ***/


// Quit
void evt_deletewindow() {
  gtk_main_quit ();
}

// Transform the NoiseAdapt toggle state into a variable
void evt_GetAdaptive() {
  Adaptive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_adapt));
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

  BufferDrop = false;
  Abort = true;

  pthread_join(thread1, NULL);

  if (pcm_handle != NULL) snd_pcm_close(pcm_handle);

  status = initPcmDevice(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(gui.combo_card)));


  switch(status) {
    case 0:
      gtk_image_set_from_stock(GTK_IMAGE(gui.image_devstatus),GTK_STOCK_YES,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(gui.image_devstatus, "Device successfully opened");
      break;
    case -1:
      gtk_image_set_from_stock(GTK_IMAGE(gui.image_devstatus),GTK_STOCK_DIALOG_WARNING,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(gui.image_devstatus, "Device was opened, but doesn't support 44100 Hz");
      break;
    case -2:
      gtk_image_set_from_stock(GTK_IMAGE(gui.image_devstatus),GTK_STOCK_DIALOG_ERROR,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(gui.image_devstatus, "Failed to open device");
      break;
  }

  g_key_file_set_string(keyfile,"slowrx","device",gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(gui.combo_card)));

  pthread_create (&thread1, NULL, Listen, NULL);

}

// Clear received picture & metadata
void evt_clearPix() {
  gdk_pixbuf_fill (pixbuf_disp, 0);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);
  gtk_label_set_markup (GTK_LABEL(gui.label_fskid), "");
  gtk_label_set_markup (GTK_LABEL(gui.label_utc), "");
  gtk_label_set_markup (GTK_LABEL(gui.label_lastmode), "");
}

// Manual slant adjust
void evt_clickimg(GtkWidget *widget, GdkEventButton* event, GdkWindowEdge edge) {
  static double prevx=0,prevy=0;
  static bool   secondpress=false;
  double        dx,dy,a;

  if (event->type == GDK_BUTTON_PRESS && event->button == 1 && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_setedge))) {
    if (secondpress) {
      printf(":) %.1f,%.1f -> %.1f,%.1f\n",prevx,prevy,event->x,event->y);
      dx = event->x - prevx;
      dy = event->y - prevy;
      a  = rad2deg(M_PI/2 - asin(dx/sqrt(pow(dx,2) + pow(dy,2))));
      if (event->y < prevy) a = fabs(a-180);
      printf("%.3f\n",a);

      secondpress=false;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui.tog_setedge),false);
    } else {
      prevx = event->x;
      prevy = event->y;
      secondpress = true;
    }
  }
}
