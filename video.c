#include <stdlib.h>
#include <stdio.h>
#include <complex.h>
#include <math.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "fft.h"
#include "modespec.h"
#include "pcm.h"
#include "pic.h"
#include "video.h"

#define VIDEO_MAX_WIDTH (800)
#define VIDEO_MAX_HEIGHT (616)
#define VIDEO_MAX_CHANNELS (3)

static guchar VideoImage[VIDEO_MAX_WIDTH][VIDEO_MAX_HEIGHT][VIDEO_MAX_CHANNELS] = {{{0}}};
void (*OnVideoInitBuffer)(guchar Mode);
void (*OnVideoWritePixel)(gushort x, gushort y, guchar r, guchar g, guchar b);
EventCallback OnVideoStartRedraw;
EventCallback OnVideoRefresh;
UpdateVUCallback OnVideoPowerCalculated;

typedef struct {
  int X;
  int Y;
  int Time;
  guchar Channel;
  gboolean Last;
} _PixelGrid;


/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    FALSE = Apply windowing and FFT to the signal, TRUE = Redraw from cached FFT data
 *  returns:   TRUE when finished, FALSE when aborted
 */
gboolean GetVideo(guchar Mode, double Rate, int Skip, gboolean Redraw) {
  guint      MaxBin = 0;
  guint      VideoPlusNoiseBins=0, ReceiverBins=0, NoiseOnlyBins=0;
  guint      n=0;
  guint      SyncSampleNum;
  guint      i=0, j=0;
  guint      FFTLen=1024, WinLength=0;
  guint      SyncTargetBin;
  int        SampleNum, Length, NumChans;
  int        x = 0, y = 0, tx=0, k=0;
  double     Hann[7][1024] = {{0}};
  double     Freq = 0;
  //double     PrevFreq = 0, InterpFreq = 0;
  int        NextSNRtime = 0, NextSyncTime = 0;
  double     Praw, Psync;
  double     Power[1024] = {0};
  double     Pvideo_plus_noise=0, Pnoise_only=0, Pnoise=0, Psignal=0;
  double     SNR = 0;
  double     ChanStart[4] = {0}, ChanLen[4] = {0};
  guchar     Channel = 0, WinIdx = 0;

  _PixelGrid  *PixelGrid = calloc( ModeSpec[Mode].ImgWidth * ModeSpec[Mode].NumLines * 3, sizeof(_PixelGrid) );

  // Initialize Hann windows of different lengths
  gushort HannLens[7] = { 48, 64, 96, 128, 256, 512, 1024 };
  for (j = 0; j < 7; j++)
    for (i = 0; i < HannLens[j]; i++)
      Hann[j][i] = 0.5 * (1 - cos( (2 * M_PI * i) / (HannLens[j] - 1)) );


  // Starting times of video channels on every line, counted from beginning of line
  switch (Mode) {

    case R36:
    case R24:
      ChanLen[0]   = ModeSpec[Mode].PixelTime * ModeSpec[Mode].ImgWidth * 2;
      ChanLen[1]   = ChanLen[2] = ModeSpec[Mode].PixelTime * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SyncTime + ModeSpec[Mode].PorchTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeptrTime;
      ChanStart[2] = ChanStart[1];
      break;

    case S1:
    case S2:
    case SDX:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = ModeSpec[Mode].PixelTime * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SeptrTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeptrTime;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + ModeSpec[Mode].SyncTime + ModeSpec[Mode].PorchTime;
      break;

    case PD50:
    case PD90:
    case PD120:
    case PD160:
    case PD180:
    case PD240:
    case PD290:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = ChanLen[3] = ModeSpec[Mode].PixelTime * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SyncTime + ModeSpec[Mode].PorchTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeptrTime;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + ModeSpec[Mode].SeptrTime;
      ChanStart[3] = ChanStart[2] + ChanLen[2] + ModeSpec[Mode].SeptrTime;
      break;

    default:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = ModeSpec[Mode].PixelTime * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SyncTime + ModeSpec[Mode].PorchTime;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeptrTime;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + ModeSpec[Mode].SeptrTime;
      break;
  }

  // Number of channels per line
  switch(Mode) {
    case R24BW:
    case R12BW:
    case R8BW:
      NumChans = 1;
      break;
    case R24:
    case R36:
      NumChans = 2;
      break;

    //In PD* modes, each radio frame encodes
    //4 channels, two luminance and two chroma
    case PD50:
    case PD90:
    case PD120:
    case PD160:
    case PD180:
    case PD240:
    case PD290:
      NumChans = 4;
      break;

    default:
      NumChans = 3;
      break;
  }

  // Plan ahead the time instants (in samples) at which to take pixels out
  int PixelIdx = 0;

  if (NumChans == 4){ //Woking on PD* mode
    //Each radio frame encodes two image lines
    for (y = 0; y < ModeSpec[Mode].NumLines; y += 2){
      for (Channel = 0; Channel < NumChans; Channel++){
        for (x = 0; x < ModeSpec[Mode].ImgWidth; x++){
          PixelGrid[PixelIdx].Time = (int)round(Rate * ( y/2 * ModeSpec[Mode].LineTime + ChanStart[Channel] +
                                                ModeSpec[Mode].PixelTime * 1.0 * (x + 0.5))) +
                                                Skip;
          if (Channel == 0) {
            PixelGrid[PixelIdx].X = x;
            PixelGrid[PixelIdx].Y = y;
            PixelGrid[PixelIdx].Channel = Channel;
            PixelGrid[PixelIdx].Last = FALSE;
            PixelIdx++;
          }

          else if (Channel == 1 || Channel == 2) {
            PixelGrid[PixelIdx].X = x;
            PixelGrid[PixelIdx].Y = y;
            PixelGrid[PixelIdx].Channel = Channel;
            PixelGrid[PixelIdx].Last = FALSE;
            PixelIdx++;
            PixelGrid[PixelIdx].Time = PixelGrid[PixelIdx - 1].Time;
            PixelGrid[PixelIdx].X = x;
            PixelGrid[PixelIdx].Y = y + 1;
            PixelGrid[PixelIdx].Channel = Channel;
            PixelGrid[PixelIdx].Last = FALSE;
            PixelIdx++;
          }

          else if (Channel == 3) {
            PixelGrid[PixelIdx].X = x;
            PixelGrid[PixelIdx].Y = y + 1;
            PixelGrid[PixelIdx].Channel = 0;
            PixelGrid[PixelIdx].Last = FALSE;
            PixelIdx++;
          }
        }
      }
    }
    PixelGrid[PixelIdx - 1].Last = TRUE;
  }
  else {
    for (y = 0; y < ModeSpec[Mode].NumLines; y++) {
      for (Channel = 0; Channel < NumChans; Channel++) {
        for (x = 0; x < ModeSpec[Mode].ImgWidth; x++) {
          
          if (Mode == R36 || Mode == R24) {
            if (Channel == 1) {
              if (y % 2 == 0)
                PixelGrid[PixelIdx].Channel = 1;
              else
                PixelGrid[PixelIdx].Channel = 2;
            }
            else
              PixelGrid[PixelIdx].Channel = 0;
          }
          else{
            PixelGrid[PixelIdx].Channel = Channel;
          }

          PixelGrid[PixelIdx].Time = (int)round(Rate * (y * ModeSpec[Mode].LineTime + ChanStart[Channel] +
                                                (1.0 * (x - .5) / ModeSpec[Mode].ImgWidth * ChanLen[PixelGrid[PixelIdx].Channel]))) +
                                                Skip;
          PixelGrid[PixelIdx].X = x;
          PixelGrid[PixelIdx].Y = y;

          PixelGrid[PixelIdx].Last = FALSE;

          PixelIdx++;
        }
      }
    }
    PixelGrid[PixelIdx - 1].Last = TRUE;
  }

  for (k = 0; k < PixelIdx; k++) {
    if (PixelGrid[k].Time >= 0) {
      PixelIdx = k;
      break;
    }
  }

  /*case PD50:
        case PD90:
        case PD120:
        case PD160:
        case PD180:
        case PD240:
        case PD290:
          if (CurLineTime >= ChanStart[2] + ChanLen[2]) Channel = 3; // ch 0 of even line
          else if (CurLineTime >= ChanStart[2])         Channel = 2;
          else if (CurLineTime >= ChanStart[1])         Channel = 1;
          else                                          Channel = 0;
          break;*/

  // Initialize pixbuffer
  if ((!Redraw) && OnVideoInitBuffer) {
    OnVideoInitBuffer(Mode);
  }

  if (OnVideoStartRedraw) {
    OnVideoStartRedraw();
  }

  if(NumChans == 4) //In PD* modes, each radio frame encodes two image lines
    Length = ModeSpec[Mode].LineTime * ModeSpec[Mode].NumLines/2 * 44100;
  else
    Length = ModeSpec[Mode].LineTime * ModeSpec[Mode].NumLines * 44100;
  SyncTargetBin = GetBin(1200 + CurrentPic.HedrShift, FFTLen);
  Abort = FALSE;
  SyncSampleNum = 0;

  // Loop through signal
  for (SampleNum = 0; SampleNum < Length; SampleNum++) {

    if (!Redraw) {

      /*** Read ahead from sound card ***/

      if (pcm.WindowPtr == 0 || pcm.WindowPtr >= BUFLEN-1024) readPcm(2048);
     

      /*** Store the sync band for later adjustments ***/

      if (SampleNum == NextSyncTime) {
 
        Praw = Psync = 0;

        memset(fft.in, 0, sizeof(double)*FFTLen);
       
        // Hann window
        for (i = 0; i < 64; i++) fft.in[i] = pcm.Buffer[pcm.WindowPtr+i-32] / 32768.0 * Hann[1][i];

        fftw_execute(fft.Plan1024);

        for (i=GetBin(1500+CurrentPic.HedrShift,FFTLen); i<=GetBin(2300+CurrentPic.HedrShift, FFTLen); i++)
          Praw += power(fft.out[i]);

        for (i=SyncTargetBin-1; i<=SyncTargetBin+1; i++)
          Psync += power(fft.out[i]) * (1- .5*abs((gint)(SyncTargetBin-i)));

        Praw  /= (GetBin(2300+CurrentPic.HedrShift, FFTLen) - GetBin(1500+CurrentPic.HedrShift, FFTLen));
        Psync /= 2.0;

        // If there is more than twice the amount of power per Hz in the
        // sync band than in the video band, we have a sync signal here
        HasSync[SyncSampleNum] = (Psync > 2*Praw);

        NextSyncTime += 13;
        SyncSampleNum ++;

      }



      /*** Estimate SNR ***/

      if (SampleNum == NextSNRtime) {
        
        memset(fft.in, 0, sizeof(double)*FFTLen);

        // Apply Hann window
        for (i = 0; i < FFTLen; i++) fft.in[i] = pcm.Buffer[pcm.WindowPtr + i - FFTLen/2] / 32768.0 * Hann[6][i];

        fftw_execute(fft.Plan1024);

        // Calculate video-plus-noise power (1500-2300 Hz)

        Pvideo_plus_noise = 0;
        for (n = GetBin(1500+CurrentPic.HedrShift, FFTLen); n <= GetBin(2300+CurrentPic.HedrShift, FFTLen); n++)
          Pvideo_plus_noise += power(fft.out[n]);

        // Calculate noise-only power (400-800 Hz + 2700-3400 Hz)

        Pnoise_only = 0;
        for (n = GetBin(400+CurrentPic.HedrShift,  FFTLen); n <= GetBin(800+CurrentPic.HedrShift, FFTLen);  n++)
          Pnoise_only += power(fft.out[n]);

        for (n = GetBin(2700+CurrentPic.HedrShift, FFTLen); n <= GetBin(3400+CurrentPic.HedrShift, FFTLen); n++)
          Pnoise_only += power(fft.out[n]);

        // Bandwidths
        VideoPlusNoiseBins = GetBin(2300, FFTLen) - GetBin(1500, FFTLen) + 1;

        NoiseOnlyBins      = GetBin(800,  FFTLen) - GetBin(400,  FFTLen) + 1 +
                             GetBin(3400, FFTLen) - GetBin(2700, FFTLen) + 1;

        ReceiverBins       = GetBin(3400, FFTLen) - GetBin(400,  FFTLen);

        // Eq 15
        Pnoise  = Pnoise_only * (1.0 * ReceiverBins / NoiseOnlyBins);
        Psignal = Pvideo_plus_noise - Pnoise_only * (1.0 * VideoPlusNoiseBins / NoiseOnlyBins);

        // Lower bound to -20 dB
        SNR = ((Psignal / Pnoise < .01) ? -20 : 10 * log10(Psignal / Pnoise));

        NextSNRtime += 256;
      }



      /*** FM demodulation ***/

      if (SampleNum % 6 == 0) { // Take FFT every 6 samples

        //PrevFreq = Freq;

        // Adapt window size to SNR

        if      (!Adaptive)  WinIdx = 0;
        
        else if (SNR >=  20) WinIdx = 0;
        else if (SNR >=  10) WinIdx = 1;
        else if (SNR >=   9) WinIdx = 2;
        else if (SNR >=   3) WinIdx = 3;
        else if (SNR >=  -5) WinIdx = 4;
        else if (SNR >= -10) WinIdx = 5;
        else                 WinIdx = 6;

        // Minimum winlength can be doubled for Scottie DX
        if (Mode == SDX && WinIdx < 6) WinIdx++;

        memset(fft.in, 0, sizeof(double)*FFTLen);
        memset(Power,  0, sizeof(double)*1024);

        // Apply window function
        
        WinLength = HannLens[WinIdx];
        for (i = 0; i < WinLength; i++) fft.in[i] = pcm.Buffer[pcm.WindowPtr + i - WinLength/2] / 32768.0 * Hann[WinIdx][i];

        fftw_execute(fft.Plan1024);

        MaxBin = 0;
          
        // Find the bin with most power
        for (n = GetBin(1500 + CurrentPic.HedrShift, FFTLen) - 1; n <= GetBin(2300 + CurrentPic.HedrShift, FFTLen) + 1; n++) {

          Power[n] = power(fft.out[n]);
          if (MaxBin == 0 || Power[n] > Power[MaxBin]) MaxBin = n;

        }

        // Find the peak frequency by Gaussian interpolation
        if (MaxBin > GetBin(1500 + CurrentPic.HedrShift, FFTLen) - 1 && MaxBin < GetBin(2300 + CurrentPic.HedrShift, FFTLen) + 1) {
          Freq = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                           (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
          // In Hertz
          Freq = Freq / FFTLen * 44100;
        } else {
          // Clip if out of bounds
          Freq = ( (MaxBin > GetBin(1900 + CurrentPic.HedrShift, FFTLen)) ? 2300 : 1500 ) + CurrentPic.HedrShift;
        }

      } /* endif (SampleNum == PixelGrid[PixelIdx].Time) */

      // Linear interpolation of (chronologically) intermediate frequencies, for redrawing
      //InterpFreq = PrevFreq + (Freq-PrevFreq) * ...  // TODO!

      // Calculate luminency & store for later use
      StoredLum[SampleNum] = clip((Freq - (1500 + CurrentPic.HedrShift)) / 3.1372549);

    } /* endif (!Redraw) */


    if (SampleNum == PixelGrid[PixelIdx].Time) {
      
      //In PD* modes, two pixels need data from the same sample
      //Can't move on from SampleNum, until all are processed
      while (SampleNum == PixelGrid[PixelIdx].Time) {
        x = PixelGrid[PixelIdx].X;
        y = PixelGrid[PixelIdx].Y;
        Channel = PixelGrid[PixelIdx].Channel;

        // Store pixel
        VideoImage[x][y][Channel] = StoredLum[SampleNum];

        // Some modes have R-Y & B-Y channels that are twice the height of the Y channel
        if (Channel > 0 && (Mode == R36 || Mode == R24))
          VideoImage[x][y+1][Channel] = StoredLum[SampleNum];

        // Calculate and draw pixels to pixbuf on line change
        if (x == ModeSpec[Mode].ImgWidth - 1 || PixelGrid[PixelIdx].Last) {
          for (tx = 0; tx < ModeSpec[Mode].ImgWidth; tx++) {
            guchar r = 0, g = 0, b = 0;

            switch(ModeSpec[Mode].ColorEnc) {

            case RGB:
              r = VideoImage[tx][y][0];
              g = VideoImage[tx][y][1];
              b = VideoImage[tx][y][2];
              break;

            case GBR:
              r = VideoImage[tx][y][2];
              g = VideoImage[tx][y][0];
              b = VideoImage[tx][y][1];
              break;

            case YUV:
              r = clip((100 * VideoImage[tx][y][0] + 140 * VideoImage[tx][y][1] - 17850) / 100.0);
              g = clip((100 * VideoImage[tx][y][0] -  71 * VideoImage[tx][y][1] - 33 *
                  VideoImage[tx][y][2] + 13260) / 100.0);
              b = clip((100 * VideoImage[tx][y][0] + 178 * VideoImage[tx][y][2] - 22695) / 100.0);
              break;

            case BW:
              r = g = b = VideoImage[tx][y][0];
              break;
            }

            if (OnVideoWritePixel) {
              OnVideoWritePixel(tx, y, r, g, b);
            }
          }

          if ((!Redraw || y % 5 == 0 || PixelGrid[PixelIdx].Last) && OnVideoRefresh) {
            OnVideoRefresh();
          }
        }

        PixelIdx++;
      }
    } /* endif (SampleNum == PixelGrid[PixelIdx].Time) */

    if (!Redraw && (SampleNum % 8820 == 0) && OnVideoPowerCalculated) {
      OnVideoPowerCalculated(Power, FFTLen, WinIdx);
    }

    if (Abort) {
      free(PixelGrid);
      return FALSE;
    }

    pcm.WindowPtr ++;

  }

  free(PixelGrid);
  return TRUE;

}
