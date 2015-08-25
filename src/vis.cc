#include "common.hh"
#include <cmath>

/* 
 *
 * Detect Robot header & VIS
 *
 */

SSTVMode GetVIS (DSPworker *dsp) {

  double dt = 5e-3;
  int bitlen = 30e-3 / dt;
  int upsample_factor = 10;

  int        selmode, ptr=0;
  int        vis = 0, Parity = 0, ReadPtr = 0;
  std::vector<double> HedrCirBuf(bitlen*22);
  std::vector<double> delta_f(bitlen*22);
  std::vector<double> tone(bitlen*22);
  bool       gotvis = false;
  unsigned   Bit[8] = {0}, ParityBit = 0;


  printf("bitlen=%d\n",bitlen);

  printf("Waiting for header\n");

  while ( dsp->is_open() ) {

    HedrCirBuf[ReadPtr] = dsp->peakFreq(500, 3300, WINDOW_HANN1023);

    //printf("%f,%f\n",dsp->get_t(),HedrCirBuf[ReadPtr]);

    delta_f[0] = HedrCirBuf[(ReadPtr+1) % HedrCirBuf.size()];
    for (int i = 1; i < HedrCirBuf.size(); i++) {
      delta_f[i] = HedrCirBuf[(ReadPtr + 1 + i) % HedrCirBuf.size()] - delta_f[0];
    }

    // Is there a pattern that looks like (the end of) a calibration header + VIS?
    CurrentPic.HedrShift = 0;
    gotvis    = false;
    double freq_margin = 25;

    if ( fabs(delta_f[4*bitlen]) < freq_margin && fabs(delta_f[6*bitlen]) < freq_margin &&
         fabs(delta_f[8*bitlen]) < freq_margin && fabs(delta_f[10*bitlen]) < freq_margin &&
         fabs(delta_f[12*bitlen]+(1900-1200)) < freq_margin &&
         fabs(delta_f[21*bitlen]+(1900-1200)) < freq_margin ) {

      double fshift = ((delta_f[0] - 1900) + delta_f[4*bitlen] + delta_f[6*bitlen] + delta_f[8*bitlen] +
        delta_f[10*bitlen]) / 5.0;

      printf("header! t=%f, fshift=%+.1f Hz\n",dsp->get_t(),fshift);

      // hi-res zero-crossing search
      std::vector<double> abs_f(delta_f.size());
      for (int i=1; i<abs_f.size(); i++)
        abs_f[i] += delta_f[0] + delta_f[i];
      std::vector<double> header_interp = upsampleLanczos(abs_f, upsample_factor);

      int n_zc = 0;
      double t_zc1=0, t_zc2=0;
      double s=0,s0=0;
      for (int i=0; i<header_interp.size(); i++) {
        s = header_interp[i] - fshift - (1200.0+(1900.0-1200.0)/2.0);
        double t = dsp->get_t() - (header_interp.size()-1-i)*dt/upsample_factor;
        //printf("%f,%f\n",t,s);
        if (i*dt/upsample_factor>15e-3 && s * s0 < 0) {
          if (n_zc == 1) {
            t_zc1 = t - dt + ((-s0)/(s-s0))*dt/upsample_factor;
          }
          if (n_zc == 2) {
            t_zc2 = t - dt + ((-s0)/(s-s0))*dt/upsample_factor;
            break;
          }
          n_zc++;

        }
        s0 = s;
      }

      /*printf ("n_zc=%d, t_zc1=%f, t_zc2=%fi (dur = %.05f ms, error = %.03f%%)\n",n_zc,t_zc1,t_zc2,
          (t_zc2-t_zc1)*1e3,
          (t_zc2 - t_zc1) / 300e-3 * 100 - 100
          );*/

      int parity_rx=0;
      for (int k=0; k<8; k++) {
        if (abs_f[(13+k)*bitlen]+fshift < 1200) {
          vis |= (1 << k);
          parity_rx ++;
        }
      }
      vis &= 0x7f;

      printf("VIS: %dd (%02Xh) @ %+f Hz\n", vis, vis, fshift);
      if (vis2mode.find(vis) == vis2mode.end()) {
        printf("Unknown VIS\n");
        gotvis = false;
      } else {
        if ((parity_rx % 2) != ModeSpec[vis2mode[vis]].VISParity) {
          printf("Parity fail\n");
          gotvis = false;
        } else {
          printf("%s\n",ModeSpec[vis2mode[vis]].Name.c_str());
          double start_of_video = t_zc2 + 10 * 30e-3;
          dsp->forward_to_time(start_of_video);
          break;
        }
      }
    }
    ReadPtr = (ReadPtr+1) % HedrCirBuf.size();
    dsp->forward_time(dt);
  }

  return vis2mode[vis];

}
