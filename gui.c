#include <stdlib.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

void createGUI() {

  GtkBuilder *builder;
  GtkWidget  *label;
  GtkWidget  *quititem;
  GtkWidget  *aboutitem;

  builder = gtk_builder_new();
  gtk_builder_add_from_file(builder, "slowrx.ui",      NULL);
  gtk_builder_add_from_file(builder, "aboutdialog.ui", NULL);

  vugrid      = GTK_WIDGET(gtk_builder_get_object(builder,"vugrid"));
  mainwindow  = GTK_WIDGET(gtk_builder_get_object(builder,"mainwindow"));
  RxImage     = GTK_WIDGET(gtk_builder_get_object(builder,"RxImage"));
  statusbar   = GTK_WIDGET(gtk_builder_get_object(builder,"statusbar"));
  infolabel   = GTK_WIDGET(gtk_builder_get_object(builder,"infolabel"));
  quititem    = GTK_WIDGET(gtk_builder_get_object(builder,"quititem"));
  aboutitem   = GTK_WIDGET(gtk_builder_get_object(builder,"aboutitem"));
  aboutdialog = GTK_WIDGET(gtk_builder_get_object(builder,"aboutdialog"));

  g_signal_connect        (quititem,    "activate",     G_CALLBACK(delete_event),        NULL);
  g_signal_connect        (mainwindow,  "delete-event", G_CALLBACK(delete_event),        NULL);
  g_signal_connect_swapped(aboutitem,   "activate",     G_CALLBACK(gtk_widget_show_all), aboutdialog);
  g_signal_connect_swapped(aboutitem,   "activate",     G_CALLBACK(gtk_widget_show_all), aboutdialog);
  g_signal_connect_swapped(aboutdialog, "close",        G_CALLBACK(gtk_widget_hide),     aboutdialog);

  RxPixbuf   = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 320, 256);
  ClearPixbuf (RxPixbuf, 320, 256);
  DispPixbuf = gdk_pixbuf_scale_simple (RxPixbuf, 500, 400, GDK_INTERP_NEAREST);
  gtk_image_set_from_pixbuf(GTK_IMAGE(RxImage), DispPixbuf);

  /* PWR & SNR indicators */

  int i;

  int  PWRdBthresh[10] = {0, -1, -2, -3, -5, -7, -10, -15, -20, -25};
  int  SNRdBthresh[10] = {30, 15, 10, 5, 3, 0, -3, -5, -10, -15};
  char dbstr[40];

  /* dB labels */

  for (i=0; i<10; i++) {
    label = gtk_label_new("");
    if (PWRdBthresh[i] < 0) snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>−%d</span>", abs(PWRdBthresh[i]));
    else                    snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>%d</span>", PWRdBthresh[i]);
    gtk_label_set_markup   (GTK_LABEL(label), dbstr);
    gtk_misc_set_alignment (GTK_MISC(label),1,0);
    gtk_grid_attach        (GTK_GRID(vugrid), label, 0, i, 1, 1);
  }
  
  for (i=0; i<10; i++) {
    label = gtk_label_new("");
    if (SNRdBthresh[i] < 0) snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>−%d</span>", abs(SNRdBthresh[i]));
    else                    snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>%+d</span>", SNRdBthresh[i]);
    gtk_label_set_markup   (GTK_LABEL(label), dbstr);
    gtk_misc_set_alignment (GTK_MISC(label), 0, 0);
    gtk_grid_attach        (GTK_GRID(vugrid), label, 3, i, 1, 1);
  }

  /* Indicator pictures */

  guchar *pixels, *p;
  unsigned int x,y,rowstride;

  VUpixbufPWR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 14);
  VUpixbufSNR = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 14);
  VUpixbufDim = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 14);

  rowstride = gdk_pixbuf_get_rowstride (VUpixbufDim);
  pixels    = gdk_pixbuf_get_pixels    (VUpixbufDim);

  for (y = 0; y < 14; y++) {
    for (x = 0; x < 20; x++) {
      p = pixels + y * rowstride + x * 3;
      p[0] = p[1] = p[2] = (y % 2 ? 192 : 160);
    }
  }

  rowstride = gdk_pixbuf_get_rowstride (VUpixbufPWR);
  pixels    = gdk_pixbuf_get_pixels    (VUpixbufPWR);

  for (y = 0; y < 14; y++) {
    for (x = 0; x < 20; x++) {
      p = pixels + y * rowstride + x * 3;
      if (y % 2 == 0) {
        p[0] = 42;
        p[1] = 127;
        p[2] = 255;
      } else {
        p[0] = p[1] = p[2] = 192;
      }
    }
  }

  rowstride = gdk_pixbuf_get_rowstride (VUpixbufSNR);
  pixels    = gdk_pixbuf_get_pixels    (VUpixbufSNR);

  for (y = 0; y < 14; y++) {
    for (x = 0; x < 20; x++) {
      p = pixels + y * rowstride + x * 3;
      if (y % 2 == 0) {
        p[0] = 255;
        p[1] = 127;
        p[2] = 45;
      } else {
        p[0] = p[1] = p[2] = 192;
      }
    }
  }

  for (i=0;i<10;i++) {
    PWRimage[i] = gtk_image_new_from_pixbuf (VUpixbufDim);
    SNRimage[i] = gtk_image_new_from_pixbuf (VUpixbufDim);
    gtk_grid_attach(GTK_GRID(vugrid), PWRimage[i], 1, i, 1, 1);
    gtk_grid_attach(GTK_GRID(vugrid), SNRimage[i], 2, i, 1, 1);
  }

  gtk_widget_show_all  (mainwindow);

}
