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
} PixelSample;

class ProgressBar {
  public:
    ProgressBar(double maxval, int width=50);

    void set(double val);
    void finish();
  private:
    double maxval_;
    double val_;
    double width_;
};

std::vector<PixelSample> pixelSamplingPoints(SSTVMode mode);

uint8_t  clip          (double a);
double   fclip         (double a);
double   deg2rad       (double Deg);
std::string   GetFSK        ();
void     setVU         (double *Power, int FFTLen, int WinIdx, bool ShowWin);
std::tuple<bool,double,double> findMelody (const Wave&, const Melody&, double, double, double);

SSTVMode vis2mode (int);

void runTest(const char*);

size_t maxIndex (std::vector<double>);

uint8_t freq2lum(double);

void printWave(Wave, double);


#endif
