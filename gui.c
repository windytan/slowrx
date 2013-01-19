#include <stdlib.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <math.h>

#include <fftw3.h>

#include "common.h"

void createGUI() {

  GtkBuilder *builder;

  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "slowrx.ui",      NULL);
  gtk_builder_add_from_file(builder, "window_about.ui", NULL);
  
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

  pixbuf_PWR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 20);
  pixbuf_SNR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 20);

  gtk_combo_box_set_active(GTK_COMBO_BOX(gui.combo_mode), 0);

  if (g_key_file_get_string(config,"slowrx","rxdir",NULL) != NULL) {
    gtk_entry_set_text(GTK_ENTRY(gui.entry_picdir),g_key_file_get_string(config,"slowrx","rxdir",NULL));
  } else {
    g_key_file_set_string(config,"slowrx","rxdir",g_get_home_dir());
    gtk_entry_set_text(GTK_ENTRY(gui.entry_picdir),g_key_file_get_string(config,"slowrx","rxdir",NULL));
  }

  setVU(0, -100);

  gtk_widget_show_all  (gui.window_main);

}

// Draw signal level meters according to given values
void setVU (short int PcmValue, double SNRdB) {
  int          x,y;
  int          PWRdB = (int)round(10 * log10(pow(PcmValue/32767.0,2)));
  guchar       *pixelsPWR, *pixelsSNR, *pPWR, *pSNR;
  unsigned int rowstridePWR,rowstrideSNR;

  rowstridePWR = gdk_pixbuf_get_rowstride (pixbuf_PWR);
  pixelsPWR    = gdk_pixbuf_get_pixels    (pixbuf_PWR);
  
  rowstrideSNR = gdk_pixbuf_get_rowstride (pixbuf_SNR);
  pixelsSNR    = gdk_pixbuf_get_pixels    (pixbuf_SNR);

  for (y=0; y<20; y++) {
    for (x=0; x<100; x++) {

      pPWR = pixelsPWR + y * rowstridePWR + (99-x) * 3;
      pSNR = pixelsSNR + y * rowstrideSNR + (99-x) * 3;

      if (y > 1 && y < 18 && x % 2 == 0) {

        if (PWRdB >= -0.0075*pow(x,2)-3) {
          pPWR[0] = 0x89;
          pPWR[1] = 0xfe;
          pPWR[2] = 0xf4;
        } else {
          pPWR[0] = pPWR[1] = pPWR[2] = 0x80;
        }

        if (SNRdB >= -0.6*x+40) {
          pSNR[0] = 0xef;
          pSNR[1] = 0xe4;
          pSNR[2] = 0x34;
        } else {
          pSNR[0] = pSNR[1] = pSNR[2] = 0x80;
        }

      } else {
        pPWR[0] = pPWR[1] = pPWR[2] = 0x40;
        pSNR[0] = pSNR[1] = pSNR[2] = 0x40;
      }

    }
  }

  gdk_threads_enter();
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_pwr), pixbuf_PWR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_snr), pixbuf_SNR);
  gdk_threads_leave();

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
