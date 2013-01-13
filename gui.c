#include <stdlib.h>
#include <stdbool.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <math.h>

#include <fftw3.h>

#include "common.h"

void createGUI() {

  GtkBuilder *builder;
  GtkWidget  *quititem,  *aboutitem;
  GtkWidget  *iconview;

  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "slowrx.ui",      NULL);
  gtk_builder_add_from_file(builder, "aboutdialog.ui", NULL);
  
  quititem    = GTK_WIDGET(gtk_builder_get_object(builder,"quititem"));
  aboutitem   = GTK_WIDGET(gtk_builder_get_object(builder,"aboutitem"));
  
  gui.mainwindow    = GTK_WIDGET(gtk_builder_get_object(builder,"mainwindow"));
  gui.aboutdialog   = GTK_WIDGET(gtk_builder_get_object(builder,"aboutdialog"));
  gui.vugrid        = GTK_WIDGET(gtk_builder_get_object(builder,"vugrid"));
  gui.RxImage       = GTK_WIDGET(gtk_builder_get_object(builder,"RxImage"));
  gui.statusbar     = GTK_WIDGET(gtk_builder_get_object(builder,"statusbar"));
  gui.utclabel      = GTK_WIDGET(gtk_builder_get_object(builder,"utclabel"));
  gui.lastmodelabel = GTK_WIDGET(gtk_builder_get_object(builder,"lastmodelabel"));
  gui.cardcombo     = GTK_WIDGET(gtk_builder_get_object(builder,"cardcombo"));
  gui.togslant      = GTK_WIDGET(gtk_builder_get_object(builder,"TogSlant"));
  gui.togsave       = GTK_WIDGET(gtk_builder_get_object(builder,"TogSave"));
  gui.togadapt      = GTK_WIDGET(gtk_builder_get_object(builder,"TogAdapt"));
  gui.togrx         = GTK_WIDGET(gtk_builder_get_object(builder,"TogRx"));
  gui.togfsk        = GTK_WIDGET(gtk_builder_get_object(builder,"TogFSK"));
  gui.modecombo     = GTK_WIDGET(gtk_builder_get_object(builder,"modecombo"));
  gui.btnabort      = GTK_WIDGET(gtk_builder_get_object(builder,"BtnAbort"));
  gui.btnstart      = GTK_WIDGET(gtk_builder_get_object(builder,"BtnStart"));
  gui.manualframe   = GTK_WIDGET(gtk_builder_get_object(builder,"ManualFrame"));
  gui.shiftspin     = GTK_WIDGET(gtk_builder_get_object(builder,"ShiftSpin"));
  gui.pwrimage      = GTK_WIDGET(gtk_builder_get_object(builder,"PowerImage"));
  gui.snrimage      = GTK_WIDGET(gtk_builder_get_object(builder,"SNRImage"));
  gui.idlabel       = GTK_WIDGET(gtk_builder_get_object(builder,"IDLabel"));
  gui.devstatusicon = GTK_WIDGET(gtk_builder_get_object(builder,"devstatusicon"));
  gui.browsebtn     = GTK_WIDGET(gtk_builder_get_object(builder,"browsebtn"));
  gui.picdirentry   = GTK_WIDGET(gtk_builder_get_object(builder,"picdirentry"));
  
  iconview          = GTK_WIDGET(gtk_builder_get_object(builder,"SavedIconView"));

  g_signal_connect        (quititem,    "activate",     G_CALLBACK(delete_event),        NULL);
  g_signal_connect        (gui.mainwindow,"delete-event",G_CALLBACK(delete_event),       NULL);
  g_signal_connect        (aboutitem,   "activate",     G_CALLBACK(show_aboutdialog),    NULL);
  g_signal_connect_swapped(gui.togadapt,    "toggled",      G_CALLBACK(GetAdaptive),         NULL);
  g_signal_connect        (gui.btnstart,    "clicked",      G_CALLBACK(ManualStart),         NULL);
  g_signal_connect        (gui.btnabort,    "clicked",      G_CALLBACK(AbortRx),             NULL);
  g_signal_connect        (gui.cardcombo,   "changed",      G_CALLBACK(changeDevices),       NULL);
  g_signal_connect        (gui.browsebtn,   "clicked",      G_CALLBACK(chooseDir),           NULL);

  savedstore = GTK_LIST_STORE(gtk_icon_view_get_model(GTK_ICON_VIEW(iconview)));

  RxPixbuf   = gdk_pixbuf_new (GDK_COLORSPACE_RGB, false, 8, 320, 256);
  gdk_pixbuf_fill(RxPixbuf, 0x000000ff);
  DispPixbuf = gdk_pixbuf_scale_simple (RxPixbuf, 500, 400, GDK_INTERP_BILINEAR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.RxImage), DispPixbuf);

  pixbufPWR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, false, 8, 100, 20);
  pixbufSNR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, false, 8, 100, 20);

  gtk_combo_box_set_active(GTK_COMBO_BOX(gui.modecombo), 0);

  if (g_key_file_get_string(keyfile,"slowrx","rxdir",NULL) != NULL) {
    gtk_entry_set_text(GTK_ENTRY(gui.picdirentry),g_key_file_get_string(keyfile,"slowrx","rxdir",NULL));
  } else {
    g_key_file_set_string(keyfile,"slowrx","rxdir",g_get_home_dir());
    gtk_entry_set_text(GTK_ENTRY(gui.picdirentry),g_key_file_get_string(keyfile,"slowrx","rxdir",NULL));
  }

  setVU(0, -100);

  gtk_widget_show_all  (gui.mainwindow);

}

// Draw signal level meters according to given values
void setVU (short int PcmValue, double SNRdB) {
  int x,y;
  int PWRdB = (int)round(10 * log10(pow(PcmValue/32767.0,2)));
  guchar *pixelsPWR, *pixelsSNR, *pPWR, *pSNR;
  unsigned int rowstridePWR,rowstrideSNR;

  rowstridePWR = gdk_pixbuf_get_rowstride (pixbufPWR);
  pixelsPWR    = gdk_pixbuf_get_pixels    (pixbufPWR);
  
  rowstrideSNR = gdk_pixbuf_get_rowstride (pixbufSNR);
  pixelsSNR    = gdk_pixbuf_get_pixels    (pixbufSNR);

  for (y=0; y<20; y++) {
    for (x=0; x<100; x++) {

      pPWR = pixelsPWR + y * rowstridePWR + (99-x) * 3;
      pSNR = pixelsSNR + y * rowstrideSNR + (99-x) * 3;

      if (y > 1 && y < 18 && x % 10 > 1 && x % 10 < 8 && x % 2 == 0 && y % 2 == 0) {

        if (PWRdB >= PWRdBthresh[x/10]) {
          pPWR[0] = 0x59;
          pPWR[1] = 0xfe;
          pPWR[2] = 0xf4;
        } else {
          pPWR[0] = pPWR[1] = pPWR[2] = 0x80;
        }

        if (SNRdB >= SNRdBthresh[x/10]) {        
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
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.pwrimage), pixbufPWR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.snrimage), pixbufSNR);
  gdk_threads_leave();

}

void chooseDir() {
  GtkWidget *dialog;
  dialog = gtk_file_chooser_dialog_new ("Select folder",
                                      GTK_WINDOW(gui.mainwindow),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                      NULL);
  
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    g_key_file_set_string(keyfile,"slowrx","rxdir",gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
    gtk_entry_set_text(GTK_ENTRY(gui.picdirentry),gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
  }

  gtk_widget_destroy (dialog);
}

void show_aboutdialog() {
  gtk_dialog_run(GTK_DIALOG(gui.aboutdialog));
  gtk_widget_hide(gui.aboutdialog);
}
