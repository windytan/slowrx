#include <stdlib.h>
#include <gtk/gtk.h>
#include "common.h"

void delete_event() {
  gtk_main_quit ();
}

void createGUI() {

  int i;

  GtkWidget *vbox1;
  GtkWidget *camvbox1;
  GtkWidget *camvbox2;
  GtkWidget *camvbox3;
  GtkWidget *camhbox1;
  GtkWidget *camframe1;
  GtkWidget *camalign1;
  GtkWidget *label;
  GtkWidget *vuframe1;
  GtkWidget *vualign1;

  window = gtk_window_new               (GTK_WINDOW_TOPLEVEL);
  g_signal_connect                      (window, "delete-event", G_CALLBACK (delete_event), NULL);

  /* Cam tab */

  camframe1 = gtk_frame_new             ("Last image");
  gtk_container_set_border_width        (GTK_CONTAINER (camframe1), 5);
  RxPixbuf = gdk_pixbuf_new            (GDK_COLORSPACE_RGB, FALSE, 8, 320, 256);
  ClearPixbuf                           (RxPixbuf, 320, 256);
  DispPixbuf = gdk_pixbuf_scale_simple  (RxPixbuf, 500, 400, GDK_INTERP_NEAREST);
  CamImage  = gtk_image_new_from_pixbuf (DispPixbuf);

  camvbox3 = gtk_vbox_new               (FALSE, 1);
  gtk_box_pack_start                    (GTK_BOX (camvbox3), CamImage, FALSE, FALSE, 1);
  infolabel  = gtk_label_new            (" ");

  camalign1   = gtk_alignment_new       (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding             (GTK_ALIGNMENT (camalign1), 10,10,10,10);
  gtk_container_add                     (GTK_CONTAINER (camalign1), camvbox3);
  gtk_container_add                     (GTK_CONTAINER (camframe1), camalign1);

  camhbox1 = gtk_hbox_new               (FALSE, 1);
  camvbox1 = gtk_vbox_new               (FALSE, 1);
  gtk_box_pack_start                    (GTK_BOX (camhbox1), camvbox1, FALSE, FALSE, 1);
  gtk_box_pack_start                    (GTK_BOX (camvbox1), camframe1, FALSE, FALSE, 1);
  gtk_box_pack_start                    (GTK_BOX (camvbox3), infolabel, FALSE, FALSE, 1);

  /* Recent signals list */
  /*GtkTreeStore      *RecentStore;
  GtkWidget         *RecentTree;
  GtkTreeViewColumn *RecentColumn;
  GtkCellRenderer   *RecentRenderer;

  RecentStore    = gtk_list_store_new                       (1, GDK_TYPE_PIXBUF);
  populate_recent                                           ();
  RecentTree     = gtk_tree_view_new_with_model             (GTK_TREE_MODEL (RecentStore));
  g_object_unref                                            (G_OBJECT (RecentStore));
  RecentRenderer = gtk_cell_renderer_pixbuf_new             ();
  RecentColumn   = gtk_tree_view_column_new_with_attributes ("Recent", RecentRenderer, NULL);
  gtk_tree_view_append_column                               (GTK_TREE_VIEW (RecentTree), RecentColumn);
  gtk_box_pack_start                                        (GTK_BOX (camhbox1), RecentTree, FALSE, FALSE, 1);*/

  /* VU meter */

  camvbox2 = gtk_vbox_new              (FALSE, 1);
  gtk_box_pack_start                   (GTK_BOX (camhbox1), camvbox2, FALSE, FALSE, 1);

  vuframe1 = gtk_frame_new             ("Signal dB");
  gtk_container_set_border_width       (GTK_CONTAINER (vuframe1), 5);

  vutable = gtk_table_new              (5, 2, FALSE);

  vualign1   = gtk_alignment_new       (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding            (GTK_ALIGNMENT (vualign1), 10,10,10,10);
  gtk_container_add                    (GTK_CONTAINER (vualign1), vutable);
  gtk_container_add                    (GTK_CONTAINER (vuframe1), vualign1);
  gtk_box_pack_start                   (GTK_BOX (camvbox2), vuframe1, FALSE, FALSE, 1);

  int dBthresh[10]    = {0, -1, -2, -3, -5, -7, -10, -15, -20, -25};
  char dbstr[40];

  for (i=0; i<10; i++) {
    label = gtk_label_new("");
    if (dBthresh[i] < 0) snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>−%d</span>", abs(dBthresh[i]));
    else                 snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>%d</span>", dBthresh[i]);
    gtk_label_set_markup   (GTK_LABEL(label), dbstr);
    gtk_misc_set_alignment (GTK_MISC(label),1,0);
    gtk_table_attach       (GTK_TABLE(vutable), label, 0, 1, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
  }

  label  = gtk_label_new ("PWR");
  gtk_misc_set_alignment (GTK_MISC(label),.5,0);
  gtk_table_attach       (GTK_TABLE(vutable), label, 0, 2, 10, 11, GTK_FILL, GTK_FILL, 0, 4);
  label  = gtk_label_new ("SNR");
  gtk_misc_set_alignment (GTK_MISC(label),.5,0);
  gtk_table_attach       (GTK_TABLE(vutable), label, 2, 4, 10, 11, GTK_FILL, GTK_FILL, 0, 4);

  guchar *pixels, *p;
  unsigned int x,y,rowstride;

  VUpixbufActive = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 14);
  VUpixbufDim    = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 14);
  VUpixbufSNR    = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 14);

  rowstride = gdk_pixbuf_get_rowstride (VUpixbufDim);
  pixels    = gdk_pixbuf_get_pixels    (VUpixbufDim);

  for (y = 0; y < 14; y++) {
    for (x = 0; x < 20; x++) {
      p = pixels + y * rowstride + x * 3;
      p[0] = p[1] = p[2] = 192;
    }
  }

  rowstride = gdk_pixbuf_get_rowstride (VUpixbufActive);
  pixels    = gdk_pixbuf_get_pixels    (VUpixbufActive);

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
    VUimage[i]  = gtk_image_new_from_pixbuf (VUpixbufDim);
    SNRimage[i] = gtk_image_new_from_pixbuf (VUpixbufDim);
    gtk_table_attach(GTK_TABLE(vutable), VUimage[i],  1, 2, i, i+1, GTK_FILL, GTK_FILL, 2, 0);
    gtk_table_attach(GTK_TABLE(vutable), SNRimage[i], 2, 3, i, i+1, GTK_FILL, GTK_FILL, 2, 0);
  }

  int SNRdBthresh[10] = {30, 15, 10, 5, 3, 0, -3, -5, -10, -15};
  for (i=0; i<10; i++) {
    label = gtk_label_new("");
    if (SNRdBthresh[i] < 0) snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>−%d</span>", abs(SNRdBthresh[i]));
    else                    snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>%+d</span>", SNRdBthresh[i]);
    gtk_label_set_markup   (GTK_LABEL(label), dbstr);
    gtk_misc_set_alignment (GTK_MISC(label),0,0);
    gtk_table_attach       (GTK_TABLE(vutable), label, 3, 4, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
  }

  gtk_table_set_row_spacings(GTK_TABLE(vutable), 0);
  gtk_table_set_col_spacings(GTK_TABLE(vutable), 0);

  /* Tabbed notebook */
  notebook = gtk_notebook_new   ();
  gtk_notebook_set_tab_border   (GTK_NOTEBOOK (notebook), 4);
  gtk_notebook_append_page      (GTK_NOTEBOOK (notebook), camhbox1, gtk_label_new ("Rx") );

  /* Statusbar */
  statusbar = gtk_statusbar_new ();

  vbox1 = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start   (GTK_BOX (vbox1), notebook,  TRUE, TRUE, 0);
  gtk_box_pack_start   (GTK_BOX (vbox1), statusbar, TRUE, TRUE, 0);

  gtk_container_add    (GTK_CONTAINER (window), vbox1);

  gtk_widget_show_all  (window);

}
