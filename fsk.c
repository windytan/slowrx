#include <stdlib.h>
#include <math.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"

/* 
 *
 * Decode FSK ID
 *
 * * The FSK IDs are 6-bit bytes, LSB first, 45.45 baud (22 ms/bit), 1900 Hz = 1, 2100 Hz = 0
 * * Text data starts with 20 2A and ends in 01
 * * Add 0x20 and the data becomes ASCII
 *
 */

void GetFSK (char *dest) {

  guint      FFTLen = 2048, i=0, LoBin, HiBin, MidBin, TestNum=0, TestPtr=0;
  guchar     Bit = 0, AsciiByte = 0, BytePtr = 0, TestBits[24] = {0}, BitPtr=0;
  double     HiPow,LoPow,Hann[970];
  gboolean   InSync = FALSE;

  // Bit-reversion lookup table
  static const guchar BitRev[] = {
    0x00, 0x20, 0x10, 0x30,   0x08, 0x28, 0x18, 0x38,
    0x04, 0x24, 0x14, 0x34,   0x0c, 0x2c, 0x1c, 0x3c,
    0x02, 0x22, 0x12, 0x32,   0x0a, 0x2a, 0x1a, 0x3a,
    0x06, 0x26, 0x16, 0x36,   0x0e, 0x2e, 0x1e, 0x3e,
    0x01, 0x21, 0x11, 0x31,   0x09, 0x29, 0x19, 0x39,
    0x05, 0x25, 0x15, 0x35,   0x0d, 0x2d, 0x1d, 0x3d,
    0x03, 0x23, 0x13, 0x33,   0x0b, 0x2b, 0x1b, 0x3b,
    0x07, 0x27, 0x17, 0x37,   0x0f, 0x2f, 0x1f, 0x3f };

  for (i = 0; i < FFTLen; i++) fft.in[i] = 0;

  // Create 22ms Hann window
  for (i = 0; i < 970; i++) Hann[i] = 0.5 * (1 - cos( 2 * M_PI * i / 969.0 ) );

  while ( TRUE ) {

    // Read data from DSP
    readPcm(InSync ? 970: 485);

    if (pcm.WindowPtr < 485)
      continue;

    // Apply Hann window
    for (i = 0; i < 970; i++) fft.in[i] = pcm.Buffer[pcm.WindowPtr+i- 485] * Hann[i];
    
    pcm.WindowPtr += (InSync ? 970 : 485);

    // FFT of last 22 ms
    fftw_execute(fft.Plan2048);

    LoBin  = GetBin(1900+CurrentPic.HedrShift, FFTLen)-1;
    MidBin = GetBin(2000+CurrentPic.HedrShift, FFTLen);
    HiBin  = GetBin(2100+CurrentPic.HedrShift, FFTLen)+1;

    LoPow = 0;
    HiPow = 0;

    for (i = LoBin; i <= HiBin; i++) {
      if (i < MidBin) LoPow += power(fft.out[i]);
      else            HiPow += power(fft.out[i]);
    }

    Bit = (LoPow>HiPow);

    if (!InSync) {

      // Wait for 20 2A
      
      TestBits[TestPtr % 24] = Bit;

      TestNum = 0;
      for (i=0; i<12; i++) TestNum |= TestBits[(TestPtr - (23-i*2)) % 24] << (11-i);

      if (BitRev[(TestNum >>  6) & 0x3f] == 0x20 && BitRev[TestNum & 0x3f] == 0x2a) {
        InSync    = TRUE;
        AsciiByte = 0;
        BitPtr    = 0;
        BytePtr   = 0;
      }

      if (++TestPtr > 200) break;

    } else {

      AsciiByte |= Bit << BitPtr;

      if (++BitPtr == 6) {
        if (AsciiByte < 0x0d || BytePtr > 9) break;
        dest[BytePtr] = AsciiByte + 0x20;
        BitPtr        = 0;
        AsciiByte     = 0;
        BytePtr ++;
      }
    }

  }
    
  dest[BytePtr] = '\0';

}
