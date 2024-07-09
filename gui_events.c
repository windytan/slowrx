#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "gui.h"
#include "modespec.h"
#include "pcm.h"


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

  static int init;
  if (init)
    pthread_join(thread1, NULL);
  init = 1;

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
    y = event->y * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0) / ModeSpec[CurrentPic.Mode].LineHeight;

    if (secondpress) {
      secondpress=FALSE;

      dx = x - prevx;
      dy = y - prevy;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui.tog_setedge),FALSE);

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
