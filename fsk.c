#include <stdlib.h>
#include <math.h>
#include <fftw3.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

/* 
 *
 * Decode FSK
 *
 *
 */

void GetFSK () {

  int        samplesread = 0;
  int        Pointer = 0, ThisBit = 0, RunLength=0, PrevBit = -1, Bit = 0;
  guint      FFTLen = 2048, i=0, LoBin, HiBin, MidBin;
  guchar     AsciiByte = 0;
  double    *in, *out;
  double     HiPow,LoPow,Hann[2048];
  gboolean   InFSK = FALSE, InCode = FALSE;
  fftw_plan  Plan;

  // Plan for frequency estimation
  in = fftw_malloc(sizeof(double) * FFTLen);
  if (in == NULL) {
    perror("GetFSK: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }

  out = fftw_malloc(sizeof(double) * FFTLen);
  if (out == NULL) {
    perror("GetFSK: Unable to allocate memory for FFT");
    fftw_free(in);
    exit(EXIT_FAILURE);
  }
  Plan = fftw_plan_r2r_1d(FFTLen, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  for (i = 0; i < FFTLen; i++) in[i] = 0;

  // Create 22ms Hann window
  for (i = 0; i < SRATE*22e-3; i++) Hann[i] = 0.5 * (1 - cos( (2 * M_PI * (double)i) / (SRATE*22e-3 -1) ) );

  // Allocate space for PCM (1 second)
  PCM = calloc(SRATE, sizeof(double));
  if (PCM == NULL) {
    perror("GetFSK: Unable to allocate memory for PCM");
    exit(EXIT_FAILURE);
  }

  printf("FSK ID: ");

  //int a;
  // Skip 300 ms end tone
  //a = snd_pcm_forward (pcm_handle, SRATE*300e-3);
  //printf("skip %d\n",a);

  while ( TRUE ) {

    // Read 11 ms from DSP
    samplesread = snd_pcm_readi(pcm_handle, PcmBuffer, SRATE*11e-3);

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
    }

    // Apply Hann window
    for (i = 0; i < SRATE* 22e-3; i++) in[i] = PCM[i] * Hann[i];

    // FFT of last 10 ms
    fftw_execute(Plan);

    LoBin  = GetBin(1900+HedrShift, FFTLen)-1;
    MidBin = GetBin(2000+HedrShift, FFTLen);
    HiBin  = GetBin(2100+HedrShift, FFTLen)+1;

    LoPow = 0;
    HiPow = 0;

    for (i = LoBin; i <= HiBin; i++) {
      if (i < MidBin) LoPow += pow(out[i], 2) + pow(out[FFTLen - i], 2);
      else            HiPow += pow(out[i], 2) + pow(out[FFTLen - i], 2);
    }

    Bit = (HiPow<LoPow);

    if (Bit != PrevBit) {
      if (RunLength/2.0 > 3) InFSK = TRUE;

      if (InFSK) {
        if (RunLength/2.0 < .5) break;

        for (i=0; i<RunLength/2.0; i++) {
          AsciiByte >>= 1;
          AsciiByte ^= (PrevBit << 6);

          //printf("%d",PrevBit);

          if (InCode) {
            ThisBit ++;
            if (ThisBit % 6 == 0) {
              if ( (AsciiByte&0x3F)+0x20 == '!') break;
              printf("%c",(AsciiByte&0x3F)+0x20);
            }
          }

          if (AsciiByte == 0x55 && !InCode) {
            ThisBit=-1;
            InCode = TRUE;
          }

        }
      }

      RunLength = 1;
      PrevBit = Bit;
    } else {
      RunLength ++;
    }

    Pointer++;

    if (Pointer > 200) {
      break;
    }

  }

  printf("\n");

  fftw_free(in);
  fftw_free(out);
  fftw_destroy_plan(Plan);

  free(PCM);

}


