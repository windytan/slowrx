#include <stdlib.h>
#include <math.h>
#include <fftw3.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

/* 
 *
 * Detect VIS
 *
 * Each bit lasts 30 ms (1323 samples)
 *
 */

guchar GetVIS () {

  int        selmode;
  int        Pointer = 0, VIS = 0, Parity = 0, ParityBit = 0, Bit[8] = {0}, HedrPtr = 0;
  gushort    MaxPcm = 0;
  guint      FFTLen = 2048, i=0, j=0, k=0, MaxBin = 0;
  double     Power[2048] = {0}, HedrBuf[100] = {0}, tone[100] = {0}, Hann[882] = {0};
  char       infostr[60] = {0};
  gboolean   gotvis = FALSE;

  for (i = 0; i < FFTLen; i++) in[i] = 0;

  // Create 20ms Hann window
  for (i = 0; i < 882; i++) Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / 881 ) );

  ManualActivated = FALSE;
  
  printf("Waiting for header\n");

  gdk_threads_enter();
  gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Ready" );
  gdk_threads_leave();

  while ( TRUE ) {

    // Read 10 ms from sound card
    readPcm(441);

    // Apply Hann window
    for (i = 0; i < 882; i++) in[i] = PcmBuffer[PcmPointer + i - 441] / 32768.0 * Hann[i];

    // FFT of last 20 ms
    fftw_execute(Plan2048);

    MaxBin = 0;

    // Save most powerful freq
    for (i = GetBin(500, FFTLen); i <= GetBin(3300, FFTLen); i++) {

      Power[i] = pow(out[i], 2) + pow(out[FFTLen - i], 2);
      if (Power[i] > Power[MaxBin] || MaxBin == 0) MaxBin = i;

    }

    // Gaussian interpolation to get the exact peak frequency
    if (MaxBin > GetBin(500, FFTLen) && MaxBin < GetBin(3300, FFTLen)) {
      HedrBuf[HedrPtr] = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                          (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
    } else {
      HedrBuf[HedrPtr] = HedrBuf[(HedrPtr-1)%50];
    }

    // Header buffer holds 50 * 10 msec = 500 msec
    HedrPtr = (HedrPtr + 1) % 50;

    for (i = 0; i < 50; i++) {
      tone[i] = HedrBuf[(HedrPtr + i) % 50];
      tone[i] = 1.0 * tone[i] / FFTLen * 44100;
    }

    // Is there a pattern that looks like (the end of) a calibration header + VIS?
    // Tolerance ±10 Hz
    HedrShift = 0;
    gotvis    = FALSE;
    for (i = 0; i < 3; i++) {
      if (HedrShift != 0) break;
      for (j = 0; j < 3; j++) {
        if ( (tone[1*3+i]  > tone[0+j] - 10  && tone[1*3+i]  < tone[0+j] + 10) && // 1900 Hz leader
             (tone[2*3+i]  > tone[0+j] - 10  && tone[2*3+i]  < tone[0+j] + 10) && // 1900 Hz leader
             (tone[3*3+i]  > tone[0+j] - 10  && tone[3*3+i]  < tone[0+j] + 10) && // 1900 Hz leader
             (tone[4*3+i]  > tone[0+j] - 10  && tone[4*3+i]  < tone[0+j] + 10) && // 1900 Hz leader

             (tone[5*3+i]  > tone[0+j] - 710 && tone[5*3+i]  < tone[0+j] - 690) && // 1200 Hz start bit
             (tone[14*3+i] > tone[0+j] - 710 && tone[14*3+i] < tone[0+j] - 690)    // 1200 Hz stop bit
           ) {

          printf("Possible header @ %+.0f Hz\n",tone[0+j]-1900);

          // Read VIS

          gotvis = TRUE;
          for (k = 0; k < 8; k++) {
            if      (tone[6*3+i+3*k] > tone[0+j] - 610 && tone[6*3+i+3*k] < tone[0+j] - 590) Bit[k] = 0;
            else if (tone[6*3+i+3*k] > tone[0+j] - 810 && tone[6*3+i+3*k] < tone[0+j] - 790) Bit[k] = 1;
            else { // erroneous bit
              gotvis = FALSE;
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

            if (Parity != ParityBit && VIS != 0x06) {
              printf("  Parity fail\n");
              gotvis = FALSE;
            } else if (VISmap[VIS] == UNKNOWN) {
              printf("  Unknown VIS\n");
              snprintf(infostr, sizeof(infostr)-1, "How to decode image with VIS %d (%02Xh)?", VIS, VIS);
              gotvis = FALSE;
              gdk_threads_enter();
              gtk_label_set_markup(GTK_LABEL(infolabel), infostr);
              gdk_threads_leave();
            } else {
              break;
            }
          }
        }
      }
    }

    if (gotvis) {
     if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togrx))) break;
    }

    // Manual start
    if (ManualActivated) {

      gdk_threads_enter();
      gtk_widget_set_sensitive( manualframe, FALSE );
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

    if (++Pointer >= 50) Pointer = 0;

    if (Pointer == 0 || Pointer == 25) {
      setVU(MaxPcm, -20);
      MaxPcm = 0;
    }

    PcmPointer += 441;
  }

  // Hack;
  //PcmPointer -= 1800;

  // In case of Scottie, skip 9 ms
  if (VISmap[VIS] == S1 || VISmap[VIS] == S2 || VISmap[VIS] == SDX) readPcm(44100*9e-3);

  if (VISmap[VIS] != UNKNOWN) return VISmap[VIS];
  else                        printf("  No VIS found\n");
  return 0;
}


