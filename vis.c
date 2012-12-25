#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#ifdef GPL
#include <fftw3.h>
#endif

#include "common.h"

/* 
 *
 * Detect VIS & frequency shift
 *
 * Each bit lasts 30 ms (1323 samples)
 *
 */

guchar GetVIS () {

  int        selmode, ptr=0;
  //int        Pointer = 0;
  int        VIS = 0, Parity = 0, HedrPtr = 0;
  //gushort    MaxPcm = 0;
  guint      FFTLen = 2048, i=0, j=0, k=0, MaxBin = 0;
  double     Power[2048] = {0}, HedrBuf[100] = {0}, tone[100] = {0}, Hann[882] = {0};
  char       infostr[60] = {0};
  bool       gotvis = false;
  guchar     Bit[8] = {0}, ParityBit = 0;

  for (i = 0; i < FFTLen; i++) in[i]    = 0;

  // Create 20ms Hann window
  for (i = 0; i < 882; i++) Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / 881 ) );

  ManualActivated = false;
  
  printf("Waiting for header\n");

  gdk_threads_enter();
  gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Ready" );
  gdk_threads_leave();

  while ( true ) {

    // Read 10 ms from sound card
    readPcm(441);

    // Apply Hann window
    for (i = 0; i < 882; i++) in[i] = PcmBuffer[PcmPointer + i - 441] / 32768.0 * Hann[i];

    // FFT of last 20 ms
    fftw_execute(Plan2048);

    // Find the bin with most power
    MaxBin = 0;
    for (i = GetBin(500, FFTLen); i <= GetBin(3300, FFTLen); i++) {
      Power[i] = pow(out[i], 2) + pow(out[FFTLen - i], 2);
      if (Power[i] > Power[MaxBin] || MaxBin == 0) MaxBin = i;
    }

    // Find the peak frequency by Gaussian interpolation
    if (MaxBin > GetBin(500, FFTLen) && MaxBin < GetBin(3300, FFTLen))
         HedrBuf[HedrPtr] = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                             (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
    else HedrBuf[HedrPtr] = HedrBuf[(HedrPtr-1) % 45];

    // In Hertz
    HedrBuf[HedrPtr] = HedrBuf[HedrPtr] / FFTLen * 44100;

    // Header buffer holds 45 * 10 msec = 450 msec
    HedrPtr = (HedrPtr + 1) % 45;

    // Frequencies in the last 450 msec
    for (i = 0; i < 45; i++) tone[i] = HedrBuf[(HedrPtr + i) % 45];

    // Is there a pattern that looks like (the end of) a calibration header + VIS?
    // Tolerance ±25 Hz
    HedrShift = 0;
    gotvis    = false;
    for (i = 0; i < 3; i++) {
      if (HedrShift != 0) break;
      for (j = 0; j < 3; j++) {
        if ( (tone[1*3+i]  > tone[0+j] - 25  && tone[1*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[2*3+i]  > tone[0+j] - 25  && tone[2*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[3*3+i]  > tone[0+j] - 25  && tone[3*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[4*3+i]  > tone[0+j] - 25  && tone[4*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[5*3+i]  > tone[0+j] - 725 && tone[5*3+i]  < tone[0+j] - 675) && // 1200 Hz start bit
                                                                                   // ...8 VIS bits...
             (tone[14*3+i] > tone[0+j] - 725 && tone[14*3+i] < tone[0+j] - 675)    // 1200 Hz stop bit
           ) {

          printf("Possible header @ %+.0f Hz\n",tone[0+j]-1900);

          // Attempt to read VIS

          gotvis = true;
          for (k = 0; k < 8; k++) {
            if      (tone[6*3+i+3*k] > tone[0+j] - 625 && tone[6*3+i+3*k] < tone[0+j] - 575) Bit[k] = 0;
            else if (tone[6*3+i+3*k] > tone[0+j] - 825 && tone[6*3+i+3*k] < tone[0+j] - 775) Bit[k] = 1;
            else { // erroneous bit
              gotvis = false;
              break;
            }
          }
          if (gotvis) {
            HedrShift = tone[0+j] - 1900;

            VIS = Bit[0] + (Bit[1] << 1) + (Bit[2] << 2) + (Bit[3] << 3) + (Bit[4] << 4) +
                 (Bit[5] << 5) + (Bit[6] << 6);
            ParityBit = Bit[7];

            printf("  VIS %d (%02Xh) @ %d Hz\n", VIS, VIS, HedrShift);

            Parity = Bit[0] ^ Bit[1] ^ Bit[2] ^ Bit[3] ^ Bit[4] ^ Bit[5] ^ Bit[6];

            if (VISmap[VIS] == R12BW) Parity = !Parity;

            if (Parity != ParityBit) {
              printf("  Parity fail\n");
              gotvis = false;
            } else if (VISmap[VIS] == UNKNOWN) {
              printf("  Unknown VIS\n");
              snprintf(infostr, sizeof(infostr)-1, "How to decode image with VIS %d (%02Xh)?", VIS, VIS);
              gotvis = false;
              gdk_threads_enter();
              gtk_label_set_markup(GTK_LABEL(infolabel), infostr);
              gdk_threads_leave();
            } else {
              gdk_threads_enter();
              gtk_combo_box_set_active (GTK_COMBO_BOX(modecombo), VISmap[VIS]-1);
              gtk_spin_button_set_value (GTK_SPIN_BUTTON(shiftspin), HedrShift);
              gdk_threads_leave();
              break;
            }
          }
        }
      }
    }

    if (gotvis)
     if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togrx))) break;

    // Manual start
    if (ManualActivated) {

      gdk_threads_enter();
      gtk_widget_set_sensitive( manualframe, false );
      gdk_threads_leave();

      selmode   = gtk_combo_box_get_active (GTK_COMBO_BOX(modecombo)) + 1;
      HedrShift = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(shiftspin));
      VIS = 0;
      for (i=0; i<0x80; i++) {
        if (VISmap[i] == selmode) {
          VIS = i;
          break;
        }
      }

      break;
    }

    if (++ptr == 25) {
      setVU(MaxPcm, -20);
      MaxPcm = 0;
      ptr = 0;
    }

    PcmPointer += 441;
  }

  // Skip the rest of the stop bit
  readPcm(20e-3 * 44100);
  PcmPointer += 20e-3 * 44100;

  // In case of Scottie, skip first sync pulse
  if (VISmap[VIS] == S1 || VISmap[VIS] == S2 || VISmap[VIS] == SDX) {
    readPcm(9e-3 * 44100);
    PcmPointer += 9e-3 * 44100;
  }

  if (VISmap[VIS] != UNKNOWN) return VISmap[VIS];
  else                        printf("  No VIS found\n");
  return 0;
}


