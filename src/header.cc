#include "common.h"
#include "dsp.h"

#include <cmath>

SSTVMode nextHeader (DSPworker *dsp) {

  double dt = 5e-3;
  SSTVMode mode = MODE_UNKNOWN;

  int        ptr_read = 0;
  Wave cirbuf_header(1100e-3 / dt);
  Wave cirbuf_header_bb(1100e-3 / dt);
  Wave freq(1100e-3 / dt);
  Wave freq_bb(1100e-3 / dt);

  Melody mmsstv_vox = {
    Tone(100e-3, 1900), Tone(100e-3, 1500), Tone(100e-3, 1900), Tone(100e-3, 1500),
    Tone(100e-3, 2300), Tone(100e-3, 1500), Tone(100e-3, 2300), Tone(100e-3, 1500),
    Tone(300e-3, 1900)
  };

  Melody robot_vox = {
    Tone(300e-3, 1900), Tone(10e-3,  1200), Tone(300e-3, 1900),
    Tone(30e-3,  1200)
  };

  fprintf(stderr,"wait for header\n");

  while ( dsp->is_open() ) {

    cirbuf_header[ptr_read]     = dsp->peakFreq(FREQ_MIN, FREQ_MAX, WINDOW_HANN1023);

    for (size_t i = 0; i < cirbuf_header.size(); i++) {
      freq[i]     = cirbuf_header[(ptr_read + 1 + i) % cirbuf_header.size()];
    }

    double fshift;
    std::tuple<bool,double,double> has = findMelody(freq, mmsstv_vox, dt, -800, 800);
    if (std::get<0>(has)) {
      fshift = std::get<1>(has);
      double tshift = std::get<2>(has);
      fprintf(stderr,"  got MMSSTV VOX (t=%.03f s, fshift=%+.0f Hz)\n",
          dsp->get_t()+tshift, fshift);

      dsp->forward_time(tshift + 8*100e-3 + 300e-3 + 10e-3 + 300e-3 + 30e-3);

      mode = readVIS(dsp, fshift);

      if (mode != MODE_UNKNOWN) {
        dsp->set_fshift(fshift);
        dsp->forward_time(30e-3);
        break;
      }
    }

    has = findMelody(freq, robot_vox, dt, -800, 800);
    if (std::get<0>(has)) {
      fshift = std::get<1>(has);
      double tshift = std::get<2>(has);

      fprintf(stderr,"  got Robot header (t=%f s, fshift=%+.1f Hz)\n",dsp->get_t(),fshift);

      dsp->forward_time(tshift + 300e-3 + 10e-3 + 300e-3 + 30e-3);

      mode = readVIS(dsp, fshift);

      if (mode != MODE_UNKNOWN) {
        dsp->set_fshift(fshift);
        dsp->forward_time(30e-3);
        break;
      }

    }
    ptr_read = (ptr_read+1) % cirbuf_header.size();
    dsp->forward_time(dt);
  }

  return mode;

}

SSTVMode readVIS(DSPworker* dsp, double fshift) {

  unsigned vis = 0;
  SSTVMode mode = MODE_UNKNOWN;
  std::vector<int> bits = readFSK(dsp, 33.333, 1200+fshift, 100, 8);
  int parity_rx=0;
  for (int i=0; i<8; i++) {
    vis |= (bits[i] << i);
    parity_rx += bits[i];
  }
  vis &= 0x7F;

  mode = vis2mode(vis);

  if (mode == MODE_UNKNOWN) {
    fprintf(stderr,"(unknown mode %dd=%02Xh)\n",vis,vis);
  } else {
    if ((parity_rx % 2) != ModeSpec[mode].vis_parity) {
      fprintf(stderr,"(parity fail)\n");
    } else {
      fprintf(stderr,"  got VIS: %dd / %02Xh (%s)", vis, vis,
          ModeSpec[mode].name.c_str());
    }
  }

  return mode;
}

