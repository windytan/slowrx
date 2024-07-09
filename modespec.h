#ifndef _MODESPEC_H_
#define _MODESPEC_H_

#include <stdint.h>

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

extern const uint8_t VISmap[];

typedef struct ModeSpec {
  char    *Name;
  char    *ShortName;
  double   SyncTime;
  double   PorchTime;
  double   SeptrTime;
  double   PixelTime;
  double   LineTime;
  uint16_t ImgWidth;
  uint16_t NumLines;
  uint8_t  LineHeight;
  uint8_t  ColorEnc;
} _ModeSpec;

extern const _ModeSpec ModeSpec[];

#endif
