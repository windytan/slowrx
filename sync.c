#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fftw3.h>
#include <alsa/asoundlib.h>

#include "common.h"
#include "modespec.h"

/* Find the slant angle of the sync singnal and adjust sample rate to cancel it out
 *   Length:  number of PCM samples to process
 *   Mode:    one of M1, M2, S1, S2, R72, R36 ...
 *   Rate:    approximate sampling rate used
 *   Skip:    pointer to variable where the skip amount will be returned
 *   returns  adjusted sample rate
 *
 */
double FindSync (guchar Mode, double Rate, int *Skip) {

  int      LineWidth = ModeSpec[Mode].LineTime / ModeSpec[Mode].SyncTime * 4;
  int      x,y;
  int      q, d, qMost, dMost;
  gushort  xAcc[700] = {0};
  gushort  lines[600][(MAXSLANT-MINSLANT)*2];
  gushort  cy, cx, Retries = 0;
  gboolean SyncImg[700][630] = {{FALSE}};
  double   t=0, slantAngle, s;
  double   ConvoFilter[8] = { 1,1,1,1,-1,-1,-1,-1 };
  double   convd, maxconvd=0;
  int      xmax=0;

  // Repeat until slant < 0.5° or until we give up
  while (TRUE) {

    // Draw the 2D sync signal at current rate
    
    for (y=0; y<ModeSpec[Mode].NumLines; y++) {
      for (x=0; x<LineWidth; x++) {
        t = (y + 1.0*x/LineWidth) * ModeSpec[Mode].LineTime;
        SyncImg[x][y] = HasSync[ (int)( t * Rate / 13.0) ];
      }
    }

    /** Linear Hough transform **/

    dMost = qMost = 0;
    memset(lines, 0, sizeof(lines[0][0]) * (MAXSLANT-MINSLANT)*2 * 600);

    // Find white pixels
    for (cy = 0; cy < ModeSpec[Mode].NumLines; cy++) {
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
    Rate += tan(deg2rad(90 - slantAngle)) / LineWidth * Rate;

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
  
  // accumulate a 1-dim array of the position of the sync pulse
  memset(xAcc, 0, sizeof(xAcc[0]) * 700);
  for (y=0; y<ModeSpec[Mode].NumLines; y++) {
    for (x=0; x<700; x++) { 
      t = y * ModeSpec[Mode].LineTime + x/700.0 * ModeSpec[Mode].LineTime;
      xAcc[x] += HasSync[ (int)(t / (13.0/44100) * Rate/44100) ];
    }
  }

  // find falling edge of the sync pulse by 8-point convolution
  for (x=0;x<700-8;x++) {
    convd = 0;
    for (int i=0;i<8;i++) convd += xAcc[x+i] * ConvoFilter[i];
    if (convd > maxconvd) {
      maxconvd = convd;
      xmax = x+4;
    }
  }

  // If pulse is near the right edge of the image, it just probably slipped
  // out the left edge
  if (xmax > 350) xmax -= 350;

  // Skip until the start of the line
  s = xmax / 700.0 * ModeSpec[Mode].LineTime - ModeSpec[Mode].SyncTime;
  
  // (Scottie modes don't start lines with sync)
  if (Mode == S1 || Mode == S2 || Mode == SDX)
    s = s - ModeSpec[Mode].PixelTime * ModeSpec[Mode].ImgWidth / 2.0
          + ModeSpec[Mode].PorchTime * 2;

  *Skip = s * Rate;

  printf("will return %.2f\n",Rate);
  
  return (Rate);

}
