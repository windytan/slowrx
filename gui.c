#include <stdlib.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

void createGUI() {

  GtkBuilder *builder;
  GtkWidget  *quititem;
  GtkWidget  *aboutitem;
  GtkWidget  *prefitem;

  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "slowrx.ui",      NULL);
  gtk_builder_add_from_file(builder, "aboutdialog.ui", NULL);
  gtk_builder_add_from_file(builder, "prefs.ui",       NULL);

  vugrid      = GTK_WIDGET(gtk_builder_get_object(builder,"vugrid"));
  mainwindow  = GTK_WIDGET(gtk_builder_get_object(builder,"mainwindow"));
  RxImage     = GTK_WIDGET(gtk_builder_get_object(builder,"RxImage"));
  statusbar   = GTK_WIDGET(gtk_builder_get_object(builder,"statusbar"));
  infolabel   = GTK_WIDGET(gtk_builder_get_object(builder,"infolabel"));
  quititem    = GTK_WIDGET(gtk_builder_get_object(builder,"quititem"));
  prefitem    = GTK_WIDGET(gtk_builder_get_object(builder,"prefmenuitem"));
  aboutitem   = GTK_WIDGET(gtk_builder_get_object(builder,"aboutitem"));
  aboutdialog = GTK_WIDGET(gtk_builder_get_object(builder,"aboutdialog"));
  prefdialog  = GTK_WIDGET(gtk_builder_get_object(builder,"prefdialog"));
  cardcombo   = GTK_WIDGET(gtk_builder_get_object(builder,"cardcombo"));
  togslant    = GTK_WIDGET(gtk_builder_get_object(builder,"TogSlant"));
  togsave     = GTK_WIDGET(gtk_builder_get_object(builder,"TogSave"));
  togadapt    = GTK_WIDGET(gtk_builder_get_object(builder,"TogAdapt"));
  togrx       = GTK_WIDGET(gtk_builder_get_object(builder,"TogRx"));
  modecombo   = GTK_WIDGET(gtk_builder_get_object(builder,"modecombo"));
  btnabort    = GTK_WIDGET(gtk_builder_get_object(builder,"BtnAbort"));
  btnstart    = GTK_WIDGET(gtk_builder_get_object(builder,"BtnStart"));
  manualframe = GTK_WIDGET(gtk_builder_get_object(builder,"ManualFrame"));
  shiftspin   = GTK_WIDGET(gtk_builder_get_object(builder,"ShiftSpin"));
  pwrimage    = GTK_WIDGET(gtk_builder_get_object(builder,"PowerImage"));
  snrimage    = GTK_WIDGET(gtk_builder_get_object(builder,"SNRImage"));

  g_signal_connect        (quititem,    "activate",     G_CALLBACK(delete_event),        NULL);
  g_signal_connect        (mainwindow,  "delete-event", G_CALLBACK(delete_event),        NULL);
  g_signal_connect_swapped(aboutitem,   "activate",     G_CALLBACK(gtk_widget_show_all), aboutdialog);
  g_signal_connect_swapped(prefitem,    "activate",     G_CALLBACK(gtk_widget_show_all), prefdialog);
  g_signal_connect_swapped(aboutdialog, "close",        G_CALLBACK(gtk_widget_hide),     aboutdialog);
  g_signal_connect_swapped(togadapt,    "toggled",      G_CALLBACK(GetAdaptive),         NULL);
  g_signal_connect        (btnstart,    "clicked",      G_CALLBACK(ManualStart),         NULL);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togslant), TRUE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togsave),  TRUE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togadapt), TRUE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togrx),    TRUE);
  gtk_combo_box_set_active    (GTK_COMBO_BOX(modecombo),    0);
  gtk_widget_set_sensitive    (btnabort,                    FALSE);
  
  RxPixbuf   = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 320, 256);
  ClearPixbuf (RxPixbuf, 320, 256);
  DispPixbuf = gdk_pixbuf_scale_simple (RxPixbuf, 500, 400, GDK_INTERP_NEAREST);
  gtk_image_set_from_pixbuf(GTK_IMAGE(RxImage), DispPixbuf);

  pixbufPWR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 20);
  pixbufSNR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 100, 20);

  setVU(0, -100);

  gtk_widget_show_all  (mainwindow);

}
