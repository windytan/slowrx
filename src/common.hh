#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT   30
#define MAXSLANT   150
#define CIRBUF_LEN 4096
#define READ_CHUNK_LEN 1024

// moment length only affects length of delay and maximum window size.
#define MOMENT_LEN 1023
#define SYNCPIXLEN 1.5e-3

#include <iostream>
#include "portaudio.h"
#include "sndfile.hh"
#include "fftw3.h"
#include "gtkmm.h"

enum WindowType {
  WINDOW_HANN47 = 0,
  WINDOW_HANN63,
  WINDOW_HANN95,
  WINDOW_HANN127,
  WINDOW_HANN255,
  WINDOW_HANN511,
  WINDOW_HANN1023,
  WINDOW_CHEB47
};

enum SSTVMode {
 MODE_UNKNOWN=0,
 MODE_M1,    MODE_M2,    MODE_M3,    MODE_M4,    MODE_S1,
 MODE_S2,    MODE_SDX,   MODE_R72,   MODE_R36,   MODE_R24,
 MODE_R24BW, MODE_R12BW, MODE_R8BW,  MODE_PD50,  MODE_PD90,
 MODE_PD120, MODE_PD160, MODE_PD180, MODE_PD240, MODE_PD290,
 MODE_P3,    MODE_P5,    MODE_P7,    MODE_W2120, MODE_W2180
};

enum eColorEnc {
  COLOR_GBR, COLOR_RGB, COLOR_YUV, COLOR_MONO
};

enum eSyncOrder {
  SYNC_SIMPLE, SYNC_SCOTTIE
};

enum eSubSamp {
  SUBSAMP_444, SUBSAMP_422_YUV, SUBSAMP_420_YUYV, SUBSAMP_440_YUVY
};

enum {
  STREAM_FILE, STREAM_PA
};

extern std::map<int, SSTVMode> vis2mode;

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
  bool   BufferDrop;
};
//extern PcmData pcm;


class DSPworker {

  public:

    DSPworker();

    void openAudioFile(std::string);
    double forward(unsigned);
    double forward();
    double forward_ms(double);
    void getWindowedMoment(WindowType, double *);
    double getPeakFreq (double, double, WindowType);
    int  getBin        (double);
    double getFourierPower (fftw_complex coeff);
    bool is_still_listening ();
    std::vector<double> getBandPowerPerHz(std::vector<std::vector<double> >);
    WindowType getBestWindowFor(SSTVMode, double);
    WindowType getBestWindowFor(SSTVMode);
    void readMore();
    void openPortAudio();

    static short win_lens_[8];
    static double window_[8][1024];

  private:

    mutable Glib::Threads::Mutex Mutex;
    short cirbuf_[CIRBUF_LEN * 2];
    int   cirbuf_head_;
    int   cirbuf_tail_;
    int   cirbuf_fill_count_;
    bool  please_stop_;
    SndfileHandle file_;
    int fft_len_;
    double *fft_inbuf_;
    fftw_complex *fft_outbuf_;
    fftw_plan     fft_plan_;
    int  samplerate_;
    bool is_still_listening_;
    PaStream *pa_stream_;
    int stream_type_;
};

class SlowGUI {

  public:

    SlowGUI();

    void ping();

  private:

    Gtk::Button *button_abort;
    Gtk::Button *button_browse;
    Gtk::Button *button_clear;
    Gtk::Button *button_start;
    Gtk::ComboBoxText *combo_card;
    Gtk::ComboBox *combo_mode;
    Gtk::Entry *entry_picdir;
    Gtk::EventBox *eventbox_img;
    Gtk::Frame *frame_manual;
    Gtk::Frame *frame_slant;
    Gtk::Grid *grid_vu;
    Gtk::IconView *iconview;
    Gtk::Image *image_devstatus;
    Gtk::Image *image_pwr;
    Gtk::Image *image_rx;
    Gtk::Image *image_snr;
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

extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_PWR;
extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_SNR;
extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
extern Glib::RefPtr<Gdk::Pixbuf> pixbuf_disp;

extern Gtk::ListStore *savedstore;

extern Glib::KeyFile config;


typedef struct _PicMeta PicMeta;
struct _PicMeta {
  int    HedrShift;
  int    Mode;
  double Rate;
  int    Skip;
  Gdk::Pixbuf *thumbbuf;
  char   timestr[40];
};
extern PicMeta CurrentPic;


typedef struct ModeSpec {
  std::string     Name;
  std::string     ShortName;
  double     tSync;
  double     tPorch;
  double     tSep;
  double     tScan;
  double     tLine;
  int        ScanPixels;
  int        NumLines;
  int        HeaderLines;
  eColorEnc  ColorEnc;
  eSyncOrder SyncOrder;
  eSubSamp   SubSampling;
} _ModeSpec;

extern _ModeSpec ModeSpec[];

double   power         (fftw_complex coeff);
int      clip          (double a);
void     createGUI     ();
double   deg2rad       (double Deg);
double   FindSync      (SSTVMode Mode, double Rate, int *Skip);
std::string   GetFSK        ();
bool     GetVideo      (SSTVMode Mode, DSPworker *dsp);
SSTVMode GetVIS        (DSPworker*);
int      initPcmDevice (std::string);
void     *Listen       ();
void     populateDeviceList ();
void     readPcm       (int numsamples);
void     saveCurrentPic();
void     setVU         (double *Power, int FFTLen, int WinIdx, bool ShowWin);
int      startGui      (int, char**);

void     evt_AbortRx       ();
void     evt_changeDevices ();
void     evt_chooseDir     ();
void     evt_clearPix      ();
void     evt_clickimg      (Gtk::Widget*, GdkEventButton*, Gdk::WindowEdge);
void     evt_deletewindow  ();
void     evt_GetAdaptive   ();
void     evt_ManualStart   ();
void     evt_show_about    ();


class MyPortaudioClass{

  int myMemberCallback(const void *input, void *output,
      unsigned long frameCount,
      const PaStreamCallbackTimeInfo* timeInfo,
      PaStreamCallbackFlags statusFlags);

  static int myPaCallback(
      const void *input, void *output,
      unsigned long frameCount,
      const PaStreamCallbackTimeInfo* timeInfo,
      PaStreamCallbackFlags statusFlags,
      void *userData ) {
      return ((MyPortaudioClass*)userData)
             ->myMemberCallback(input, output, frameCount, timeInfo, statusFlags);
  }
};

#endif
