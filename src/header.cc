#include "common.hh"
#include <cmath>


/* pass:     wave      FM demodulated signal
 *           melody    array of Tones
 *                     (zero frequency to accept any)
 *           dt        sample delta
 *
 * returns:  (bool)    did we find it
 *           (double)  at which frequency shift
 *           (int)     starting at how many dt from beginning of buffer
 */
std::tuple<bool,double,int> findMelody (Wave wave, Melody melody, double dt) {
  bool   was_found = true;
  int    start = 0;
  double avg_fdiff = 0;
  double freq_margin = 25;
  double t = melody[melody.size()-1].dur;
  std::vector<double> fdiffs;

  for (int i=melody.size()-2; i>=0; i--)  {
    if (melody[i].freq != 0) {
      double delta_f_ref = melody[i].freq - melody[melody.size()-1].freq;
      double delta_f     = wave[wave.size()-1 - (t/dt)] - wave[wave.size()-1];
      double fshift       = delta_f - delta_f_ref;
      was_found &= fabs(fshift) < freq_margin;
    }
    start = wave.size() - (t / dt);
    t += melody[i].dur;
  }

  if (was_found) {
    int melody_i = 0;
    double next_tone_t = melody[melody_i].dur;
    for (int i=start; i<wave.size(); i++) {
      double fref = melody[melody_i].freq;
      double fdiff = (wave[i] - fref);
      fdiffs.push_back(fdiff);
      if ( (i-start)*dt >= next_tone_t ) {
        melody_i ++;
        next_tone_t += melody[melody_i].dur;
      }
    }
    std::sort(fdiffs.begin(), fdiffs.end());
    avg_fdiff = fdiffs[fdiffs.size()/2];
  }


  return { was_found, avg_fdiff, start };
}



SSTVMode modeFromNextHeader (DSPworker *dsp) {

  double dt = 5e-3;
  int bitlen = 30e-3 / dt;
  int upsample_factor = 10;

  int        selmode, ptr=0;
  int        vis = 0, Parity = 0, ReadPtr = 0;
  Wave HedrCirBuf(1100e-3 / dt);
  Wave delta_f(1100e-3 / dt);
  Wave freq(1100e-3 / dt);
  Wave tone(1100e-3 / dt);
  bool       gotvis = false;
  unsigned   Bit[8] = {0}, ParityBit = 0;

  Melody mmsstv_melody = {
    Tone(100e-3, 1900), Tone(100e-3, 1500), Tone(100e-3, 1900), Tone(100e-3, 1500),
    Tone(100e-3, 2300), Tone(100e-3, 1500), Tone(100e-3, 2300), Tone(100e-3, 1500),
    Tone(300e-3, 1900)
  };

  Melody robot_melody = {
    Tone(150e-3, 1900), Tone(150e-3, 1900), Tone(160e-3, 1900), Tone(150e-3, 1900),
    Tone(30e-3,  1200), Tone(8*30e-3,0),    Tone(30e-3,  1200)
  };

  fprintf(stderr,"Waiting for header\n");

  while ( dsp->is_open() ) {

    HedrCirBuf[ReadPtr] = dsp->peakFreq(500, 3300, WINDOW_HANN1023);

    for (int i = 0; i < HedrCirBuf.size(); i++) {
      freq[i] = HedrCirBuf[(ReadPtr + 1 + i) % HedrCirBuf.size()];
    }

    double fshift;
    std::tuple<bool,double,int> has = findMelody(freq, mmsstv_melody, dt);
    if (std::get<0>(has)) {
      fshift = std::get<1>(has);
      int start = std::get<2>(has);
      fprintf(stderr,"  got MMSSTV header (t=%f s, fshift=%f Hz, start at %d)\n",dsp->get_t(), fshift, start);

      Wave powers = rms(deriv(freq), 6);
      for (int i=0; i<powers.size(); i++) {

      }
      
      Wave header_interp = upsampleLanczos(freq, upsample_factor, 1900);
      std::vector<double> peaks_pos = derivPeaks(header_interp, 8);

      printWave(freq, dt);

      exit(0);
    }

    if (false) {
      double fshift = ((delta_f[0] - 1900) + delta_f[4*bitlen] + delta_f[6*bitlen] + delta_f[8*bitlen] +
        delta_f[10*bitlen]) / 5.0;

      printf("  got robot header: t=%f, fshift=%+.1f Hz\n",dsp->get_t(),fshift);

      // hi-res zero-crossing search
      Wave abs_f(delta_f.size());
      abs_f[0] = delta_f[0];
      for (int i=1; i<abs_f.size(); i++)
        abs_f[i] += delta_f[0] + delta_f[i];
      Wave header_interp = upsampleLanczos(abs_f, upsample_factor);

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
          if (n_zc > 1 && t > t_zc1+270e-3 ) {
            t_zc2 = t - dt + ((-s0)/(s-s0))*dt/upsample_factor;
            break;
          }
          n_zc++;

        }
        s0 = s;
      }

      printf ("    n_zc=%d, t_zc1=%f, t_zc2=%fi (dur = %.05f ms, speed = %.5f)\n",n_zc,t_zc1,t_zc2,
          (t_zc2-t_zc1)*1e3,
          300e-3 / (t_zc2 - t_zc1)
          );

      int parity_rx=0;
      for (int k=0; k<8; k++) {
        if (abs_f[(13+k)*bitlen]+fshift < 1200) {
          vis |= (1 << k);
          parity_rx ++;
        }
      }
      vis &= 0x7f;

      printf("  got VIS: %dd (%02Xh) @ %+f Hz\n", vis, vis, fshift);
      if (vis2mode.find(vis) == vis2mode.end()) {
        printf("    Unknown VIS\n");
        gotvis = false;
      } else {
        if ((parity_rx % 2) != ModeSpec[vis2mode[vis]].VISParity) {
          printf("    Parity fail\n");
          gotvis = false;
        } else {
          printf("    %s\n",ModeSpec[vis2mode[vis]].Name.c_str());
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
