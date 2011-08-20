#include <stdlib.h>
#include <stdbool.h>
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
 *
 */
double FindSync (guchar Mode, double Rate, int *Skip) {

  int      LineWidth = ModeSpec[Mode].LineLen / ModeSpec[Mode].SyncLen * 4;
  int      x,y,xmid,x0;
  int      q, d, qMost, dMost;
  gushort  xAcc[700] = {0}, xmax, s;
  gushort  lines[600][(MAXSLANT-MINSLANT)*2];
  gushort  cy, cx, Retries = 0;
  bool     SyncImg[700][630] = {{false}};
  double   t=0, slantAngle;


  // Repeat until slant < 0.5° or until we give up
  while (true) {

    // Draw the 2D sync signal at current rate

    for (y=0; y<ModeSpec[Mode].ImgHeight; y++) {
      for (x=0; x<LineWidth; x++) {
        t = (y + 1.0*x/LineWidth) * ModeSpec[Mode].LineLen;
        // Center sync pulse horizontally
        if (y>0 || x>=LineWidth/2) SyncImg[x][y] = HasSync[ (int)( (t-ModeSpec[Mode].LineLen/2) / 1.5e-3 * Rate/44100) ];
      }
    }

    /** Linear Hough transform **/

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

    printf("    %.1f° (d=%d) @ %.1f Hz", slantAngle, dMost, Rate);

    // Adjust sample rate
    Rate = Rate + tan(deg2rad(90 - slantAngle)) / LineWidth * Rate;

    if (slantAngle > 89 && slantAngle < 91) {
      printf("            slant OK :)\n");
      break;
    } else if (Retries == 3) {
      printf("            still slanted; giving up\n");
      Rate = 44100;
      printf("    -> 44100\n");
      break;
    }
    printf(" -> %.1f    recalculating\n", Rate);
    Retries ++;
  }
  
  // find abscissa at higher resolution
  memset(xAcc, 0, sizeof(xAcc[0]) * 700);
  xmax = 0;
 
  for (y=0; y<ModeSpec[Mode].ImgHeight; y++) {
    for (x=0; x<700; x++) { 
      t = y * ModeSpec[Mode].LineLen + x/700.0 * ModeSpec[Mode].LineLen;
      xAcc[x] += HasSync[ (int)(t / 1.5e-3 * Rate/44100) ];
      if (xAcc[x] > xAcc[xmax]) xmax = x;
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
