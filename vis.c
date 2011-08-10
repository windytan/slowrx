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

int GetVIS () {

  printf("Waiting for header\n");

  gdk_threads_enter();
  gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Waiting for header" );
  gdk_threads_leave();

  fftw_plan VISPlan;
  double *in;
  double *out;
  unsigned int FFTLen = 2048;
  int selmode;

  // Plan for frequency estimation
  in      = fftw_malloc(sizeof(double) * FFTLen);
  if (in == NULL) {
    perror("GetVIS: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }

  out     = fftw_malloc(sizeof(double) * FFTLen);
  if (out == NULL) {
    perror("GetVIS: Unable to allocate memory for FFT");
    fftw_free(in);
    exit(EXIT_FAILURE);
  }
  VISPlan = fftw_plan_r2r_1d(FFTLen, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  unsigned int i=0, j=0, k=0, MaxBin = 0;
  int          samplesread = 0;
  int          Pointer = 0, VIS = 0, Parity = 0, ParityBit = 0, Bit[8] = {0};
  double       Power[2048] = {0};
  double       HedrBuf[100] = {0}, tone[100] = {0};
  int          HedrPtr = 0;
  short int    MaxPcm = 0;
  char         infostr[60] = {0}, gotvis = FALSE;

  for (i = 0; i < FFTLen; i++) in[i] = 0;

  // Create 20ms Hann window
  double Hann[SRATE/50] = {0};
  for (i = 0; i < SRATE*20e-3; i++) Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / (SRATE*20e-3 -1) ) );

  // Allocate space for PCM (1 second)
  PCM = calloc(SRATE, sizeof(double));
  if (PCM == NULL) {
    perror("GetVIS: Unable to allocate memory for PCM");
    exit(EXIT_FAILURE);
  }

  ManualActivated = FALSE;

  while ( TRUE ) {

    // Read 10 ms from DSP
    samplesread = snd_pcm_readi(pcm_handle, PcmBuffer, SRATE*10e-3);

    if (samplesread == -EPIPE) {
      printf("ALSA buffer overrun :(\n");
      exit(EXIT_FAILURE);
    } else if (samplesread == -EBADFD) {
      printf("ALSA: PCM is not in the right state\n");
      exit(EXIT_FAILURE);
    } else if (samplesread == -ESTRPIPE) {
      printf("ALSA: a suspend event occurred\n");
      exit(EXIT_FAILURE);
    } else if (samplesread < 0) {
      printf("ALSA error\n");
      exit(EXIT_FAILURE);
    }

    // Move buffer
    for (i = 0; i < samplesread; i++) {
      PCM[i] = PCM[i + samplesread];
      PCM[i+samplesread] = PcmBuffer[i];

      // Keep track of max power for VU meter
      if (abs(PcmBuffer[i]) > MaxPcm) MaxPcm = abs(PcmBuffer[i]);
    }

    // Apply Hann window
    for (i = 0; i < SRATE* 20e-3; i++) in[i] = PCM[i] * Hann[i];

    // FFT of last 20 ms
    fftw_execute(VISPlan);

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
      tone[i] = 1.0 * tone[i] / FFTLen * SRATE;
    }

    // Is there a pattern that looks like (the end of) a calibration header + VIS?
    // Tolerance ±25 Hz
    HedrShift = 0;
    gotvis    = FALSE;
    for (i = 0; i < 3; i++) {
      if (HedrShift != 0) break;
      for (j = 0; j < 3; j++) {
        if ( (tone[3+i]  > tone[0+j] - 25 && tone[3+i]  < tone[0+j] + 25) && // 1900 Hz leader
             (tone[6+i]  > tone[0+j] - 25 && tone[6+i]  < tone[0+j] + 25) && // 1900 Hz leader
             (tone[9+i]  > tone[0+j] - 25 && tone[9+i]  < tone[0+j] + 25) && // 1900 Hz leader
             (tone[12+i] > tone[0+j] - 25 && tone[12+i] < tone[0+j] + 25) && // 1900 Hz leader

             (tone[15+i] > tone[0+j] - 725 && tone[15+i] < tone[0+j] - 675) && // 1200 Hz start bit
             (tone[42+i] > tone[0+j] - 725 && tone[42+i] < tone[0+j] - 675)    // 1200 Hz stop bit
           ) {

          printf("Possible header @ %+.0f Hz\n",tone[0+j]-1900);

          // Read VIS

          gotvis = TRUE;
          for (k = 0; k < 8; k++) {
            if      (tone[18+i+3*k] > tone[0+j] - 625 && tone[18+i+3*k] < tone[0+j] - 575) Bit[k] = 0;
            else if (tone[18+i+3*k] > tone[0+j] - 825 && tone[18+i+3*k] < tone[0+j] - 775) Bit[k] = 1;
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

            printf("  VIS %d (%02Xh) @ %+.0f Hz\n", VIS, VIS, HedrShift);

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
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togrx))) {
        break;
      }
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
  }

  fftw_free(in);
  fftw_free(out);
  fftw_destroy_plan(VISPlan);

  free(PCM);

  // In case of Scottie, skip 9 ms
  if (VISmap[VIS] == S1 || VISmap[VIS] == S2 || VISmap[VIS] == SDX)
    samplesread = snd_pcm_readi(pcm_handle, PcmBuffer, SRATE*9e-3);

  if (VISmap[VIS] != UNKNOWN) return VISmap[VIS];
  else                        printf("  No VIS found\n");
  return -1;
}


