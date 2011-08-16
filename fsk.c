#include <stdlib.h>
#include <math.h>
#include <fftw3.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

/* 
 *
 * Decode FSK ID
 *
 * * The FSK IDs are 6-bit ASCII, LSB first, 45.45 baud, center freq 2000, shift 200
 * * Text data starts after 3F 20 2A and ends in 01
 *
 */

void GetFSK (char *dest) {

  int        Pointer = 0, ThisBitIndex = 0, RunLength=0, PrevBit = -1, Bit = 0;
  guint      FFTLen = 2048, i=0, LoBin, HiBin, MidBin;
  guchar     AsciiByte = 0, ThisByteIndex = 0;
  double     HiPow,LoPow,Hann[970];
  gboolean   InFSK = FALSE, InCode = FALSE, EndFSK = FALSE;

  for (i = 0; i < FFTLen; i++) in[i] = 0;

  // Create 22ms Hann window
  for (i = 0; i < 970; i++) Hann[i] = 0.5 * (1 - cos( 2 * M_PI * i / 969.0 ) );

  while ( TRUE ) {

    // Read 11 ms from DSP
    readPcm(485);

    // Apply Hann window
    for (i = 0; i < 970; i++) in[i] = PcmBuffer[PcmPointer+i-485] * Hann[i];

    // FFT of last 22 ms
    fftw_execute(Plan2048);

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

          if (InCode) {
            ThisBitIndex ++;
            if (ThisBitIndex > 0 && ThisBitIndex % 6 == 0) {
              // Consider end of data when values would only produce special characters
              if ( (AsciiByte&0x3F) < 0x0c) {
                EndFSK = TRUE;
                break;
              }
              dest[ThisByteIndex] = (AsciiByte&0x3F)+0x20;
              if (++ThisByteIndex > 20) break;
            }
          }

          if (AsciiByte == 0x55 && !InCode) {
            ThisBitIndex=-1;
            InCode = TRUE;
          }

        }
        if ( EndFSK ) break;
      }

      RunLength = 1;
      PrevBit = Bit;
    } else {
      RunLength ++;
    }


    Pointer++;

    if (Pointer > 200 || (!InFSK && Pointer > 50)) break;

    PcmPointer += 485;

  }
    
  dest[ThisByteIndex] = '\0';

}


