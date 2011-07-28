#include <stdlib.h>
#include <math.h>
#include <string.h>
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

  int                LineWidth = ModeSpec[Mode].LineLen / ModeSpec[Mode].SyncLen * 4;

//  LineWidth = 400;

  unsigned int       i, s, TotPix, xmax;
  double             NextImgSample;
  double             t=0, slantAngle;
  unsigned char      SyncImg[700][630];
  int                x,y,xmid,x0;
  unsigned short int xAcc[700] = {0};

  double  Praw, Psync;
  unsigned char    *HasSync;
  HasSync = malloc(Length * sizeof(char));
  if (HasSync == NULL) {
    perror("FindSync: Unable to allocate memory for sync signal");
    exit(EXIT_FAILURE);
  }
  memset(HasSync,0,Length * sizeof(char));
  
  unsigned short int lines[600][(MAXSLANT-MINSLANT)*2];

  unsigned short int cy, cx;
  int                q, d, qMost, dMost;
  unsigned short int Retries = 0;
  int                maxsy = 0;
  FILE               *GrayFile;
  char               PixBuf[1] = {0};
  double             Pwr[2048];
    
  // FFT plan
  fftw_plan    Plan;
  double       *in;
  double       *out;
  unsigned int FFTLen = 1024;

  in      = fftw_malloc(sizeof(double) * FFTLen);
  if (in == NULL) {
    perror("FindSync: Unable to allocate memory for FFT");
    free(HasSync);
    exit(EXIT_FAILURE);
  }
  out     = fftw_malloc(sizeof(double) * FFTLen);
  if (out == NULL) {
    perror("FindSync: Unable to allocate memory for FFT");
    fftw_free(in);
    free(HasSync);
    exit(EXIT_FAILURE);
  }
  Plan    = fftw_plan_r2r_1d(FFTLen, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  // Create 50-point Hann window
  double Hann[50];
  for (i = 0; i < 50; i++) Hann[i] = 0.5 * (1 - cos( 2 * M_PI * i / 49.0) );

  // Zero fill input array
  memset(in, 0, FFTLen * sizeof(in[0]));
    
  unsigned int LopassBin = GetBin(3000, FFTLen);

  printf("power est.\n");

  // Power estimation
  
  for (s = 0; s < Length; s+=50) {

    // Hann window
    for (i = 0; i < 50; i++) in[i] = PCM[s+i] * Hann[i];

    // FFT
    fftw_execute(Plan);

    // Power in the whole band
    Praw = 0;
    for (i=0;i<LopassBin;i++) {
      Pwr[i] = pow(out[i], 2) + pow(out[FFTLen-i], 2);
      Praw += Pwr[i];
    }

    Praw /= (FFTLen/2.0) * ( LopassBin/(FFTLen/2.0));

    // Power around the sync band
    i = GetBin(1200+HedrShift, FFTLen);
    Psync = (Pwr[i-1] + Pwr[i] + Pwr[i+1]) / 3.0;

    // If there is more than twice the amount of Power per Hz in the
    // sync band than in the rest of the band, we have a sync signal here
    if (Psync > 2*Praw)  HasSync[s] = TRUE;
    else                 HasSync[s] = FALSE;

    for (i = 0; i < 50; i++) {
      if (s+i >= Length) break;
      HasSync[s+i] = HasSync[s];
    }

  }

  printf("hough\n");

  // Repeat until slant < 0.5° or until we give up
  while (1) {

    TotPix        = LineWidth/2; // Start at the middle of the picture
    NextImgSample = 0;
    t             = 0;
    maxsy         = 0;
    x = y = 0;

    memset(SyncImg, 0, sizeof(SyncImg[0][0]) * LineWidth * 500);
        
    // Draw the sync signal into memory
    for (s = 0; s < Length; s++) {

      // t keeps track of time in seconds
      t += 1.0/Rate;

      if (t >= NextImgSample) {

        SyncImg[x][y] = HasSync[s];

        if (y > maxsy) maxsy = y;

        TotPix++;
        x++;
        if (x >= LineWidth) {
          y++;
          x=0;
        }

        NextImgSample += ModeSpec[Mode].LineLen / (1.0 * LineWidth);
      }
    }

    // write sync.gray
    GrayFile = fopen("sync.gray","w");
    if (GrayFile == NULL) {
      perror("Unable to open sync.gray for writing");
      exit(EXIT_FAILURE);
    }
    for (y=0;y<maxsy;y++) {
      for (x=0;x<LineWidth;x++) {
        PixBuf[0] = (SyncImg[x][y] ? 255 : 0);
        fwrite(PixBuf, 1, 1, GrayFile);
      }
    }
    fclose(GrayFile);

    /** Linear Hough transform **/

    // zero arrays
    dMost = qMost = 0;
    for (d=0; d<LineWidth; d++)
      for (q=MINSLANT*2; q < MAXSLANT * 2; q++)
        lines[d][q-MINSLANT*2] = 0;

    // Find white pixels
    for (cy = 0; cy < TotPix / LineWidth; cy++) {
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
    printf("    %.1f° (d=%d) @ %.2f Hz", slantAngle, dMost, Rate);

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
      printf(" -> %.2f    recalculating\n", Rate);
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
  
  free(HasSync);
  fftw_destroy_plan(Plan);
  fftw_free(in);
  fftw_free(out);

  return (Rate);

}
