#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   4096
#define SYNCPIXLEN 1.5e-3

typedef struct _FFTStuff FFTStuff;
struct _FFTStuff {
  double       *in;
  fftw_complex *out;
  fftw_plan     Plan1024;
  fftw_plan     Plan2048;
};
extern FFTStuff fft;

typedef struct _PcmData PcmData;
struct _PcmData {
//  snd_pcm_t *handle;
  void      *handle;
  gint16    *Buffer;
  int        WindowPtr;
  gboolean   BufferDrop;
};
//extern PcmData pcm;

typedef struct _GuiObjs GuiObjs;
struct _GuiObjs {
  Gtk::Button *button_abort;
  Gtk::Button *button_browse;
  Gtk::Button *button_clear;
  Gtk::Button *button_start;
  Gtk::ComboBox *combo_card;
  Gtk::ComboBox *combo_mode;
  Gtk::Widget *entry_picdir;
  Gtk::Widget *eventbox_img;
  Gtk::Widget *frame_manual;
  Gtk::Widget *frame_slant;
  Gtk::Widget *grid_vu;
  Gtk::Widget *iconview;
  Gtk::Widget *image_devstatus;
  Gtk::Widget *image_pwr;
  Gtk::Widget *image_rx;
  Gtk::Widget *image_snr;
  Gtk::Label *label_fskid;
  Gtk::Label *label_lastmode;
  Gtk::Label *label_utc;
  Gtk::MenuItem *menuitem_about;
  Gtk::MenuItem *menuitem_quit;
  Gtk::SpinButton *spin_shift;
  Gtk::Widget *statusbar;
  Gtk::ToggleButton *tog_adapt;
  Gtk::ToggleButton *tog_fsk;
  Gtk::ToggleButton *tog_rx;
  Gtk::ToggleButton *tog_save;
  Gtk::ToggleButton *tog_setedge;
  Gtk::ToggleButton *tog_slant;
  Gtk::Window *window_about;
  Gtk::Window *window_main;
};
extern GuiObjs   gui;

/*
extern GdkPixbuf *pixbuf_PWR;
extern GdkPixbuf *pixbuf_SNR;
extern GdkPixbuf *pixbuf_rx;
extern GdkPixbuf *pixbuf_disp;

extern GtkListStore *savedstore;

extern GKeyFile  *config;*/


typedef struct _PicMeta PicMeta;
struct _PicMeta {
  int    HedrShift;
  int    Mode;
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
  std::string  Name;
  std::string  ShortName;
  double  SyncTime;
  double  PorchTime;
  double  SeptrTime;
  double  PixelTime;
  double  LineTime;
  int     ImgWidth;
  int     NumLines;
  int     LineHeight;
  int     ColorEnc;
} _ModeSpec;

extern _ModeSpec ModeSpec[];

double   power         (fftw_complex coeff);
int      clip          (double a);
void     createGUI     ();
double   deg2rad       (double Deg);
double   FindSync      (int Mode, double Rate, int *Skip);
void     GetFSK        (char *dest);
bool     GetVideo      (int Mode, double Rate, int Skip, bool Redraw);
int      GetVIS        ();
int      GetBin        (double Freq, int FFTLen);
int      initPcmDevice (char *);
void     *Listen       ();
void     populateDeviceList ();
void     readPcm       (int numsamples);
void     saveCurrentPic();
void     setVU         (double *Power, int FFTLen, int WinIdx, bool ShowWin);

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
