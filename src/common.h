#ifndef _COMMON_H_
#define _COMMON_H_

#include <iostream>
#include <cassert>

#include <vector>

#include "modespec.h"

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

class DSPworker;

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

enum {
  KERNEL_LANCZOS2, KERNEL_LANCZOS3, KERNEL_TENT
};

enum eStreamType {
  STREAM_TYPE_FILE, STREAM_TYPE_PA, STREAM_TYPE_STDIN
};


typedef struct {
  Point pt;
  int ch;
  double t;
  bool exists;
} PixelSample;

std::vector<PixelSample> pixelSamplingPoints(SSTVMode mode);

uint8_t  clip          (double a);
double   fclip         (double a);
void     createGUI     ();
double   deg2rad       (double Deg);
std::string   GetFSK        ();
bool     rxVideo      (SSTVMode Mode, DSPworker *dsp);
SSTVMode nextHeader       (DSPworker*);
int      initPcmDevice (std::string);
void     populateDeviceList ();
void     readPcm       (int numsamples);
void     saveCurrentPic();
void     setVU         (double *Power, int FFTLen, int WinIdx, bool ShowWin);
int      startGui      (int, char**);
std::tuple<bool,double,double> findMelody (Wave, Melody, double, double, double);
SSTVMode readVIS (DSPworker*, double fshift=0);
Wave* upsample    (Wave orig, size_t factor, int kern_type);

SSTVMode vis2mode (int);

void runTest(const char*);

size_t maxIndex (std::vector<double>);

uint8_t freq2lum(double);

void printWave(Wave, double);


#endif
