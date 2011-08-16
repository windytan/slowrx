#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   4096

extern guchar     VISmap[128];
extern int        PcmPointer;
extern int        PWRdBthresh[10];
extern int        SNRdBthresh[10];
extern gint16    *PcmBuffer;
extern double    *StoredFreq;
extern double    *in;
extern double    *out;
extern guint      StoredFreqRate;
extern gshort     HedrShift;
extern gboolean   Adaptive;
extern gboolean   ManualActivated;
extern gboolean   Abort;
extern gboolean  *HasSync;

extern GtkWidget *mainwindow;
extern GtkWidget *RxImage;
extern GtkWidget *statusbar;
extern GtkWidget *snrbar;
extern GtkWidget *pwrbar;
extern GtkWidget *vugrid;
extern GtkWidget *infolabel;
extern GtkWidget *aboutdialog;
extern GtkWidget *prefdialog;
extern GtkWidget *cardcombo;
extern GtkWidget *modecombo;
extern GtkWidget *togslant;
extern GtkWidget *togsave;
extern GtkWidget *togadapt;
extern GtkWidget *togrx;
extern GtkWidget *togfsk;
extern GtkWidget *btnabort;
extern GtkWidget *btnstart;
extern GtkWidget *manualframe;
extern GtkWidget *shiftspin;
extern GtkWidget *pwrimage;
extern GtkWidget *snrimage;
extern GtkWidget *idlabel;

extern GdkPixbuf *pixbufPWR;
extern GdkPixbuf *pixbufSNR;
extern GdkPixbuf *RxPixbuf;
extern GdkPixbuf *DispPixbuf;

extern GtkListStore *savedstore;

extern snd_pcm_t *pcm_handle;

extern fftw_plan  Plan1024;
extern fftw_plan  Plan2048;

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

typedef struct ModeSpecDef {
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
} ModeSpecDef;

extern ModeSpecDef ModeSpec[];

void     ClearPixbuf   (GdkPixbuf *, gushort, gushort);
void     createGUI     ();
gboolean GetVideo      (guchar, guint, int, gboolean);
guint    GetBin        (double, int);
guchar   clip          (double);
void     setVU         (short int, double);
guchar   GetVIS        ();
guint    FindSync      (guint, guchar, guint, int*);
double   deg2rad       (double);
void     initPcmDevice ();
void     delete_event  ();
void     GetAdaptive   ();
void     ManualStart   ();
void     AbortRx       ();
void     GetFSK        (char*);
void     readPcm       (gint);

#endif
