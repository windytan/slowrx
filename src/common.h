#ifndef COMMON_H_
#define COMMON_H_

#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include <mutex>

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

template<class T> class CirBuffer {
  public:
    CirBuffer(int len);
    void moveHead(int n);
    int size() const;
    void append(const std::vector<T>& input_data, int n);
    void append(T input_element);
    void forward(int n);
    int getFillCount() const;
    T at(int n) const;

  private:
    std::vector<T> m_data;
    int  m_head;
    int  m_tail;
    int  m_fill_count;
    int  m_len;

};

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
  KERNEL_LANCZOS2, KERNEL_LANCZOS3, KERNEL_TENT, KERNEL_BOX
};

enum eStreamType {
  STREAM_TYPE_FILE, STREAM_TYPE_PA, STREAM_TYPE_STDIN
};

enum {
  BORDER_REPEAT, BORDER_ZERO
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
    double m_maxval;
    double m_val;
    int    m_width;
};

std::vector<PixelSample> pixelSamplingPoints(SSTVMode mode);

uint8_t  clipToByte          (double a);
double   fclip              (double a);
double   deg2rad       (double Deg);
std::string   GetFSK        ();
void     setVU         (double *Power, int FFTLen, int WinIdx, bool ShowWin);
std::tuple<bool,double,double> findMelody (const Wave&, const Melody&, double, double, double);

extern bool g_is_pa_initialized;

void runTest(const char*);

int maxIndex (std::vector<double>);

uint8_t freq2lum(double);

void printWave(Wave, double);


#endif
