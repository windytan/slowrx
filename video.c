#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <samplerate.h>

#include <fftw3.h>

#include "common.h"

/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    false = Apply windowing and FFT to the signal, true = Redraw from cached FFT data
 *  returns:   true when finished, false when aborted
 */
bool GetVideo(guchar Mode, double Rate, int Skip, bool Redraw) {

  guint      MaxBin = 0;
  guint      VideoPlusNoiseBins=0, ReceiverBins=0, NoiseOnlyBins=0;
  guint      n=0;
  guint      SyncSampleNum;
  guint      i=0, j=0;
  guint      FFTLen=1024, WinLength=0;
  guint      LopassBin,SyncTargetBin;
  int        LineNum = 0, SampleNum, Length;
  int        x = 0, y = 0, prevline=0, tx=0;
  double     Hann[7][1024] = {{0}};
  double     t=0, Freq = 0, PrevFreq = 0, InterpFreq = 0, NextPixelTime = 0, NextSNRtime = 0, NextFFTtime = 0;
  double     NextSyncTime = 0;
  double     Praw, Psync;
  double     Power[1024] = {0};
  double     Pvideo_plus_noise=0, Pnoise_only=0, Pnoise=0, Psignal=0;
  double     SNR = 0;
  double     CurLineTime = 0;
  double     ChanStart[4] = {0}, ChanLen[4] = {0};
  guchar     Image[800][616][3] = {{{0}}};
  guchar     Channel = 0, WinIdx = 0;
    
  // Initialize Hann windows of different lengths
  gushort HannLens[7] = { 32, 64, 96, 128, 256, 512, 1024 };
  for (j = 0; j < 7; j++)
    for (i = 0; i < HannLens[j]; i++)
      Hann[j][i] = 0.5 * (1 - cos( (2 * M_PI * i) / (HannLens[j] - 1)) );

  // Starting times of video channels on every line, counted from beginning of line

  switch (Mode) {

    case R36:
    case R24:
      ChanLen[0]   = ModeSpec[Mode].PixelLen * ModeSpec[Mode].ImgWidth * 2;
      ChanLen[1]   = ChanLen[2] = ModeSpec[Mode].PixelLen * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SyncLen + ModeSpec[Mode].PorchLen;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeparatorLen;
      ChanStart[2] = ChanStart[1];
      break;

    case S1:
    case S2:
    case SDX:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = ModeSpec[Mode].PixelLen * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SeparatorLen;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeparatorLen;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + ModeSpec[Mode].SyncLen + ModeSpec[Mode].PorchLen;
      break;

    default:
      ChanLen[0]   = ChanLen[1] = ChanLen[2] = ModeSpec[Mode].PixelLen * ModeSpec[Mode].ImgWidth;
      ChanStart[0] = ModeSpec[Mode].SyncLen + ModeSpec[Mode].PorchLen;
      ChanStart[1] = ChanStart[0] + ChanLen[0] + ModeSpec[Mode].SeparatorLen;
      ChanStart[2] = ChanStart[1] + ChanLen[1] + ModeSpec[Mode].SeparatorLen;
      break;

  }

  // Initialize pixbuffer
  if (!Redraw) {
    g_object_unref(pixbuf_rx);
    pixbuf_rx = gdk_pixbuf_new (GDK_COLORSPACE_RGB, false, 8, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight);
    gdk_pixbuf_fill(pixbuf_rx, 0);
  }

  int     rowstride = gdk_pixbuf_get_rowstride (pixbuf_rx);
  guchar *pixels, *p;
  pixels = gdk_pixbuf_get_pixels(pixbuf_rx);
          
  g_object_unref(pixbuf_disp);
  pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
      500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_BILINEAR);

  gdk_threads_enter();
  gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);
  gdk_threads_leave();

  Length        = ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight * 44100;
  SyncTargetBin = GetBin(1200+CurrentPic.HedrShift, FFTLen);
  LopassBin     = GetBin(5000, FFTLen);
  Abort         = false;
  SyncSampleNum = 0;

  // Loop through signal
  for (SampleNum = 0; SampleNum < Length; SampleNum++) {

    t = (SampleNum - Skip) / Rate;

    CurLineTime = fmod(t, ModeSpec[Mode].LineLen);

    if (!Redraw) {

      /*** Read ahead from sound card ***/

      if (pcm.WindowPtr == 0 || pcm.WindowPtr >= BUFLEN-1024) readPcm(2048);
     

      /*** Store the sync band for later adjustments ***/

      if (t >= NextSyncTime) {
 
        Praw = Psync = 0;

        memset(in,  0, sizeof(in[0]) *FFTLen);
        memset(out, 0, sizeof(out[0])*FFTLen);
       
        // Hann window
        for (i = 0; i < 64; i++) in[i] = pcm.Buffer[pcm.WindowPtr+i-32] / 32768.0 * Hann[1][i];

        fftw_execute(Plan1024);

        for (i=0;i<LopassBin;i++) {
          if (i >= GetBin(1500+CurrentPic.HedrShift, FFTLen) && i <= GetBin(2300+CurrentPic.HedrShift, FFTLen))
              Praw += pow(out[i], 2) + pow(out[FFTLen-i], 2);

          if (i >= SyncTargetBin-1 && i <= SyncTargetBin+1)
            Psync += (pow(out[i], 2) + pow(out[FFTLen-i], 2)) * (1- .5*abs(SyncTargetBin-i));
        }

        Praw  /= (GetBin(2300+CurrentPic.HedrShift, FFTLen) - GetBin(1500+CurrentPic.HedrShift, FFTLen));
        Psync /= 2.0;

        // If there is more than twice the amount of power per Hz in the
        // sync band than in the video band, we have a sync signal here
        if (Psync > 2*Praw)  HasSync[SyncSampleNum] = true;
        else                 HasSync[SyncSampleNum] = false;

        NextSyncTime += SYNCPIXLEN;
        SyncSampleNum ++;

      }


      /*** Estimate SNR ***/

      if (t >= NextSNRtime) {

        // Apply Hann window
        for (i = 0; i < FFTLen; i++) in[i] = pcm.Buffer[pcm.WindowPtr + i - FFTLen/2] / 32768.0 * Hann[6][i];

        // FFT
        fftw_execute(Plan1024);

        // Calculate video-plus-noise power (1500-2300 Hz)

        Pvideo_plus_noise = 0;
        for (n = GetBin(1500+CurrentPic.HedrShift, FFTLen); n <= GetBin(2300+CurrentPic.HedrShift, FFTLen); n++)
          Pvideo_plus_noise += pow(out[n], 2) + pow(out[FFTLen - n], 2);

        // Calculate noise-only power (400-800 Hz + 2700-3400 Hz)

        Pnoise_only = 0;
        for (n = GetBin(400+CurrentPic.HedrShift,  FFTLen); n <= GetBin(800+CurrentPic.HedrShift, FFTLen);  n++)
          Pnoise_only += pow(out[n], 2) + pow(out[FFTLen - n], 2);

        for (n = GetBin(2700+CurrentPic.HedrShift, FFTLen); n <= GetBin(3400+CurrentPic.HedrShift, FFTLen); n++)
          Pnoise_only += pow(out[n], 2) + pow(out[FFTLen - n], 2);

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

        NextSNRtime += 8e-3;
      }


      /*** FM demodulation ***/

      if (t >= NextFFTtime) {

        PrevFreq = Freq;

        // Adapt window size to SNR

        if      (!Adaptive)  WinIdx = 0;

        else if (SNR >=  30) WinIdx = 0;
        else if (SNR >=  10) WinIdx = 1;
        else if (SNR >=   9) WinIdx = 2;
        else if (SNR >=   3) WinIdx = 3;
        else if (SNR >=  -5) WinIdx = 4;
        else if (SNR >= -10) WinIdx = 5;
        else                 WinIdx = 6;

        // Minimum winlength can be doubled for Scottie DX
        if (Mode == SDX && WinIdx < 6) WinIdx++;

        WinLength = HannLens[WinIdx];

        memset(in, 0, sizeof(double)*FFTLen);

        // Apply window function

        for (i = 0; i < WinLength; i++) in[i] = pcm.Buffer[pcm.WindowPtr + i - WinLength/2] / 32768.0 * Hann[WinIdx][i];

        fftw_execute(Plan1024);

        MaxBin = 0;
          
        // Find the bin with most power
        for (n = GetBin(1500 + CurrentPic.HedrShift, FFTLen) - 1; n <= GetBin(2300 + CurrentPic.HedrShift, FFTLen) + 1; n++) {

          Power[n] = pow(out[n],2) + pow(out[FFTLen - n], 2);
          if (MaxBin == 0 || Power[n] > Power[MaxBin]) MaxBin = n;

        }

        // Find the peak frequency by Gaussian interpolation
        if (MaxBin > GetBin(1500 + CurrentPic.HedrShift, FFTLen) - 1 && MaxBin < GetBin(2300 + CurrentPic.HedrShift, FFTLen) + 1) {
          Freq = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                           (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
          // In Hertz
          Freq = Freq / FFTLen * 44100;
          InterpFreq = Freq;
        } else {
          // Clip if out of bounds
          Freq = ( (MaxBin > GetBin(1900 + CurrentPic.HedrShift, FFTLen)) ? 2300 : 1500 ) + CurrentPic.HedrShift;
        }

        NextFFTtime += 0.3e-3;

      }

      // Linear interpolation of (chronologically) intermediate frequencies
      InterpFreq = PrevFreq + (t - NextFFTtime + 0.6e-3) * ((Freq - PrevFreq) / 0.3e-3);

      // Calculate luminency & store for later use
      StoredLum[SampleNum] = clip((InterpFreq - (1500 + CurrentPic.HedrShift)) / 3.1372549);

    }


    /*** Are we on a video line, and should we sample a pixel? ***/

    if ( ( (CurLineTime >= ChanStart[0] && CurLineTime < ChanStart[0] + ChanLen[0])
        || (CurLineTime >= ChanStart[1] && CurLineTime < ChanStart[1] + ChanLen[1])
        || (CurLineTime >= ChanStart[2] && CurLineTime < ChanStart[2] + ChanLen[2]) )
        && t >= NextPixelTime
       ) {

      LineNum = t / ModeSpec[Mode].LineLen;

      // Which channel is this?
      switch(Mode) {

        case R24BW:
        case R12BW:
        case R8BW:
          Channel = 0;
          break;

        case R36:
        case R24:
          if (CurLineTime >= ChanStart[1]) {
            if (LineNum % 2 == 0) Channel = 1;
            else                  Channel = 2;
          } else                  Channel = 0;
          break;

        case PD50:
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
          break;

        default:
          if      (CurLineTime >= ChanStart[2])         Channel = 2;
          else if (CurLineTime >= ChanStart[1])         Channel = 1;
          else                                          Channel = 0;
          break;

      }

      // X coordinate of this pixel
      x = (CurLineTime - ChanStart[Channel]) / ChanLen[Channel] * ModeSpec[Mode].ImgWidth;

      // Y coordinate of this pixel
      switch(Channel) {
        case 3:
          y = LineNum + 1;
          Channel = 0;
          break;

        default:
          y = LineNum;
          break;
      }

      // Store pixel 
      if (x >= 0 && y >= 0 && x < ModeSpec[Mode].ImgWidth) {
        Image[x][y][Channel] = StoredLum[SampleNum];
        // Some modes have R-Y & B-Y channels that are twice the height of the Y channel
        if (Channel > 0)
          switch(Mode) {
            case R36:
            case R24:
              if (y < ModeSpec[Mode].ImgHeight-1) Image[x][y+1][Channel] = StoredLum[SampleNum];
              break;
          }
      }

      if (y > ModeSpec[Mode].ImgHeight-1) break;

      // Calculate and draw pixels to pixbuf on line change
      if (LineNum != prevline || (LineNum == ModeSpec[Mode].ImgHeight-1 && x == ModeSpec[Mode].ImgWidth-1)) {
        for (tx = 0; tx < ModeSpec[Mode].ImgWidth; tx++) {
          p = pixels + prevline * rowstride + tx * 3;

          switch(ModeSpec[Mode].ColorEnc) {

            case RGB:
              p[0] = Image[tx][prevline][0];
              p[1] = Image[tx][prevline][1];
              p[2] = Image[tx][prevline][2];
              break;

            case GBR:
              p[0] = Image[tx][prevline][2];
              p[1] = Image[tx][prevline][0];
              p[2] = Image[tx][prevline][1];
              break;

            case YUV:
              p[0] = clip((100 * Image[tx][prevline][0] + 140 * Image[tx][prevline][1] - 17850) / 100.0);
              p[1] = clip((100 * Image[tx][prevline][0] -  71 * Image[tx][prevline][1] - 33 *
                  Image[tx][prevline][2] + 13260) / 100.0);
              p[2] = clip((100 * Image[tx][prevline][0] + 178 * Image[tx][prevline][2] - 22695) / 100.0);
              break;

            case BW:
              p[0] = p[1] = p[2] = Image[tx][prevline][0];
              break;

          }
        }

        if (!Redraw || LineNum % 5 == 0 || LineNum == ModeSpec[Mode].ImgHeight-1) {
          // Scale and update image
          g_object_unref(pixbuf_disp);
          pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
              500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_BILINEAR);

          gdk_threads_enter();
          gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);
          gdk_threads_leave();
        }
      }
      prevline = LineNum;

      NextPixelTime += ModeSpec[Mode].PixelLen / 2;
    }

    if (!Redraw && SampleNum % 8820 == 0) {
      setVU(pcm.PeakVal, SNR);
      pcm.PeakVal = 0;
    }

    if (Abort) return false;

    pcm.WindowPtr ++;

  }

  return true;

}
