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

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect        (window, "delete-event", G_CALLBACK (delete_event), NULL);

  /* Cam tab */

  camframe1 = gtk_frame_new             ("Last image");
  gtk_container_set_border_width        (GTK_CONTAINER (camframe1), 5);
  CamPixbuf = gdk_pixbuf_new            (GDK_COLORSPACE_RGB, FALSE, 8, 320, 256);
  ClearPixbuf                           (CamPixbuf, 320, 256);
  CamImage  = gtk_image_new_from_pixbuf (CamPixbuf);

  camvbox3 = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start      (GTK_BOX (camvbox3), CamImage, FALSE, FALSE, 1);
  infolabel  = gtk_label_new(" ");

  camalign1   = gtk_alignment_new        (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding              (GTK_ALIGNMENT (camalign1), 10,10,10,10);
  gtk_container_add                      (GTK_CONTAINER (camalign1), camvbox3);
  gtk_container_add                      (GTK_CONTAINER (camframe1), camalign1);

  camhbox1 = gtk_hbox_new (FALSE, 1);
  camvbox1 = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start      (GTK_BOX (camhbox1), camvbox1, FALSE, FALSE, 1);
  gtk_box_pack_start      (GTK_BOX (camvbox1), camframe1, FALSE, FALSE, 1);
  gtk_box_pack_start      (GTK_BOX (camvbox3), infolabel, FALSE, FALSE, 1);

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

  camvbox2 = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start                     (GTK_BOX (camhbox1), camvbox2, FALSE, FALSE, 1);

  vuframe1 = gtk_frame_new             ("Signal dB");
  gtk_container_set_border_width       (GTK_CONTAINER (vuframe1), 5);

  vutable = gtk_table_new (5, 2, FALSE);

  vualign1   = gtk_alignment_new         (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding              (GTK_ALIGNMENT (vualign1), 10,10,10,10);
  gtk_container_add                      (GTK_CONTAINER (vualign1), vutable);
  gtk_container_add                      (GTK_CONTAINER (vuframe1), vualign1);
  gtk_box_pack_start                     (GTK_BOX (camvbox2), vuframe1, FALSE, FALSE, 1);

  int dBthresh[10]    = {0, -1, -2, -3, -5, -7, -10, -15, -20, -25};
  char dbstr[40];

  for (i=0; i<10; i++) {
    label = gtk_label_new("");
    if (dBthresh[i] < 0) snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>−%d</span>", abs(dBthresh[i]));
    else                 snprintf(dbstr, sizeof(dbstr)-1, "<span font='9px'>%d</span>", dBthresh[i]);
    gtk_label_set_markup(GTK_LABEL(label), dbstr);
    gtk_misc_set_alignment(GTK_MISC(label),1,0);
    gtk_table_attach(GTK_TABLE(vutable), label, 0, 1, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
  }

  label  = gtk_label_new("PWR");
  gtk_misc_set_alignment(GTK_MISC(label),.5,0);
  gtk_table_attach(GTK_TABLE(vutable), label, 0, 2, 10, 11, GTK_FILL, GTK_FILL, 0, 4);
  label  = gtk_label_new("SNR");
  gtk_misc_set_alignment(GTK_MISC(label),.5,0);
  gtk_table_attach(GTK_TABLE(vutable), label, 2, 4, 10, 11, GTK_FILL, GTK_FILL, 0, 4);

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
    gtk_label_set_markup(GTK_LABEL(label), dbstr);
    gtk_misc_set_alignment(GTK_MISC(label),0,0);
    gtk_table_attach(GTK_TABLE(vutable), label, 3, 4, i, i+1, GTK_FILL, GTK_FILL, 0, 0);
  }

  gtk_table_set_row_spacings(GTK_TABLE(vutable), 0);
  gtk_table_set_col_spacings(GTK_TABLE(vutable), 0);

  /* WAV tab */

  GtkWidget *wavhbox1;
  //GtkWidget *wavvbox1;

  wavhbox1 = gtk_hbox_new (FALSE, 2);
  /*wavvbox1 = gtk_vbox_new (FALSE, 5);
  gtk_box_pack_start      (GTK_BOX (wavhbox1), wavvbox1, TRUE, TRUE, 0);

  GtkWidget    *wavframe1;
  wavframe1   = gtk_frame_new               ("WAV file");
  gtk_container_set_border_width            (GTK_CONTAINER (wavframe1), 10);
  gtk_box_pack_start                        (GTK_BOX (wavvbox1), wavframe1, FALSE, TRUE, 0);
  GtkWidget    *wavalign1;
  wavalign1   = gtk_alignment_new           (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding                 (GTK_ALIGNMENT (wavalign1), 5,5,12,5);
  GtkWidget    *wavchooserb;
  wavchooserb = gtk_file_chooser_button_new ("Select a file", GTK_FILE_CHOOSER_ACTION_OPEN);
  gtk_container_add                         (GTK_CONTAINER (wavalign1), wavchooserb);
  gtk_container_add                         (GTK_CONTAINER (wavframe1), wavalign1);

  GtkWidget  *wavframe2;
  wavframe2 = gtk_frame_new       ("Decoder");
  gtk_container_set_border_width  (GTK_CONTAINER (wavframe2), 10);
  GtkWidget  *wavtable1;
  wavtable1 = gtk_table_new       (4,3,FALSE);
  GtkWidget  *wavalign2;
  wavalign2 = gtk_alignment_new   (0.5,0.5,1.0,1.0);
  gtk_alignment_set_padding       (GTK_ALIGNMENT (wavalign2), 5,5,12,5);
  gtk_container_add               (GTK_CONTAINER (wavalign2), wavtable1);
  gtk_container_add               (GTK_CONTAINER (wavframe2), wavalign2);

  GtkWidget  *modecombo;
  modecombo = gtk_combo_box_new_text();
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Martin M1");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Martin M2");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Scottie S1");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Scottie S2");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Scottie DX");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Robot 72 Color");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Robot 36 Color");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Robot 24 Color");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Robot 24 B&W");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Robot 12 B&W");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Robot 8 B&W");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "PD90");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "PD50");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Wraase SC2-180");
  gtk_combo_box_append_text (GTK_COMBO_BOX (modecombo), "Auto mode selection (VIS)");
  gtk_combo_box_set_active  (GTK_COMBO_BOX (modecombo), 14);
  gtk_table_attach_defaults (GTK_TABLE (wavtable1), modecombo, 0, 4, 0, 1);
  gtk_box_pack_start        (GTK_BOX (wavvbox1), wavframe2, FALSE, TRUE, 0);

  GtkWidget    *adaptcheck;
  adaptcheck  = gtk_check_button_new_with_label ("Noise adapt.");
  gtk_table_attach_defaults                     (GTK_TABLE (wavtable1), adaptcheck, 0, 2, 1, 2);
 
  GtkWidget      *autosynccheck;
  autosynccheck = gtk_check_button_new_with_label ("Auto sync.");
  gtk_table_attach_defaults                       (GTK_TABLE (wavtable1), autosynccheck, 2, 4, 1, 2);

  label        = gtk_label_new       ("Shift Hz");
  gtk_table_attach_defaults          (GTK_TABLE (wavtable1), label, 0, 1, 2, 3);
  GtkObject     *wavshiftadj;
  wavshiftadj  = gtk_adjustment_new  (0, -100, 100, 1, 2, 0);
  GtkWidget     *wavshiftspin;
  wavshiftspin = gtk_spin_button_new (GTK_ADJUSTMENT (wavshiftadj), 1, 2);
  gtk_spin_button_set_numeric        (GTK_SPIN_BUTTON (wavshiftspin), TRUE);
  gtk_table_attach_defaults          (GTK_TABLE (wavtable1), wavshiftspin, 1, 2, 2, 3);

  label =    gtk_label_new       ("Fs");
  gtk_table_attach_defaults      (GTK_TABLE (wavtable1), label, 2, 3, 2, 3);
  GtkObject *wavadj1;
  wavadj1  = gtk_adjustment_new  (44100, 42000, 46000, 1, 2, 0);
  GtkWidget *wavspin1;
  wavspin1 = gtk_spin_button_new (GTK_ADJUSTMENT (wavadj1), 1, 2);
  gtk_spin_button_set_numeric    (GTK_SPIN_BUTTON (wavspin1), TRUE);
  gtk_table_attach_defaults      (GTK_TABLE (wavtable1), wavspin1, 3, 4, 2, 3);

  label    = gtk_label_new       ("Phase");
  gtk_table_attach_defaults      (GTK_TABLE (wavtable1), label, 2, 3, 3, 4);
  GtkObject *wavadj2;
  wavadj2  = gtk_adjustment_new  (0, 0, 5000, 1, 2, 0);
 GtkWidget *wavspin2;
  wavspin2 = gtk_spin_button_new (GTK_ADJUSTMENT (wavadj2), 1, 1);
  gtk_spin_button_set_numeric    (GTK_SPIN_BUTTON (wavspin2), TRUE);
  gtk_table_attach_defaults      (GTK_TABLE (wavtable1), wavspin2, 3, 4, 3, 4);

  GtkWidget    *demodbutton;
  demodbutton = gtk_button_new_with_label ("Run");
  gtk_table_attach_defaults               (GTK_TABLE (wavtable1), demodbutton, 0, 4, 4, 5);
  g_signal_connect                      (GTK_OBJECT (demodbutton), "clicked", GTK_SIGNAL_FUNC (wavdemod), NULL);

  GtkWidget   *wavframe4;
  wavframe4 = gtk_frame_new       ("Log entry");
  gtk_container_set_border_width  (GTK_CONTAINER (wavframe4), 10);
  GtkWidget  *wavtable2;
  wavtable2 = gtk_table_new       (4, 5, FALSE);*/



  /* Tabbed notebook */
  notebook = gtk_notebook_new   ();
  gtk_notebook_set_tab_border   (GTK_NOTEBOOK (notebook), 4);
  gtk_notebook_append_page      (GTK_NOTEBOOK (notebook), camhbox1, gtk_label_new ("Rx") );
  gtk_notebook_append_page      (GTK_NOTEBOOK (notebook), wavhbox1, gtk_label_new ("WAV") );

  /* Statusbar */
  statusbar = gtk_statusbar_new ();

  vbox1 = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start   (GTK_BOX (vbox1), notebook,  TRUE, TRUE, 0);
  gtk_box_pack_start   (GTK_BOX (vbox1), statusbar, TRUE, TRUE, 0);

  gtk_container_add    (GTK_CONTAINER (window), vbox1);

  gtk_widget_show_all  (window);

}
