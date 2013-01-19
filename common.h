#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   4096
#define SYNCPIXLEN 1.5e-3

extern bool       Abort;
extern bool       Adaptive;
extern bool      *HasSync;
extern double    *in;
extern bool       ManualActivated;
extern bool       ManualResync;
extern double    *out;
extern guchar    *StoredLum;
extern pthread_t  thread1;
extern guchar     VISmap[];

typedef struct _PcmData PcmData;
struct _PcmData {
  snd_pcm_t *handle;
  int        PeakVal;
  gint16    *Buffer;
  int        WindowPtr;
  bool       BufferDrop;
};
extern PcmData pcm;

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


extern fftw_plan  Plan1024;
extern fftw_plan  Plan2048;

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

typedef struct ModeSpec {
  char   *Name;
  char   *ShortName;
  double  SyncLen;
  double  PorchLen;
  double  SeparatorLen;
  double  PixelLen;
  double  LineLen;
  gushort ImgWidth;
  gushort ImgHeight;
  guchar  YScale;
  guchar  ColorEnc;
} _ModeSpec;

extern _ModeSpec ModeSpec[];

guchar   clip          (double a);
void     createGUI     ();
double   deg2rad       (double Deg);
double   FindSync      (guchar Mode, double Rate, int *Skip);
void     GetFSK        (char *dest);
bool     GetVideo      (guchar Mode, double Rate, int Skip, bool Redraw);
guchar   GetVIS        ();
guint    GetBin        (double Freq, guint FFTLen);
int      initPcmDevice ();
void     *Listen       ();
void     populateDeviceList ();
void     readPcm       (gint numsamples);
void     saveCurrentPic();
void     setVU         (short int PcmValue, double SNRdB);

void     evt_AbortRx       ();
void     evt_changeDevices ();
void     evt_chooseDir     ();
void     evt_clearPix      ();
void     evt_clickimg      ();
void     evt_deletewindow  ();
void     evt_GetAdaptive   ();
void     evt_ManualStart   ();
void     evt_show_about    ();

#endif
