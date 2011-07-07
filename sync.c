#include <stdlib.h>
#include <math.h>
#include <fftw3.h>
#include <gtk/gtk.h>
#include "common.h"

/* Find the horizontal sync signal and adjust sample rate to cancel out any slant.
 *   Length:  number of PCM samples to process
 *   Mode:    one of M1, M2, S1, S2, R72, R36 ...
 *   Rate:    approximate sampling rate used
 *   Skip:    pointer to variable where the skip amount will be returned
 *   returns  adjusted sample rate
 */
double FindSync (unsigned int Length, int Mode, double Rate, int *Skip) {

  unsigned int       i, s, j, TotPix;
  double             NextImgSample;
  double             t=0, slantAngle;
  unsigned short int SyncImg[700][500];
  int                x,y;

  double  Praw, Psync;
  char    *HasSync;
  HasSync = malloc(Length * sizeof(char));
  if (HasSync == NULL) {
    perror("FindSync: Unable to allocate memory for sync signal\n");
    exit(EXIT_FAILURE);
  }
  
  unsigned short int *lines;
  lines = calloc(3000*720, sizeof(unsigned short int));
  if (HasSync == NULL) {
    perror("FindSync: Unable to allocate memory for Hough transform\n");
    exit(EXIT_FAILURE);
  }

  unsigned short int cy, cx;
  int                q, d, qMost, dMost;
  unsigned short int Retries = 0;
  int                maxsy = 0;
  FILE               *GrayFile;
  char               PixBuf[1] = {0};
  unsigned short int xAcc[700] = {0};
  unsigned short int xMax = 0;
  unsigned short int Leftmost;
    
  // FFT plan
  fftw_plan    Plan;
  double       *in;
  double       *out;
  unsigned int FFTLen = 1024;

  in      = fftw_malloc(sizeof(double) * FFTLen);
  if (in == NULL) {
    perror("FindSync: Unable to allocate memory for FFT\n");
    free(HasSync);
    exit(EXIT_FAILURE);
  }
  out     = fftw_malloc(sizeof(double) * FFTLen);
  if (out == NULL) {
    perror("FindSync: Unable to allocate memory for FFT\n");
    fftw_free(in);
    free(HasSync);
    exit(EXIT_FAILURE);
  }
  Plan    = fftw_plan_r2r_1d(FFTLen, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  // Create 50-point Hann window
  double Hann[50] = {0};
  for (i = 0; i < 50; i++) Hann[i] = 0.5 * (1 - cos( 2 * M_PI * i / 49.0) );

  // Zero fill input array
  for (i = 0; i < FFTLen; i++) in[i] = 0;

  // Power estimation
  for (s = 0; s < Length; s+=50) {

    // Hann window
    for (i = 0; i < 50; i++) in[i] = PCM[s+i] * 32768 * Hann[i];

    // FFT
    fftw_execute(Plan);

    // Power in raw band
    i = GetBin(700+HedrShift, FFTLen, 44100);
    Praw  = (pow(out[i-1],   2) + pow(out[FFTLen - (i-1)],   2) +
             pow(out[i],     2) + pow(out[FFTLen - i],       2) +
             pow(out[i+1],   2) + pow(out[FFTLen - (i+1)],   2))  / 3.0;

    // Power in the sync band
    i = GetBin(1200+HedrShift, FFTLen, 44100);
    Psync  = pow(out[i],   2) + pow(out[FFTLen - i],   2);

    // If there is more than twice the amount of Power per Hz in the
    // sync band than in the empty band, we have a sync signal here
    if (Psync > 2*Praw) HasSync[s] = TRUE;
    else                HasSync[s] = FALSE;

    HasSync[s] = !HasSync[s];  // Bug: Otherwise would produce reverse grayscale!

    for (i = 0; i < 50; i++) {
      if (s+i >= Length) break;
      HasSync[s+i] = HasSync[s];
    }

  }


  // Repeat until slant < 0.5° or until we give up
  while (1) {

    GrayFile = fopen("sync.gray","w");

    TotPix        = 0;
    NextImgSample = 0;
    t             = 0;
    maxsy         = 0;

    for (i=0;i<700;i++)
      for (j=0;j<500;j++)
        SyncImg[i][j] = 0;

    // Draw the sync signal into memory
    for (s = 0; s < Length; s++) {

      // t keeps track of time in seconds
      t += 1.0/Rate;

      if (t >= NextImgSample) {

        x = TotPix % 700;
        y = TotPix / 700;

        SyncImg[x][y] = HasSync[s];

        if (y > maxsy) maxsy = y;

        PixBuf[0] = (SyncImg[x][y] ? 255 : 0);
        fwrite(PixBuf, 1, 1, GrayFile);

        TotPix++;

        NextImgSample += ModeSpec[Mode].LineLen / 700.0;
      }
    }
    fclose(GrayFile);

    /** Linear Hough transform **/

    // zero arrays
    dMost = qMost = 0;
    for (d=0;d<3000;d++)
      for (q=MINSLANT*2; q <= MAXSLANT * 2; q++)
        lines[d*720 + q] = 0;

    // Find white pixels that likely belong to the left edge of a sync pulse
    for (cy = 0; cy < TotPix / 700; cy++) {
      for (cx = 0; cx < 700; cx++) {
        if (cx > 0 && cx < 697 && !SyncImg[cx - 1][cy] && SyncImg[cx][cy]) {

          // Slant angles to consider
          for (q = MINSLANT*2; q <= MAXSLANT*2; q ++) {

            // Find the line parameters with most occurrences
            d = round( -cx * sin(deg2rad(q/2.0)) + cy * cos(deg2rad(q/2.0)) );
            d = abs(d+2000);
            if (d<0)    d = 0;
            if (d>2999) d = 2999;
            lines[d*720 + q] ++;
            if (lines[d*720 + q] > lines[dMost*720 + qMost]) {
              dMost = d;
              qMost = q;
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
    printf("    %.1f° @ %.2f Hz", 90 - slantAngle, Rate);

    Rate = Rate + tan(deg2rad(90 - slantAngle)) / 700.0 * Rate;

    if (Rate < 40000 || Rate > 50000) {
      printf("    unrealistic receiving conditions; giving up.\n");
      Rate = 44100;
      break;
    }

    if (slantAngle > 89 && slantAngle < 91) {
      printf(" -> %.2f    slant OK :)\n", Rate);
      break;
    } else if (Retries == 3) {
      printf("            still slanted; giving up\n");
      Rate = 44100;
      printf("    -> 44100\n");
      break;
    } else {
      printf(" -> %.2f    still slanted; retrying\n", Rate);
      Retries ++;
    }
  }

  free(lines);

  printf("    gray = %dx%d\n", 700, maxsy);

  // Find the abscissa of the now vertical sync pulse
  for (i=0;i<700;i++) xAcc[i] = 0;
  xMax = 0;
  for (cy = 0; cy < TotPix / 700; cy++) {
    for (cx = 1; cx < 700; cx++) {
      if (!SyncImg[cx - 1][cy] && SyncImg[cx][cy]) {
        xAcc[cx]++;
        if (xAcc[cx] > xAcc[xMax]) xMax = cx;
      }
    }
  }
  // Now, find the leftmost one of those vertical lines with the maximum occurrences
  Leftmost = 700;
  for (i = 0; i < 700; i++) {
    if (xAcc[i] == xAcc[xMax] && i < Leftmost) Leftmost = i;
  }

  if (Rate == 44100)  Leftmost = 0;

  printf("    abscissa = %d (%d occurrences)",  Leftmost, xAcc[Leftmost]);
  Leftmost = Leftmost * (ModeSpec[Mode].LineLen / 700.0) * Rate;
  printf(" (need to skip %d samples)\n", Leftmost);

  *Skip = Leftmost;

  free(HasSync);
  fftw_destroy_plan(Plan);
  fftw_free(in);
  fftw_free(out);

  return (Rate);

}
