#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define SYNCW    350

extern int VISmap[128];

extern FILE      *PcmInStream;
extern short int PcmBuffer[2048];
extern double    *PCM;
extern int       PcmPointer;
extern int       Sample;
extern int       UseWav;
extern guchar    *rgbbuf;
extern int       maxpwr;
extern int       minpwr;
extern double    PowerAcc[2048];
extern double    MaxPower[2048];
extern double    *StoredFreq;
extern double    StoredFreqRate;
extern double    HedrShift;

extern GtkWidget *window;
extern GtkWidget *notebook;
extern GdkPixbuf *CamPixbuf;
extern GtkWidget *CamImage;
extern GtkWidget *statusbar;
extern GtkWidget *snrbar;
extern GtkWidget *pwrbar;
extern GdkPixmap *tmpixmap;
extern GdkPixbuf *VUpixbufActive;
extern GdkPixbuf *VUpixbufDim;
extern GdkPixbuf *VUpixbufSNR;
extern GtkWidget *VUimage[10];
extern GtkWidget *SNRimage[10];
extern GtkWidget *vutable;
extern GtkWidget *infolabel;

enum {
  M1,    M2,   M3,    M4,
  S1,    S2,   SDX,
  R72,   R36,  R24,   R24BW, R12BW, R8BW,
  PD50,  PD90, PD120, PD160, PD180, PD240, PD290,
  P3,    P5,   P7,
  W2120, W2180,
  UNKNOWN, AUTO_VIS
};

enum {
  GBR, RGB, YUV, BW
};

typedef struct ModeSpecDef {
  char    *Name;
  char    *ShortName;
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

void               ClearPixbuf(GdkPixbuf *, unsigned int, unsigned int);
void               createGUI  ();

int                GetVideo   (int, double, int, int, int, int);

unsigned int       GetBin     (double, int, double);
unsigned char      clip       (double);
void               setVU      (short int, double);

int                GetVIS     ();
double             FindSync   (unsigned int, int, double, int*);
double             deg2rad    (double);

#endif
