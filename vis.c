#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "fft.h"
#include "gui.h"
#include "modespec.h"
#include "pcm.h"
#include "pic.h"

/* 
 *
 * Detect VIS & frequency shift
 *
 * Each bit lasts 30 ms (1323 samples)
 *
 */

TextStatusCallback OnVisStatusChange;

guchar GetVIS () {

  int        selmode, ptr=0;
  int        VIS = 0, Parity = 0, HedrPtr = 0;
  guint      FFTLen = 2048, i=0, j=0, k=0, MaxBin = 0;
  double     Power[2048] = {0}, HedrBuf[100] = {0}, tone[100] = {0}, Hann[882] = {0};
  gboolean   gotvis = FALSE;
  guchar     Bit[8] = {0}, ParityBit = 0;

  for (i = 0; i < FFTLen; i++) fft.in[i]    = 0;

  // Create 20ms Hann window
  for (i = 0; i < 882; i++) Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / 881 ) );

  ManualActivated = FALSE;
  
  printf("Waiting for header\n");

  if (OnVisStatusChange) {
    OnVisStatusChange("Listening");
  }

  while ( TRUE ) {

    if (Abort || ManualResync) return(0);

    // Read 10 ms from sound card
    readPcm(441);

    // Apply Hann window
    for (i = 0; i < 882; i++) fft.in[i] = pcm.Buffer[pcm.WindowPtr + i - 441] / 32768.0 * Hann[i];

    // FFT of last 20 ms
    fftw_execute(fft.Plan2048);

    // Find the bin with most power
    MaxBin = 0;
    for (i = 0; i <= GetBin(6000, FFTLen); i++) {
      Power[i] = power(fft.out[i]);
      if ( (i >= GetBin(500,FFTLen) && i < GetBin(3300,FFTLen)) &&
           (MaxBin == 0 || Power[i] > Power[MaxBin]))
        MaxBin = i;
    }

    // Find the peak frequency by Gaussian interpolation
    if (MaxBin > GetBin(500, FFTLen) && MaxBin < GetBin(3300, FFTLen) &&
        Power[MaxBin] > 0 && Power[MaxBin+1] > 0 && Power[MaxBin-1] > 0)
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
    CurrentPic.HedrShift = 0;
    gotvis    = FALSE;
    for (i = 0; i < 3; i++) {
      if (CurrentPic.HedrShift != 0) break;
      for (j = 0; j < 3; j++) {
        if ( (tone[1*3+i]  > tone[0+j] - 25  && tone[1*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[2*3+i]  > tone[0+j] - 25  && tone[2*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[3*3+i]  > tone[0+j] - 25  && tone[3*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[4*3+i]  > tone[0+j] - 25  && tone[4*3+i]  < tone[0+j] + 25)  && // 1900 Hz leader
             (tone[5*3+i]  > tone[0+j] - 725 && tone[5*3+i]  < tone[0+j] - 675) && // 1200 Hz start bit
                                                                                   // ...8 VIS bits...
             (tone[14*3+i] > tone[0+j] - 725 && tone[14*3+i] < tone[0+j] - 675)    // 1200 Hz stop bit
           ) {

          // Attempt to read VIS

          gotvis = TRUE;
          for (k = 0; k < 8; k++) {
            if      (tone[6*3+i+3*k] > tone[0+j] - 625 && tone[6*3+i+3*k] < tone[0+j] - 575) Bit[k] = 0;
            else if (tone[6*3+i+3*k] > tone[0+j] - 825 && tone[6*3+i+3*k] < tone[0+j] - 775) Bit[k] = 1;
            else { // erroneous bit
              gotvis = FALSE;
              break;
            }
          }
          if (gotvis) {
            CurrentPic.HedrShift = tone[0+j] - 1900;

            VIS = Bit[0] + (Bit[1] << 1) + (Bit[2] << 2) + (Bit[3] << 3) + (Bit[4] << 4) +
                 (Bit[5] << 5) + (Bit[6] << 6);
            ParityBit = Bit[7];

            printf("  VIS %d (%02Xh) @ %+d Hz\n", VIS, VIS, CurrentPic.HedrShift);

            Parity = Bit[0] ^ Bit[1] ^ Bit[2] ^ Bit[3] ^ Bit[4] ^ Bit[5] ^ Bit[6];

            if (VISmap[VIS] == R12BW) Parity = !Parity;

            if (Parity != ParityBit) {
              printf("  Parity fail\n");
              gotvis = FALSE;
            } else if (VISmap[VIS] == UNKNOWN) {
              printf("  Unknown VIS\n");
              gotvis = FALSE;
            } else {
              gdk_threads_enter();
              gtk_combo_box_set_active (GTK_COMBO_BOX(gui.combo_mode), VISmap[VIS]-1);
              gtk_spin_button_set_value (GTK_SPIN_BUTTON(gui.spin_shift), CurrentPic.HedrShift);
              gdk_threads_leave();
              break;
            }
          }
        }
      }
    }

    if (gotvis)
     if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_rx))) break;

    // Manual start
    if (ManualActivated) {

      gdk_threads_enter();
      gtk_widget_set_sensitive( gui.frame_manual, FALSE );
      gtk_widget_set_sensitive( gui.combo_card,   FALSE );
      gdk_threads_leave();

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
      setVU(Power, 2048, 6);
      ptr = 0;
    }

    pcm.WindowPtr += 441;
  }

  // Skip the rest of the stop bit
  readPcm(20e-3 * 44100);
  pcm.WindowPtr += 20e-3 * 44100;

  if (VISmap[VIS] != UNKNOWN) return VISmap[VIS];
  else                        printf("  No VIS found\n");
  return 0;
}
