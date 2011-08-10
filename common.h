#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define SNRSIZE  512
#define SRATE    44100

extern int          VISmap[128];
extern short int    PcmBuffer[2048];
extern double      *PCM;
extern int          PcmPointer;
extern int          Sample;
extern unsigned int SRate;
extern double      *StoredFreq;
extern double       StoredFreqRate;
extern double       HedrShift;
extern int          PWRdBthresh[10];
extern int          SNRdBthresh[10];
extern int          Adaptive;
extern int          ManualActivated;

extern GtkWidget   *mainwindow;
extern GtkWidget   *notebook;
extern GdkPixbuf   *RxPixbuf;
extern GdkPixbuf   *DispPixbuf;
extern GtkWidget   *RxImage;
extern GtkWidget   *statusbar;
extern GtkWidget   *snrbar;
extern GtkWidget   *pwrbar;
extern GtkWidget   *vugrid;
extern GdkPixbuf   *pixbufPWR;
extern GdkPixbuf   *pixbufSNR;
extern GtkWidget   *infolabel;
extern GtkWidget   *aboutdialog;
extern GtkWidget   *prefdialog;
extern GtkWidget   *sdialog;
extern GtkWidget   *cardcombo;
extern GtkWidget   *modecombo;
extern GtkWidget   *togslant;
extern GtkWidget   *togsave;
extern GtkWidget   *togadapt;
extern GtkWidget   *togrx;
extern GtkWidget   *btnabort;
extern GtkWidget   *btnstart;
extern GtkWidget   *manualframe;
extern GtkWidget   *shiftspin;
extern GtkWidget   *pwrimage;
extern GtkWidget   *snrimage;

extern snd_pcm_t   *pcm_handle;

enum {
  UNKNOWN=0,
  M1,    M2,   M3,    M4,
  S1,    S2,   SDX,
  R72,   R36,  R24,   R24BW, R12BW, R8BW,
  PD50,  PD90, PD120, PD160, PD180, PD240, PD290,
  P3,    P5,   P7,
  W2120, W2180
};

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
  int     ImgWidth;
  int     ImgHeight;
  int     YScale;
  int     ColorEnc;
} ModeSpecDef;

extern ModeSpecDef ModeSpec[];

void               ClearPixbuf   (GdkPixbuf *, unsigned int, unsigned int);
void               createGUI     ();
int                GetVideo      (int, double, int, int);
unsigned int       GetBin        (double, int);
unsigned char      clip          (double);
void               setVU         (short int, double);
int                GetVIS        ();
double             FindSync      (unsigned int, int, double, int*);
double             deg2rad       (double);
void               initPcmDevice ();
void               delete_event  ();
void               GetAdaptive   ();
void               ManualStart   ();

#endif
