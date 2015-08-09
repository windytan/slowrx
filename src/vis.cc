#include "common.hh"
#include <cmath>

/* 
 *
 * Detect VIS & frequency shift
 *
 * Each bit lasts 30 ms (1323 samples)
 *
 */

SSTVMode GetVIS (DSPworker *dsp) {

  int        selmode, ptr=0;
  int        vis = 0, Parity = 0, HedrPtr = 0;
  double     HedrBuf[100] = {0}, tone[100] = {0};
  bool       gotvis = false;
  unsigned   Bit[8] = {0}, ParityBit = 0;

  //for (i = 0; i < FFTLen; i++) fft.in[i]    = 0;

  // Create 20ms Hann window
  //for (i = 0; i < 882; i++) Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / 881 ) );

  //ManualActivated = false;
  
  printf("Waiting for header\n");

  while ( dsp->is_still_listening() ) {

    //if (Abort || ManualResync) return(0);

    HedrBuf[HedrPtr] = dsp->getPeakFreq(500, 3300, WINDOW_HANN1023);
    dsp->forward_ms(10);

    // Header buffer holds 45 * 10 msec = 450 msec
    HedrPtr = (HedrPtr + 1) % 45;

    // Frequencies in the last 450 msec
    for (int i = 0; i < 45; i++) tone[i] = HedrBuf[(HedrPtr + i) % 45];

    // Is there a pattern that looks like (the end of) a calibration header + VIS?
    // Tolerance ±25 Hz
    CurrentPic.HedrShift = 0;
    gotvis    = false;
    for (int i = 0; i < 3; i++) {
      //if (CurrentPic.HedrShift != 0) break;
      for (int j = 0; j < 3; j++) {
        if ( (tone[1*3+i]  > tone[0+j] - 25  && tone[1*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[2*3+i]  > tone[0+j] - 25  && tone[2*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[3*3+i]  > tone[0+j] - 25  && tone[3*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[4*3+i]  > tone[0+j] - 25  && tone[4*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[5*3+i]  > tone[0+j] - 725 && tone[5*3+i]  < tone[0+j] - 675) && // 1200 Hz start bit
                                                                                   // ...8 VIS bits...
             (tone[14*3+i] > tone[0+j] - 725 && tone[14*3+i] < tone[0+j] - 675)    // 1200 Hz stop bit
           ) {

          // Attempt to read VIS

          gotvis = true;
          for (int k = 0; k < 8; k++) {
            if      (tone[6*3+i+3*k] > tone[0+j] - 625 && tone[6*3+i+3*k] < tone[0+j] - 575) Bit[k] = 0;
            else if (tone[6*3+i+3*k] > tone[0+j] - 825 && tone[6*3+i+3*k] < tone[0+j] - 775) Bit[k] = 1;
            else { // erroneous bit
              gotvis = false;
              break;
            }
          }
          if (gotvis) {
            CurrentPic.HedrShift = tone[0+j] - 1900;

            vis = Bit[0] + (Bit[1] << 1) + (Bit[2] << 2) + (Bit[3] << 3) + (Bit[4] << 4) +
                 (Bit[5] << 5) + (Bit[6] << 6);
            ParityBit = Bit[7];

            printf("  VIS %d (%02Xh) @ %+d Hz\n", vis, vis, CurrentPic.HedrShift);

            Parity = Bit[0] ^ Bit[1] ^ Bit[2] ^ Bit[3] ^ Bit[4] ^ Bit[5] ^ Bit[6];

            if (vis2mode.find(vis) == vis2mode.end()) {
              printf("  Unknown VIS\n");
              gotvis = false;
            } else if (Parity != ParityBit && vis2mode[vis] != MODE_R12BW) {
              printf("  Parity fail\n");
              gotvis = false;
            } else {

              //gtk_combo_box_set_active (GTK_COMBO_BOX(gui.combo_mode), VISmap[VIS]-1);
              //gtk_spin_button_set_value (GTK_SPIN_BUTTON(gui.spin_shift), CurrentPic.HedrShift);
              break;
            }
          }
        }
      }
      if (gotvis) break;
    }
    if (gotvis) break;

    //if (gotvis)
     //if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_rx))) break;

    // Manual start
    /*if (ManualActivated) {

      gtk_widget_set_sensitive( gui.frame_manual, false );
      gtk_widget_set_sensitive( gui.combo_card,   false );

      selmode   = gtk_combo_box_get_active (GTK_COMBO_BOX(gui.combo_mode)) + 1;
      CurrentPic.HedrShift = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(gui.spin_shift));
      VIS = 0;
      for (i=0; i<0x80; i++) {
        if (VISmap[i] == selmode) {
          VIS = i;
          break;
        }
      }

      break;
    }

    if (++ptr == 10) {
      setVU(Power, 2048, 6, false);
      ptr = 0;
    }*/

  }

  // Skip the rest of the stop bit
  dsp->forward_ms(12); // TODO hack
  
  if (vis2mode.find(vis) != vis2mode.end()) {
    return vis2mode[vis];
  }

  printf("  No VIS found\n");
  return MODE_UNKNOWN;
}
