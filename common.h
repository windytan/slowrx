#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   4096

extern guchar     VISmap[];
extern int        PcmPointer;
extern int        PWRdBthresh[];
extern int        SNRdBthresh[];
extern int        MaxPcm;
extern gint16    *PcmBuffer;
extern guchar    *StoredLum;
extern double    *in;
extern double    *out;
extern gshort     HedrShift;
extern bool       Adaptive;
extern bool       ManualActivated;
extern bool       Abort;
extern bool      *HasSync;

extern pthread_t thread1;

typedef struct _GuiObjs GuiObjs;
struct _GuiObjs {
  GtkWidget *mainwindow;
  GtkWidget *RxImage;
  GtkWidget *statusbar;
  GtkWidget *vugrid;
  GtkWidget *infolabel;
  GtkWidget *utclabel;
  GtkWidget *lastmodelabel;
  GtkWidget *cardcombo;
  GtkWidget *modecombo;
  GtkWidget *togslant;
  GtkWidget *togsave;
  GtkWidget *togadapt;
  GtkWidget *togrx;
  GtkWidget *togfsk;
  GtkWidget *btnabort;
  GtkWidget *btnstart;
  GtkWidget *manualframe;
  GtkWidget *shiftspin;
  GtkWidget *pwrimage;
  GtkWidget *snrimage;
  GtkWidget *idlabel;
  GtkWidget *devstatusicon;
  GtkWidget *picdirentry;
  GtkWidget *browsebtn;
};
extern GuiObjs   gui;

extern GdkPixbuf *pixbufPWR;
extern GdkPixbuf *pixbufSNR;
extern GdkPixbuf *RxPixbuf;
extern GdkPixbuf *DispPixbuf;

extern GtkListStore *savedstore;

extern GKeyFile  *keyfile;

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

void     createGUI     ();
bool     GetVideo      (guchar Mode, double Rate, int Skip, bool Redraw);
guint    GetBin        (double Freq, guint FFTLen);
guchar   clip          (double a);
void     setVU         (short int PcmValue, double SNRdB);
guchar   GetVIS        ();
double   FindSync      (guchar Mode, double Rate, int *Skip);
double   deg2rad       (double Deg);
int      initPcmDevice ();
void     delete_event  ();
void     GetAdaptive   ();
void     ManualStart   ();
void     AbortRx       ();
void     GetFSK        (char *dest);
void     readPcm       (gint numsamples);
void     *Listen       ();
void     changeDevices ();
void     populateDeviceList ();
void     setNewRxDir   ();
void     chooseDir     ();

#endif
