#ifndef _MODESPEC_H_
#define _MODESPEC_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   4096
#define SYNCPIXLEN 1.5e-3

extern guchar     VISmap[];

typedef struct ModeSpec {
  char   *Name;
  char   *ShortName;
  double  SyncTime;
  double  PorchTime;
  double  SeptrTime;
  double  PixelTime;
  double  LineTime;
  gushort ImgWidth;
  gushort NumLines;
  guchar  LineHeight;
  guchar  ColorEnc;
} _ModeSpec;

extern _ModeSpec ModeSpec[];

#endif
