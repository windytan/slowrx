#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   4096
#define SYNCPIXLEN 1.5e-3

extern gboolean   Abort;
extern gboolean   Adaptive;
extern gboolean  *HasSync;
extern gboolean   ManualActivated;
extern gboolean   ManualResync;
extern guchar    *StoredLum;
extern pthread_t  thread1;

typedef struct _FFTStuff FFTStuff;
struct _FFTStuff {
  double       *in;
  fftw_complex *out;
  fftw_plan     Plan1024;
  fftw_plan     Plan2048;
};
extern FFTStuff fft;

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

extern GdkPixbuf *pixbuf_PWR;
extern GdkPixbuf *pixbuf_SNR;
extern GdkPixbuf *pixbuf_rx;
extern GdkPixbuf *pixbuf_disp;

extern GtkListStore *savedstore;

extern GKeyFile  *config;


typedef struct _PicMeta PicMeta;
struct _PicMeta {
  gshort HedrShift;
  guchar Mode;
  double Rate;
  int    Skip;
  GdkPixbuf *thumbbuf;
  char   timestr[40];
};
extern PicMeta CurrentPic;

// SSTV modes
enum {
  UNKNOWN=0,
  M1,    M2,   M3,    M4,
  S1,    S2,   SDX,
  R72,   R36,  R24,   R24BW, R12BW, R8BW,
  PD50,  PD90, PD120, PD160, PD180, PD240, PD290,
  P3,    P5,   P7,
  W2120, W2180
};

// Color encodings
enum {
  GBR, RGB, YUV, BW
};

double   power     (fftw_complex coeff);
guchar   clip          (double a);
double   deg2rad       (double Deg);
guchar   GetVIS        ();
guint    GetBin        (double Freq, guint FFTLen);
void     *Listen       ();
void     saveCurrentPic();

#endif
