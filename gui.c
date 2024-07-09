#include <stdlib.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <math.h>

#include <fftw3.h>

#include "common.h"
#include "config.h"
#include "gui.h"
#include "listen.h"
#include "modespec.h"
#include "pcm.h"

GuiObjs      gui;

GdkPixbuf   *pixbuf_rx       = NULL;
GdkPixbuf   *pixbuf_disp     = NULL;
GdkPixbuf   *pixbuf_PWR      = NULL;
GdkPixbuf   *pixbuf_SNR      = NULL;

GtkListStore *savedstore     = NULL;

void createGUI() {

  GtkBuilder *builder;

  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "slowrx.ui",      NULL);
  gtk_builder_add_from_file(builder, "aboutdialog.ui", NULL);
  
  gui.button_abort    = GTK_WIDGET(gtk_builder_get_object(builder,"button_abort"));
  gui.button_browse   = GTK_WIDGET(gtk_builder_get_object(builder,"button_browse"));
  gui.button_clear    = GTK_WIDGET(gtk_builder_get_object(builder,"button_clear"));
  gui.button_start    = GTK_WIDGET(gtk_builder_get_object(builder,"button_start"));
  gui.combo_card      = GTK_WIDGET(gtk_builder_get_object(builder,"combo_card"));
  gui.combo_mode      = GTK_WIDGET(gtk_builder_get_object(builder,"combo_mode"));
  gui.entry_picdir    = GTK_WIDGET(gtk_builder_get_object(builder,"entry_picdir"));
  gui.eventbox_img    = GTK_WIDGET(gtk_builder_get_object(builder,"eventbox_img"));
  gui.frame_manual    = GTK_WIDGET(gtk_builder_get_object(builder,"frame_manual"));
  gui.frame_slant     = GTK_WIDGET(gtk_builder_get_object(builder,"frame_slant"));
  gui.grid_vu         = GTK_WIDGET(gtk_builder_get_object(builder,"grid_vu"));
  gui.iconview        = GTK_WIDGET(gtk_builder_get_object(builder,"SavedIconView"));
  gui.image_devstatus = GTK_WIDGET(gtk_builder_get_object(builder,"image_devstatus"));
  gui.image_pwr       = GTK_WIDGET(gtk_builder_get_object(builder,"image_pwr"));
  gui.image_rx        = GTK_WIDGET(gtk_builder_get_object(builder,"image_rx"));
  gui.image_snr       = GTK_WIDGET(gtk_builder_get_object(builder,"image_snr"));
  gui.label_fskid     = GTK_WIDGET(gtk_builder_get_object(builder,"label_fskid"));
  gui.label_lastmode  = GTK_WIDGET(gtk_builder_get_object(builder,"label_lastmode"));
  gui.label_utc       = GTK_WIDGET(gtk_builder_get_object(builder,"label_utc"));
  gui.menuitem_quit   = GTK_WIDGET(gtk_builder_get_object(builder,"menuitem_quit"));
  gui.menuitem_about  = GTK_WIDGET(gtk_builder_get_object(builder,"menuitem_about"));
  gui.spin_shift      = GTK_WIDGET(gtk_builder_get_object(builder,"spin_shift"));
  gui.statusbar       = GTK_WIDGET(gtk_builder_get_object(builder,"statusbar"));
  gui.tog_adapt       = GTK_WIDGET(gtk_builder_get_object(builder,"tog_adapt"));
  gui.tog_fsk         = GTK_WIDGET(gtk_builder_get_object(builder,"tog_fsk"));
  gui.tog_rx          = GTK_WIDGET(gtk_builder_get_object(builder,"tog_rx"));
  gui.tog_save        = GTK_WIDGET(gtk_builder_get_object(builder,"tog_save"));
  gui.tog_setedge     = GTK_WIDGET(gtk_builder_get_object(builder,"tog_setedge"));
  gui.tog_slant       = GTK_WIDGET(gtk_builder_get_object(builder,"tog_slant"));
  gui.window_about    = GTK_WIDGET(gtk_builder_get_object(builder,"window_about"));
  gui.window_main     = GTK_WIDGET(gtk_builder_get_object(builder,"window_main"));

  g_signal_connect        (gui.button_abort,  "clicked",      G_CALLBACK(evt_AbortRx),       NULL);
  g_signal_connect        (gui.button_browse, "clicked",      G_CALLBACK(evt_chooseDir),     NULL);
  g_signal_connect        (gui.button_clear,  "clicked",      G_CALLBACK(evt_clearPix),      NULL);
  g_signal_connect        (gui.button_start,  "clicked",      G_CALLBACK(evt_ManualStart),   NULL);
  g_signal_connect        (gui.combo_card,    "changed",      G_CALLBACK(evt_changeDevices), NULL);
  g_signal_connect        (gui.eventbox_img,  "button-press-event",G_CALLBACK(evt_clickimg),     NULL);
  g_signal_connect        (gui.menuitem_quit, "activate",     G_CALLBACK(evt_deletewindow),  NULL);
  g_signal_connect        (gui.menuitem_about,"activate",     G_CALLBACK(evt_show_about),    NULL);
  g_signal_connect_swapped(gui.tog_adapt,     "toggled",      G_CALLBACK(evt_GetAdaptive),   NULL);
  g_signal_connect        (gui.window_main,   "delete-event", G_CALLBACK(evt_deletewindow),  NULL);

  savedstore = GTK_LIST_STORE(gtk_icon_view_get_model(GTK_ICON_VIEW(gui.iconview)));

  pixbuf_rx   = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 320, 256);
  gdk_pixbuf_fill(pixbuf_rx, 0x000000ff);
  pixbuf_disp = gdk_pixbuf_scale_simple (pixbuf_rx, 500, 400, GDK_INTERP_BILINEAR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);

  pixbuf_PWR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 30);
  pixbuf_SNR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 30);

  gtk_combo_box_set_active(GTK_COMBO_BOX(gui.combo_mode), 0);

  if (g_key_file_get_string(config,"slowrx","rxdir",NULL) != NULL) {
    gtk_entry_set_text(GTK_ENTRY(gui.entry_picdir),g_key_file_get_string(config,"slowrx","rxdir",NULL));
  } else {
    g_key_file_set_string(config,"slowrx","rxdir",g_get_home_dir());
    gtk_entry_set_text(GTK_ENTRY(gui.entry_picdir),g_key_file_get_string(config,"slowrx","rxdir",NULL));
  }

  //setVU(0, 6);

  gtk_widget_show_all  (gui.window_main);

}

// Draw signal level meters according to given values
void setVU (double *Power, int FFTLen, int WinIdx) {
  int          x,y, W=100, H=30;
  guchar       *pixelsPWR, *pixelsSNR, *pPWR, *pSNR;
  unsigned int rowstridePWR,rowstrideSNR, LoBin, HiBin, i;
  double       logpow,p;

  rowstridePWR = gdk_pixbuf_get_rowstride (pixbuf_PWR);
  pixelsPWR    = gdk_pixbuf_get_pixels    (pixbuf_PWR);
  
  rowstrideSNR = gdk_pixbuf_get_rowstride (pixbuf_SNR);
  pixelsSNR    = gdk_pixbuf_get_pixels    (pixbuf_SNR);

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

  gdk_threads_enter();
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_pwr), pixbuf_PWR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_snr), pixbuf_SNR);
  gdk_threads_leave();

}

// Save current picture as PNG
void saveCurrentPic() {
  GdkPixbuf *scaledpb;
  GString   *pngfilename;

  pngfilename = g_string_new(g_key_file_get_string(config,"slowrx","rxdir",NULL));
  g_string_append_printf(pngfilename, "/%s_%s.png", CurrentPic.timestr, ModeSpec[CurrentPic.Mode].ShortName);
  printf("  Saving to %s\n", pngfilename->str);

  scaledpb = gdk_pixbuf_scale_simple (pixbuf_rx, ModeSpec[CurrentPic.Mode].ImgWidth,
    ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight, GDK_INTERP_HYPER);

  ensure_dir_exists(g_key_file_get_string(config,"slowrx","rxdir",NULL));
  gdk_pixbuf_savev(scaledpb, pngfilename->str, "png", NULL, NULL, NULL);
  g_object_unref(scaledpb);
  g_string_free(pngfilename, TRUE);
}

void evt_chooseDir() {
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new ("Select folder",
                                      GTK_WINDOW(gui.window_main),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                      NULL);
  
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    g_key_file_set_string(config,"slowrx","rxdir",gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
    gtk_entry_set_text(GTK_ENTRY(gui.entry_picdir),gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
  }

  gtk_widget_destroy (dialog);
}

void evt_show_about() {
  gtk_dialog_run(GTK_DIALOG(gui.window_about));
  gtk_widget_hide(gui.window_about);
}

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
    WaitForListenerStop();
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

  StartListener();

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
