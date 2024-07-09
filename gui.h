#ifndef _GUI_H_
#define _GUI_H_

extern GdkPixbuf *pixbuf_PWR;
extern GdkPixbuf *pixbuf_SNR;
extern GdkPixbuf *pixbuf_rx;
extern GdkPixbuf *pixbuf_disp;

extern GtkListStore *savedstore;

typedef struct _GuiObjs GuiObjs;
struct _GuiObjs {
  GtkWidget *button_abort;
  GtkWidget *button_browse;
  GtkWidget *button_clear;
  GtkWidget *button_start;
  GtkWidget *combo_card;
  GtkWidget *combo_mode;
  GtkWidget *entry_picdir;
  GtkWidget *eventbox_img;
  GtkWidget *frame_manual;
  GtkWidget *frame_slant;
  GtkWidget *grid_vu;
  GtkWidget *iconview;
  GtkWidget *image_devstatus;
  GtkWidget *image_pwr;
  GtkWidget *image_rx;
  GtkWidget *image_snr;
  GtkWidget *label_fskid;
  GtkWidget *label_lastmode;
  GtkWidget *label_utc;
  GtkWidget *menuitem_about;
  GtkWidget *menuitem_quit;
  GtkWidget *spin_shift;
  GtkWidget *statusbar;
  GtkWidget *tog_adapt;
  GtkWidget *tog_fsk;
  GtkWidget *tog_rx;
  GtkWidget *tog_save;
  GtkWidget *tog_setedge;
  GtkWidget *tog_slant;
  GtkWidget *window_about;
  GtkWidget *window_main;
};
extern GuiObjs   gui;

void     createGUI     ();
void     setVU         (double *Power, int FFTLen, int WinIdx);
void     saveCurrentPic();

void     evt_chooseDir     ();
void     evt_show_about    ();
void     evt_AbortRx       ();
void     evt_changeDevices ();
void     evt_clearPix      ();
void     evt_clickimg      ();
void     evt_deletewindow  ();
void     evt_GetAdaptive   ();
void     evt_ManualStart   ();

#endif
