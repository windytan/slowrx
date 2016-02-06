#ifndef MODESPEC_H_
#define MODESPEC_H_
#include <string>

/*enum {
 MODE_UNKNOWN=0,
 MODE_M1,    MODE_M2,    MODE_M3,    MODE_M4,    MODE_S1,
 MODE_S2,    MODE_SDX,   MODE_R72,   MODE_R36,   MODE_R24,
 MODE_R36BW, MODE_R24BW, MODE_R12BW, MODE_R8BW,  MODE_PD50,  MODE_PD90,
 MODE_PD120, MODE_PD160, MODE_PD180, MODE_PD240, MODE_PD290,
 MODE_P3,    MODE_P5,    MODE_P7,    MODE_W260,  MODE_W2120,
 MODE_W2180
};*/

enum ModeFamily {
  MODE_MARTIN, MODE_SCOTTIE, MODE_ROBOT, MODE_ROBOTBW, MODE_PD, MODE_WRAASE2,
  MODE_PASOKON
};

enum eColorEnc {
  COLOR_GBR, COLOR_RGB, COLOR_YUV422, COLOR_YUV420, COLOR_MONO
};

enum eVISParity {
  PARITY_EVEN=0, PARITY_ODD=1
};

struct ModeSpec {
  std::string name;
  int         scan_pixels;
  int         num_lines;
  int         header_lines;
  double      aspect_ratio;
  double      t_sync;
  double      t_porch;
  double      t_sep;
  double      t_chanporch;
  double      t_scan;
  double      t_period;
  ModeFamily  family;
  eColorEnc   color_enc;
  uint16_t    vis;
  eVISParity  vis_parity;

  ModeSpec() {};

  ModeSpec(
      std::string nm, uint16_t sp, uint16_t nl, uint16_t hl,
      double t_sy, double t_po, double t_se, double t_chp, double t_pe,
      double t_sc, ModeFamily fa, eColorEnc ce, uint16_t vi,  eVISParity vp
    ) :
    name(nm), scan_pixels(sp), num_lines(nl), header_lines(hl),
    aspect_ratio(4.0/3.0), t_sync(t_sy*1e-3), t_porch(t_po*1e-3), t_sep(t_se*1e-3), t_chanporch(t_chp*1e-3),
    t_scan(t_sc*1e-3), t_period(t_pe*1e-3), family(fa), color_enc(ce), vis(vi),
    vis_parity(vp)
  {};
};

std::pair<bool,ModeSpec> vis2mode (uint16_t);

#endif
