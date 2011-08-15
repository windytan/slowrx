#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <fftw3.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

/* Demodulate the video signal
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    false = Apply windowing and FFT to the signal, true = Redraw from cached FFT data
 *  returns:   TRUE when finished, FALSE when aborted
 */
gboolean GetVideo(guchar Mode, guint Rate, int Skip, gboolean Redraw) {

  guint      MaxBin = 0;
  guint      VideoPlusNoiseBins=0, ReceiverBins=0, NoiseOnlyBins=0;
  guint      n=0;
  int        i=0, j=0,TargetBin=0;
  int        Length=0, Sample=0;
  int        FFTLen=0, WinLength=0,syncs,ffts,bigffts,snrs;
  int        samplesread = 0, WinIdx = 0, LineNum = 0;
  int        x = 0, y = 0, prevline=0, tx=0, MaxPcm=0;
  gushort    HannLens[7] = { 64, 96, 128, 256, 512, 1024, 2048 };
  gushort    LopassBin;
  double     Hann[7][2048] = {{0}};
  double     t=0, Freq = 0, PrevFreq = 0, InterpFreq = 0, NextPixel = 0, NextSNRtime = 0, NextFFTtime = 0;
  double     NextSyncTime = 0;
  double    *in,  *out;
  double     Praw, Psync;
  double    *PCM;
  double     Power[2048] = {0};
  double     Pvideo_plus_noise=0, Pnoise_only=0, Pnoise=0, Psignal=0;
  double     SNR = 0;
  double     CurLineTime = 0;
  double     ChanStart[4] = {0}, ChanLen[4] = {0};
  guchar     Lum=0, Image[800][616][3] = {{{0}}};
  guchar     Channel = 0;
  fftw_plan  Plan, BigPlan, SNRPlan;
    
  // Allocate space for PCM
  PCM = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * SRATE, sizeof(double));
  if (PCM == NULL) {
    perror("Listen: Unable to allocate memory for PCM");
    exit(EXIT_FAILURE);
  }

  // Prepare FFT
  in      = fftw_malloc(sizeof(double) * 2048);
  if (in == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    free(PCM);
    exit(EXIT_FAILURE);
  }
  out     = fftw_malloc(sizeof(double) * 2048);
  if (out == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(in);
    free(PCM);
    exit(EXIT_FAILURE);
  }
  
  // FFTW plans for frequency estimation
  Plan    = fftw_plan_r2r_1d(512,  in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  BigPlan = fftw_plan_r2r_1d(1024, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  // FFTW plan for SNR estimation
  SNRPlan = fftw_plan_r2r_1d(SNRSIZE, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  // Initialize Hann windows of different lengths
  for (j = 0; j < 7; j++)
    for (i = 0; i < HannLens[j]; i++)
      Hann[j][i] = 0.5 * (1 - cos( (2 * M_PI * i) / (HannLens[j] - 1)) );

  // Initialize 37-point Dolph-Chebyshev window for frequency estimation in HQ cases
  double Cheb[37] =
      { 0.1569882, 0.1206692, 0.1631808, 0.2122111, 0.2673747, 0.3280227, 0.3932469, 0.4618960,
        0.5326043, 0.6038308, 0.6739095, 0.7411060, 0.8036807, 0.8599540, 0.9083715, 0.9475647,
        0.9764067, 0.9940579, 1.0000000, 0.9940579, 0.9764067, 0.9475647, 0.9083715, 0.8599540,
        0.8036807, 0.7411060, 0.6739095, 0.6038308, 0.5326043, 0.4618960, 0.3932469, 0.3280227,
        0.2673747, 0.2122111, 0.1631808, 0.1206692, 0.1569882 };


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

    g_object_unref(RxPixbuf);
    RxPixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight);
    ClearPixbuf(RxPixbuf, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight);
  }

  int rowstride = gdk_pixbuf_get_rowstride (RxPixbuf);
  guchar *pixels, *p;
  pixels = gdk_pixbuf_get_pixels(RxPixbuf);

  if (!Redraw) StoredFreqRate = Rate;

  Length = (ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE;

  Abort = FALSE;

  snrs = bigffts = ffts = syncs = 0;

  // Loop through signal
  for (Sample = 0; Sample < Length; Sample++) {

    t = (1.0 * Sample - Skip) / Rate;

    CurLineTime = fmod(t, ModeSpec[Mode].LineLen);

    if (Redraw) {

      // We're redrawing, so all DSP is skipped

      InterpFreq = StoredFreq[Sample];

    } else {

      /*** Read ahead from sound card ***/

      if (Sample == 0 || Sample >= PcmPointer - 2048) {
        if (PcmPointer > Length-2048) break;

        samplesread = snd_pcm_readi(pcm_handle, PcmBuffer, 2048);
        if (samplesread < 2048) {
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
          break;
        }

        for (i = 0; i < 2048; i++) {

          PCM[PcmPointer + i] = PcmBuffer[i] / 32768.0;

         // Keep track of max amplitude for VU meter
         if (abs(PcmBuffer[i]) > MaxPcm) MaxPcm = abs(PcmBuffer[i]);

        }
        PcmPointer += 2048;
      }


      /*** Save the sync band for later adjustments ***/

      if (t >= NextSyncTime) {
 
        Praw = Psync = 0;

        TargetBin = GetBin(1200+HedrShift, 1024);
        LopassBin = GetBin(3000, 1024);
        
        memset(in,  0, sizeof(in[0]) *2048);
        memset(out, 0, sizeof(out[0])*2048);
        
        for (i = 0; i < 64; i++) in[i] = PCM[Sample+i] * Hann[0][i];

        fftw_execute(BigPlan);

        syncs ++;

        for (i=0;i<LopassBin;i++) {
          Praw += pow(out[i], 2) + pow(out[1024-i], 2);
          if (i >= TargetBin-1 && i <= TargetBin+1) Psync += pow(out[i], 2) + pow(out[1024-i], 2);
        }

        Praw  /= (1024/2.0) * ( LopassBin/(1024/2.0));
        Psync /= 3.0;

        // If there is more than twice the amount of power per Hz in the
        // sync band than in the rest of the band, we have a sync signal here
        if (Psync > 2*Praw)  HasSync[Sample] = TRUE;
        else                 HasSync[Sample] = FALSE;

        // Copy forward 
        for (i = 0; i < 64; i++) {
          if (Sample+i >= Length) break;
          HasSync[Sample+i] = HasSync[Sample];
        }

        NextSyncTime += 64.0/SRATE;
      }


      /*** Estimate SNR ***/

      if (t >= NextSNRtime) {

        // Apply Hann window to SNRSIZE samples
        for (i = 0; i < SNRSIZE; i++) in[i] = PCM[Sample + i] * Hann[6][i];

        // FFT
        fftw_execute(SNRPlan);

        snrs ++;

        // Calculate video-plus-noise power (1500-2300 Hz)

        Pvideo_plus_noise = 0;
        for (n = GetBin(1500+HedrShift, SNRSIZE); n <= GetBin(2300+HedrShift, SNRSIZE); n++)
          Pvideo_plus_noise += pow(out[n], 2) + pow(out[SNRSIZE - n], 2);

        // Calculate noise-only power (400-800 Hz + 2700-3400 Hz)

        Pnoise_only = 0;
        for (n = GetBin(400+HedrShift,  SNRSIZE); n <= GetBin(800+HedrShift, SNRSIZE);  n++)
          Pnoise_only += pow(out[n], 2) + pow(out[SNRSIZE - n], 2);

        for (n = GetBin(2700+HedrShift, SNRSIZE); n <= GetBin(3400+HedrShift, SNRSIZE); n++)
          Pnoise_only += pow(out[n], 2) + pow(out[SNRSIZE - n], 2);

        // Bandwidths
        VideoPlusNoiseBins = GetBin(2300, SNRSIZE) - GetBin(1500, SNRSIZE) + 1;

        NoiseOnlyBins      = GetBin(800,  SNRSIZE) - GetBin(400,  SNRSIZE) + 1 +
                             GetBin(3400, SNRSIZE) - GetBin(2700, SNRSIZE) + 1;

        ReceiverBins       = GetBin(3400, SNRSIZE) - GetBin(400,  SNRSIZE);

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

        FFTLen = 512;

        if        (!Adaptive)   WinLength = 37;
        else if   (SNR >= 30)   WinLength = 37;
        else {
          if      (SNR < -10) { WinIdx = 5; FFTLen = 1024; }
          else if (SNR < -5)    WinIdx = 4;
          else if (SNR < 3)     WinIdx = 3;
          else if (SNR < 9)     WinIdx = 2;
          else if (SNR < 10)    WinIdx = 1;
          else                  WinIdx = 0;

          WinLength = HannLens[WinIdx];

        }

        // Halve the window size for M2 and S2, except under excellent or hopeless SNR
        if ( (Mode == M2 || Mode == S2) && WinLength > 64 && WinLength < 512) {
          WinLength /= 2;
          WinIdx --;
        }

        memset(in, 0, sizeof(double)*2048);

        // Select window function based on SNR

        if (Adaptive && SNR < 30) {

          // Apply Hann window
          for (i = 0; i < WinLength; i++)
            in[i] = (Sample + i - (WinLength >> 1) < 0) ? 0 : PCM[Sample + i - (WinLength >> 1)] * Hann[WinIdx][i];

        } else {

          // Apply Chebyshev window
           for (i = 0; i < 37; i++) in[i] = (Sample + i >= (37>>1) ? PCM[Sample + i - (37 >> 1)] * Cheb[i] : 0);
        }

        // FFT
        if (FFTLen == 1024) fftw_execute(BigPlan);
        else                fftw_execute(Plan);

        if (FFTLen == 1024) bigffts ++;
        else                ffts++;

        MaxBin = 0;
          
        // Find the bin with most power
        for (n = GetBin(1500 + HedrShift, FFTLen) - 2; n <= GetBin(2300 + HedrShift, FFTLen) + 2; n++) {

          Power[n] = pow(out[n],2) + pow(out[FFTLen - n], 2);
          if (MaxBin == 0 || Power[n] > Power[MaxBin]) MaxBin = n;

        }

        // Find the exact frequency by Gaussian interpolation
        if (MaxBin > GetBin(1500 + HedrShift, FFTLen) - 2 && MaxBin < GetBin(2300 + HedrShift, FFTLen) + 2) {
          Freq = MaxBin +            (log( Power[MaxBin + 1] / Power[MaxBin - 1] )) /
                           (2 * log( pow(Power[MaxBin], 2) / (Power[MaxBin + 1] * Power[MaxBin - 1])));
          // In Hertz
          Freq = Freq / FFTLen * SRATE;
          InterpFreq = Freq;
        } else {
          // Clip if out of bounds
          Freq = (MaxBin > GetBin(1900 + HedrShift, FFTLen)) ? 2300+HedrShift : 1500+HedrShift;
        }


        NextFFTtime += 0.4e-3;

      } else {

        // Linear interpolation of intermediate frequencies
        
        InterpFreq = PrevFreq + (t - NextFFTtime + 0.8e-3) * ((Freq - PrevFreq) / 0.4e-3);

      }

      // Store frequency for later image adjustments
      StoredFreq[Sample] = InterpFreq;

    }


    /*** Are we on a video line, and should we sample a pixel? ***/

    if ( ( (CurLineTime >= ChanStart[0] && CurLineTime < ChanStart[0] + ChanLen[0])
        || (CurLineTime >= ChanStart[1] && CurLineTime < ChanStart[1] + ChanLen[1])
        || (CurLineTime >= ChanStart[2] && CurLineTime < ChanStart[2] + ChanLen[2]) )
        && t >= NextPixel
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

      // Luminance from frequency
      Lum = clip((InterpFreq - (1500 + HedrShift)) / 3.1372549);

      // Store pixel 
      if (x >= 0 && y >= 0 && x < ModeSpec[Mode].ImgWidth) {
        Image[x][y][Channel] = Lum;
        // Some modes have R-Y & B-Y channels that are twice the height of the Y channel
        if (Channel > 0)
          switch(Mode) {
            case R36:
            case R24:
              if (y < ModeSpec[Mode].ImgHeight-1) Image[x][y+1][Channel] = Lum;
              break;
          }

      }

      if (y > ModeSpec[Mode].ImgHeight-1) {
        printf("y > ImgHeight-1\n");
        break;
      }

      // Calculate and draw pixels on line change 
      if (LineNum != prevline) {
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
          g_object_unref(DispPixbuf);
          DispPixbuf = gdk_pixbuf_scale_simple(RxPixbuf, 500,
              500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_NEAREST);

          gdk_threads_enter();
          gtk_image_set_from_pixbuf(GTK_IMAGE(RxImage), DispPixbuf);
          gdk_threads_leave();
        }
      }
      prevline = LineNum;

      NextPixel += ModeSpec[Mode].PixelLen / 2;
    }

    if (!Redraw && Sample % 8820 == 0) {
      setVU(MaxPcm, SNR);
      MaxPcm = 0;
    }

    if (Abort) break;

  }

  // High-quality scaling when finished
  if (Redraw || !(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(togslant)))) {
    g_object_unref(DispPixbuf);
    DispPixbuf = gdk_pixbuf_scale_simple(RxPixbuf, 500,
        500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_HYPER);

    gdk_threads_enter();
    gtk_image_set_from_pixbuf(GTK_IMAGE(RxImage), DispPixbuf);
    gdk_threads_leave();
  }

  fftw_destroy_plan(Plan);
  fftw_destroy_plan(BigPlan);
  fftw_destroy_plan(SNRPlan);
  fftw_free(in);
  fftw_free(out);

  free(PCM);

  if (Abort) return FALSE;
  else       return TRUE;

}
