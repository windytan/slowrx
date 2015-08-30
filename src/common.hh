#ifndef _COMMON_H_
#define _COMMON_H_

// moment length only affects length of global delay, I/O interval,
// and maximum window size.
#define MOMENT_LEN     2047
#define FFT_LEN_SMALL  1024
#define FFT_LEN_BIG    2048
#define CIRBUF_LEN_FACTOR 4
#define CIRBUF_LEN ((MOMENT_LEN+1)*CIRBUF_LEN_FACTOR)
#define READ_CHUNK_LEN ((MOMENT_LEN+1)/2)

#include <iostream>
#include "portaudio.h"
#include "sndfile.hh"
#include "fftw3.h"
#include "gtkmm.h"

struct Point {
  int x;
  int y;
  explicit Point(int _x = 0, int _y=0) : x(_x), y(_y) {}
};

struct Tone {
  double dur;
  double freq;
  explicit Tone(double _dur = 0.0, double _freq=0.0) : dur(_dur), freq(_freq) {}
};

using Wave = std::vector<double>;
using Melody = std::vector<Tone>;

enum WindowType {
  WINDOW_CHEB47 = 0,
  WINDOW_HANN95,
  WINDOW_HANN127,
  WINDOW_HANN255,
  WINDOW_HANN511,
  WINDOW_HANN1023,
  WINDOW_HANN2047,
  WINDOW_HANN31,
  WINDOW_HANN63,
  WINDOW_SQUARE47
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

enum eStreamType {
  STREAM_TYPE_FILE, STREAM_TYPE_PA, STREAM_TYPE_STDIN
};

enum eVISParity {
  PARITY_EVEN=0, PARITY_ODD=1
};

extern std::map<int, SSTVMode> vis2mode;

typedef struct ModeSpec {
  std::string     Name;
  std::string     ShortName;
  double     tSync;
  double     tPorch;
  double     tSep;
  double     tScan;
  double     tLine;
  size_t     ScanPixels;
  size_t     NumLines;
  size_t     HeaderLines;
  eColorEnc  ColorEnc;
  eSyncOrder SyncOrder;
  eSubSamp   SubSampling;
  eVISParity VISParity;
} _ModeSpec;

extern _ModeSpec ModeSpec[];


struct Picture {
  SSTVMode mode;
  Wave video_signal;
  double video_dt;
  std::vector<bool> sync_signal;
  double sync_dt;
  double speed;
  double starts_at;
  explicit Picture(SSTVMode _mode)
    : mode(_mode), video_dt(ModeSpec[_mode].tScan/ModeSpec[_mode].ScanPixels/2),
      sync_dt(ModeSpec[_mode].tLine / ModeSpec[_mode].ScanPixels/3), speed(1) {}
};

class DSPworker {

  public:

    DSPworker();

    void openAudioFile (std::string);
    void openPortAudio ();
    void readMore ();
    double forward (unsigned);
    double forward ();
    double forward_time (double);
    void forward_to_time (double);
    void set_fshift (double);

    void windowedMoment (WindowType, fftw_complex *);
    double peakFreq (double, double, WindowType);
    int  freq2bin        (double, int);
    std::vector<double> bandPowerPerHz (std::vector<std::vector<double> >, WindowType wintype=WINDOW_HANN2047);
    WindowType bestWindowFor (SSTVMode, double SNR=99);
    double videoSNR();
    double lum(SSTVMode, bool is_adaptive=false);
    
    bool   is_open ();
    double get_t ();
    bool isLive();
    bool hasSync();

  private:

    mutable Glib::Threads::Mutex Mutex;
    short cirbuf_[CIRBUF_LEN * 2];
    int   cirbuf_head_;
    int   cirbuf_tail_;
    int   cirbuf_fill_count_;
    bool  please_stop_;
    short *read_buffer_;
    SndfileHandle file_;
    fftw_complex *fft_inbuf_;
    fftw_complex *fft_outbuf_;
    fftw_plan     fft_plan_small_;
    fftw_plan     fft_plan_big_;
    double  samplerate_;
    size_t num_chans_;
    PaStream *pa_stream_;
    eStreamType stream_type_;
    bool is_open_;
    double t_;
    double fshift_;
    double next_snr_time_;
    double SNR_;
    WindowType sync_window_;
    
    static std::vector<std::vector<double> > window_;
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


typedef struct {
  Point pt;
  int Channel;
  double Time;
} PixelSample;

std::vector<PixelSample> getPixelSamplingPoints(SSTVMode mode);

double   power         (fftw_complex coeff);
guint8   clip          (double a);
double   fclip         (double a);
void     createGUI     ();
double   deg2rad       (double Deg);
std::string   GetFSK        ();
bool     rxVideo      (SSTVMode Mode, DSPworker *dsp);
SSTVMode nextHeader       (DSPworker*);
int      initPcmDevice (std::string);
void     *Listen       ();
void     populateDeviceList ();
void     readPcm       (int numsamples);
void     saveCurrentPic();
void     setVU         (double *Power, int FFTLen, int WinIdx, bool ShowWin);
int      startGui      (int, char**);
void     resync  (Picture* pic);
double   gaussianPeak  (double y1, double y2, double y3);
std::tuple<bool,double,double> findMelody (Wave, Melody, double);
std::vector<int> readFSK (DSPworker*, double, double, double, size_t);
SSTVMode readVIS (DSPworker*, double fshift=0);
Wave upsampleLanczos    (Wave orig, int factor, size_t a=3);
Wave Hann    (std::size_t);
Wave Blackmann    (std::size_t);
Wave Rect    (std::size_t);
Wave Gauss    (std::size_t);

Wave deriv (Wave);
Wave peaks (Wave, size_t);
Wave derivPeaks (Wave, size_t);
Wave rms (Wave, int);
void runTest(const char*);

double complexMag (fftw_complex coeff);
guint8 freq2lum(double);

void renderPixbuf(Picture* pic);

void printWave(Wave, double);

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
