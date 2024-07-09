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

// Various callback function types for various events.
typedef void (*EventCallback)(void);
typedef void (*TextStatusCallback)(const char* status);

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
guint    GetBin        (double Freq, guint FFTLen);

void     ensure_dir_exists(const char *dir);

#endif
