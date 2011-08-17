#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fftw3.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

/* Find the slant angle of the sync singnal and adjust sample rate to cancel it out
 *   Length:  number of PCM samples to process
 *   Mode:    one of M1, M2, S1, S2, R72, R36 ...
 *   Rate:    approximate sampling rate used
 *   Skip:    pointer to variable where the skip amount will be returned
 *   returns  adjusted sample rate
 */
guint FindSync (guint Length, guchar Mode, guint Rate, int *Skip) {

  int      LineWidth = ModeSpec[Mode].LineLen / ModeSpec[Mode].SyncLen * 4;
  int      x,y,xmid,x0;
  int      q, d, qMost, dMost;
  int      maxsy = 0;
  guint    s, TotPix, xmax;
  gushort  xAcc[700] = {0};
  gushort  lines[600][(MAXSLANT-MINSLANT)*2];
  gushort  cy, cx, Retries = 0;
  gboolean SyncImg[700][630];
  double   NextImgSample, t=0, slantAngle;

  // Repeat until slant < 0.5° or until we give up
  while (TRUE) {
    TotPix        = LineWidth/2; // Start at the middle of the picture
    NextImgSample = 0;
    t             = 0;
    maxsy         = 0;
    x = y         = 0;

    memset(SyncImg, FALSE, sizeof(SyncImg[0][0]) * 700 * 630);
        
    // Draw the 2D sync signal at current rate

    for (y=0; y<ModeSpec[Mode].ImgHeight; y++) {
      for (x=0; x<LineWidth; x++) {
        t = y * ModeSpec[Mode].LineLen + 1.0*x/LineWidth * ModeSpec[Mode].LineLen;
        SyncImg[x][y] = HasSync[ (int)(t / 1.5e-3 * Rate/44100) ];
      }
    }

    /** Linear Hough transform **/

    // zero arrays
    dMost = qMost = 0;
    memset(lines, 0, sizeof(lines[0][0]) * (MAXSLANT-MINSLANT)*2 * 600);

    // Find white pixels
    for (cy = 0; cy < ModeSpec[Mode].ImgHeight; cy++) {
      for (cx = 0; cx < LineWidth; cx++) {
        if (SyncImg[cx][cy]) {

          // Slant angles to consider
          for (q = MINSLANT*2; q < MAXSLANT*2; q ++) {

            // Line accumulator
            d = LineWidth + round( -cx * sin(deg2rad(q/2.0)) + cy * cos(deg2rad(q/2.0)) );
            if (d > 0 && d < LineWidth) {
              lines[d][q-MINSLANT*2] ++;
              if (lines[d][q-MINSLANT*2] > lines[dMost][qMost-MINSLANT*2]) {
                dMost = d;
                qMost = q;
              }
            }
          }
        }
      }
    }

    if ( qMost == 0) {
      printf("    no sync signal; giving up\n");
      break;
    }

    slantAngle = qMost / 2.0;

    //printf("  most (%d occurrences): d=%d  q=%f\n", LineAcc[dMost][ (int)(qMost * 10) ], dMost, qMost);
    printf("    %.1f° (d=%d) @ %d Hz", slantAngle, dMost, Rate);

    Rate = Rate + tan(deg2rad(90 - slantAngle)) / (1.0 * LineWidth) * Rate;

    if (slantAngle > 89 && slantAngle < 91) {
      printf("            slant OK :)\n");
      break;
    } else if (Retries == 3) {
      printf("            still slanted; giving up\n");
      Rate = 44100;
      printf("    -> 44100\n");
      break;
    } else {
      printf(" -> %d    recalculating\n", Rate);
      Retries ++;
    }
  }

  printf("    gray = %dx%d\n", LineWidth, maxsy);
    
  // find abscissa at high granularity
  t = 0;
  x = 0;
  xmax=0;
  NextImgSample=0;
  
  memset(xAcc, 0, sizeof(xAcc[0]) * 700);
  
  for (s = 0; s < Length; s++) {

    t += 1.0/Rate;

    if (t >= NextImgSample) {

      xAcc[x] += HasSync[s];
      if (xAcc[x] > xAcc[xmax]) xmax = x;

      if (++x >= 700) x = 0;

      NextImgSample += ModeSpec[Mode].LineLen / 700.0;
    }
  }

  // find center of sync pulse
  x0 = -1;
  xmid=-1;
  for (x=0;x<700;x++) {
    if (xAcc[x] >= xAcc[xmax]*0.5 && x0==-1) x0 = x;
    if (x0 != -1 && xAcc[x] <  xAcc[xmax]*0.5) {
      xmid = (x + x0) / 2;
      break;
    }
  }

  // skip until the start of the sync pulse
  s = (xmid / 700.0 * ModeSpec[Mode].LineLen - ModeSpec[Mode].SyncLen/2) * Rate;

  // Scottie modes don't start lines with the sync pulse
  if (Mode == S1 || Mode == S2 || Mode == SDX)
    s -= 2 * (ModeSpec[Mode].SeparatorLen + ModeSpec[Mode].PixelLen*ModeSpec[Mode].ImgWidth) * Rate;

  *Skip = s;
  
  return (Rate);

}
