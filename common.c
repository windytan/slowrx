#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"

gboolean     Abort           = FALSE;
gboolean     Adaptive        = TRUE;
gboolean    *HasSync         = NULL;
gshort       HedrShift       = 0;
gboolean     ManualActivated = FALSE;
gboolean     ManualResync    = FALSE;
guchar      *StoredLum       = NULL;

pthread_t    thread1;

FFTStuff     fft;
GuiObjs      gui;
PicMeta      CurrentPic;
PcmData      pcm;

GdkPixbuf   *pixbuf_rx       = NULL;
GdkPixbuf   *pixbuf_disp     = NULL;
GdkPixbuf   *pixbuf_PWR      = NULL;
GdkPixbuf   *pixbuf_SNR      = NULL;

GtkListStore *savedstore     = NULL;

GKeyFile    *config          = NULL;

// Return the FFT bin index matching the given frequency
guint GetBin (double Freq, guint FFTLen) {
  return (Freq / 44100 * FFTLen);
}

// Sinusoid power from complex DFT coefficients
double power (fftw_complex coeff) {
  return pow(coeff[0],2) + pow(coeff[1],2);
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

void ensure_dir_exists(const char *dir) {
  struct stat buf;

  int i = stat(dir, &buf);
  if (i != 0) {
    if (mkdir(dir, 0777) != 0) {
      perror("Unable to create directory for output file");
      exit(EXIT_FAILURE);
    }
  }
}

// Save current picture as PNG
void saveCurrentPic() {
  GdkPixbuf *scaledpb;
  GString   *pngfilename;

  pngfilename = g_string_new(g_key_file_get_string(config,"slowrx","rxdir",NULL));
  g_string_append_printf(pngfilename, "/%s_%s.png", CurrentPic.timestr, ModeSpec[CurrentPic.Mode].ShortName);
  printf("  Saving to %s\n", pngfilename->str);

  scaledpb = gdk_pixbuf_scale_simple (pixbuf_rx, ModeSpec[CurrentPic.Mode].ImgWidth,
    ModeSpec[CurrentPic.Mode].ImgHeight * ModeSpec[CurrentPic.Mode].YScale, GDK_INTERP_HYPER);

  ensure_dir_exists(g_key_file_get_string(config,"slowrx","rxdir",NULL));
  gdk_pixbuf_savev(scaledpb, pngfilename->str, "png", NULL, NULL, NULL);
  g_object_unref(scaledpb);
  g_string_free(pngfilename, TRUE);
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
  ManualActivated = TRUE;
}

// Abort clicked during rx
void evt_AbortRx() {
  Abort = TRUE;
}

// Another device selected from list
void evt_changeDevices() {

  int status;

  pcm.BufferDrop = FALSE;
  Abort = TRUE;

  pthread_join(thread1, NULL);

  if (pcm.handle != NULL) snd_pcm_close(pcm.handle);

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

  g_key_file_set_string(config,"slowrx","device",gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(gui.combo_card)));

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
  static double prevx=0,prevy=0,newrate;
  static gboolean   secondpress=FALSE;
  double        x,y,dx,dy,xic;

  (void)widget;
  (void)edge;

  if (event->type == GDK_BUTTON_PRESS && event->button == 1 && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_setedge))) {

    x = event->x * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0);
    y = event->y * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0) / ModeSpec[CurrentPic.Mode].YScale;

    if (secondpress) {
      secondpress=FALSE;

      dx = x - prevx;
      dy = y - prevy;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui.tog_setedge),FALSE);

      // Adjust sample rate, if in sensible limits
      newrate = CurrentPic.Rate + CurrentPic.Rate * (dx * ModeSpec[CurrentPic.Mode].PixelLen) / (dy * ModeSpec[CurrentPic.Mode].YScale * ModeSpec[CurrentPic.Mode].LineLen);
      if (newrate > 32000 && newrate < 56000) {
        CurrentPic.Rate = newrate;

        // Find x-intercept and adjust skip
        xic = fmod( (x - (y / (dy/dx))), ModeSpec[CurrentPic.Mode].ImgWidth);
        if (xic < 0) xic = ModeSpec[CurrentPic.Mode].ImgWidth - xic;
        CurrentPic.Skip = fmod(CurrentPic.Skip + xic * ModeSpec[CurrentPic.Mode].PixelLen * CurrentPic.Rate,
          ModeSpec[CurrentPic.Mode].LineLen * CurrentPic.Rate);
        if (CurrentPic.Skip > ModeSpec[CurrentPic.Mode].LineLen * CurrentPic.Rate / 2.0)
          CurrentPic.Skip -= ModeSpec[CurrentPic.Mode].LineLen * CurrentPic.Rate;

        // Signal the listener to exit from GetVIS() and re-process the pic
        ManualResync = TRUE;
      }

    } else {
      secondpress = TRUE;
      prevx = x;
      prevy = y;
    }
  } else {
    secondpress=FALSE;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui.tog_setedge), FALSE);
  }
}
