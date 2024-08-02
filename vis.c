#include <stdlib.h>
#include <complex.h>
#include <math.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "fft.h"
#include "modespec.h"
#include "pcm.h"
#include "pic.h"

/* Approximate number of frames / samples for 10ms: NB can change at runtime! */
#define VIS_10MS_FRAMES	PCM_MS_FRAMES(10)
#define VIS_10MS_SAMPLES (VIS_10MS_FRAMES*2)

/* Ditto, for 20ms */
#define VIS_20MS_FRAMES	PCM_MS_FRAMES(20)

/* 
 *
 * Detect VIS & frequency shift
 *
 * Each bit lasts 30 ms (1323 samples)
 *
 */

TextStatusCallback OnVisStatusChange;
EventCallback OnVisIdentified;
UpdateVUCallback OnVisPowerComputed;
uint8_t VIS;
_Bool VisAutoStart;

#define VIS_FFT_LEN (FFT_FULL_SZ)
static double VisPower[VIS_FFT_LEN] = {0};

uint8_t GetVIS () {

  int32_t    ptr=0;
  int32_t    Parity = 0, HedrPtr = 0;
  uint32_t   i=0, j=0, k=0, MaxBin = 0;
  double     HedrBuf[100] = {0}, tone[100] = {0}, Hann[VIS_10MS_SAMPLES];
  _Bool      gotvis = false;
  uint8_t     Bit[8] = {0}, ParityBit = 0;

  memset(Hann, 0, sizeof(double) * VIS_10MS_SAMPLES);

  for (i = 0; i < VIS_FFT_LEN; i++) fft.in[i]    = 0;

  // Create 20ms Hann window
  for (i = 0; i < VIS_10MS_SAMPLES; i++) {
    Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / (VIS_10MS_SAMPLES-1) ) );
  }

  ManualActivated = false;
  
  printf("Waiting for header\n");

  if (OnVisStatusChange) {
    OnVisStatusChange("Listening");
  }

  while ( true ) {

    if (Abort || ManualResync) return(0);

    // Read 10 ms from sound card
    readPcm(VIS_10MS_FRAMES);

    // Apply Hann window
    for (i = 0; i < VIS_10MS_SAMPLES; i++) {
      fft.in[i] = pcm.Buffer[pcm.WindowPtr + i - VIS_10MS_FRAMES] / 32768.0 * Hann[i];
    }

    // FFT of last 20 ms
    fftw_execute(fft.PlanFull);

    // Find the bin with most power
    MaxBin = 0;
    for (i = 0; i <= GetBin(6000, VIS_FFT_LEN); i++) {
      VisPower[i] = power(fft.out[i]);
      if ( (i >= GetBin(500,VIS_FFT_LEN) && i < GetBin(3300,VIS_FFT_LEN)) &&
           (MaxBin == 0 || VisPower[i] > VisPower[MaxBin]))
        MaxBin = i;
    }

    // Find the peak frequency by Gaussian interpolation
    if (MaxBin > GetBin(500, VIS_FFT_LEN) && MaxBin < GetBin(3300, VIS_FFT_LEN) &&
        VisPower[MaxBin] > 0 && VisPower[MaxBin+1] > 0 && VisPower[MaxBin-1] > 0)
         HedrBuf[HedrPtr] = MaxBin +            (log( VisPower[MaxBin + 1] / VisPower[MaxBin - 1] )) /
                             (2 * log( pow(VisPower[MaxBin], 2) / (VisPower[MaxBin + 1] * VisPower[MaxBin - 1])));
    else HedrBuf[HedrPtr] = HedrBuf[(HedrPtr-1) % 45];

    // In Hertz
    HedrBuf[HedrPtr] = HedrBuf[HedrPtr] / VIS_FFT_LEN * pcm.SampleRate;

    // Header buffer holds 45 * 10 msec = 450 msec
    HedrPtr = (HedrPtr + 1) % 45;

    // Frequencies in the last 450 msec
    for (i = 0; i < 45; i++) tone[i] = HedrBuf[(HedrPtr + i) % 45];

    // Is there a pattern that looks like (the end of) a calibration header + VIS?
    // Tolerance ±25 Hz
    CurrentPic.HedrShift = 0;
    gotvis    = false;
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
            CurrentPic.HedrShift = tone[0+j] - 1900;

            VIS = Bit[0] + (Bit[1] << 1) + (Bit[2] << 2) + (Bit[3] << 3) + (Bit[4] << 4) +
                 (Bit[5] << 5) + (Bit[6] << 6);
            ParityBit = Bit[7];

            printf("  VIS %d (%02Xh) @ %+d Hz\n", VIS, VIS, CurrentPic.HedrShift);

            Parity = Bit[0] ^ Bit[1] ^ Bit[2] ^ Bit[3] ^ Bit[4] ^ Bit[5] ^ Bit[6];

            if (VISmap[VIS] == R12BW) Parity = !Parity;

            if (Parity != ParityBit) {
              printf("  Parity fail\n");
              gotvis = false;
            } else if (VISmap[VIS] == UNKNOWN) {
              printf("  Unknown VIS\n");
              gotvis = false;
            } else {
              break;
            }
          }
        }
      }
    }

    if (gotvis && OnVisIdentified) {
      OnVisIdentified();
    }
    if (gotvis && VisAutoStart) {
      break;
    }

    // Manual start
    if (ManualActivated) {
      break;
    }

    if (++ptr == 10) {
      if (OnVisPowerComputed) {
        OnVisPowerComputed(VisPower, VIS_FFT_LEN, 6);
      }
      ptr = 0;
    }

    pcm.WindowPtr += VIS_10MS_FRAMES;
  }

  // Skip the rest of the stop bit
  readPcm(VIS_20MS_FRAMES);
  pcm.WindowPtr += VIS_20MS_FRAMES;

  if (VISmap[VIS] != UNKNOWN) return VISmap[VIS];
  else                        printf("  No VIS found\n");
  return 0;
}
