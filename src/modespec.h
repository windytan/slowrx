#ifndef MODESPEC_H_
#define MODESPEC_H_

#include <string>

enum SSTVMode {
 MODE_UNKNOWN=0,
 MODE_M1,    MODE_M2,    MODE_M3,    MODE_M4,    MODE_S1,
 MODE_S2,    MODE_SDX,   MODE_R72,   MODE_R36,   MODE_R24,
 MODE_R24BW, MODE_R12BW, MODE_R8BW,  MODE_PD50,  MODE_PD90,
 MODE_PD120, MODE_PD160, MODE_PD180, MODE_PD240, MODE_PD290,
 MODE_P3,    MODE_P5,    MODE_P7,    MODE_W260,  MODE_W2120,
 MODE_W2180
};

enum ModeFamily {
  MODE_MARTIN, MODE_SCOTTIE, MODE_ROBOT, MODE_PD, MODE_WRAASE2,
  MODE_PASOKON
};

enum eColorEnc {
  COLOR_GBR, COLOR_RGB, COLOR_YUV, COLOR_MONO
};

enum eVISParity {
  PARITY_EVEN=0, PARITY_ODD=1
};

typedef struct ModeSpec {
  std::string      name;
  std::string      short_name;
  double     t_sync;
  double     t_porch;
  double     t_sep;
  double     t_scan;
  double     t_period;
  unsigned   scan_pixels;
  unsigned   num_lines;
  unsigned   header_lines;
  unsigned   vis;
  eColorEnc  color_enc;
  ModeFamily family;
  eVISParity vis_parity;
} _ModeSpec;

extern _ModeSpec ModeSpec[];

#endif
