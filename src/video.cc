#include "common.hh"

typedef struct {
  int X;
  int Y;
  int Channel;
  double Time;
} _Pixel;

bool sortPixelTime(_Pixel a, _Pixel b) { return a.Time < b.Time; }

/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    false = Apply windowing and FFT to the signal, true = Redraw from cached FFT data
 *  returns:   true when finished, false when aborted
 */
bool GetVideo(SSTVMode Mode, double Rate, DSPworker* dsp, bool Redraw) {

  vector<_Pixel> PixelGrid;

  _ModeSpec Spec = ModeSpec[Mode];

  guint8 Image[800][800][3];

  printf("Mode %d: %dx%d\n",Mode,Spec.ImgWidth, Spec.NumLines);

  // Time instants for all pixels
  for (int y=0; y<Spec.NumLines; y++) {
    for (int x=0; x<Spec.ImgWidth; x++) {
      for (int Chan=0; Chan < (Spec.ColorEnc == COLOR_MONO ? 1 : 3); Chan++) {
        _Pixel px;
        px.X = x;
        px.Y = y;
        px.Channel = Chan;
        switch(Spec.SubSamp) {

          case (SUBSAMP_NONE):
            px.Time = y*(Spec.tLine) + (x+0.5) * Spec.tPixel;

            switch (Spec.SyncOrder) {

              case (SYNC_STRAIGHT):
                px.Time += Spec.tSync + Spec.tPorch + Chan*(Spec.tPixel * Spec.ImgWidth + Spec.tSep);
                break;

              case (SYNC_SCOTTIE):
                px.Time += Spec.tSync + (Chan+1) * Spec.tSep + Chan*(Spec.tPixel * Spec.ImgWidth) +
                  (Chan == 2 ? Spec.tSync : 0);
                break;

            }
            break;

          case (SUBSAMP_2121):
            switch (Chan) {

              case (0):
                px.Time = y*(Spec.tLine) + Spec.tSync + Spec.tPorch + (x+.5) * Spec.tPixel * 2;
                break;

              case (1):
                px.Time = (y-(y % 2)) * (Spec.tLine) + Spec.tSync + Spec.tPorch + Spec.tPixel*2*Spec.ImgWidth + Spec.tSep + (x+0.5) * Spec.tPixel;
                break;

              case (2):
                px.Time = (y+1-(y % 2)) * (Spec.tLine) + Spec.tSync + Spec.tPorch + Spec.tPixel*2*Spec.ImgWidth + Spec.tSep + (x+0.5) * Spec.tPixel;
                break;

            }

          case (SUBSAMP_211):
            switch (Chan) {

              case (0):
                px.Time = y*(Spec.tLine) + Spec.tSync + Spec.tPorch + (x+.5) * Spec.tPixel * 2;
                break;

              case (1):
                px.Time = 999;//TODO
                break;

              case (2):
                px.Time = 999;
                break;

            }

          case (SUBSAMP_2112):
            switch (Chan) {

              case (0):
                px.Time = (y/2)*(Spec.tLine) + Spec.tSync + Spec.tPorch +
                  ((y%2 == 1 ? 640*3 : 0)+x+.5) * Spec.tPixel;
                break;

              case (1):
                px.Time = (y/2)*(Spec.tLine) + Spec.tSync + Spec.tPorch +
                  (640+x+.5)*Spec.tPixel;
                break;

              case (2):
                px.Time = (y/2)*(Spec.tLine) + Spec.tSync + Spec.tPorch +
                  (640*2+x+.5)*Spec.tPixel;
                break;

            }
        }
        PixelGrid.push_back(px);
      }
    }
  }

  std::sort(PixelGrid.begin(), PixelGrid.end(), sortPixelTime);

   Glib::RefPtr<Gtk::Application> app =
     Gtk::Application::create("com.windytan.slowrx");
  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, Spec.ImgWidth, Spec.NumLines);
  pixbuf_rx->fill(0x000000ff);

  // Initialize pixbuffer
  /*if (!Redraw) {
    g_object_unref(pixbuf_rx);
    pixbuf_rx = gdk_pixbuf_new (GDK_COLORSPACE_RGB, false, 8, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].NumLines);
    gdk_pixbuf_fill(pixbuf_rx, 0);
  }*/

  /*int     rowstride = gdk_pixbuf_get_rowstride (pixbuf_rx);
  guchar *pixels, *p;
  pixels = gdk_pixbuf_get_pixels(pixbuf_rx);

  g_object_unref(pixbuf_disp);
  pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
      500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].NumLines * ModeSpec[Mode].LineHeight, GDK_INTERP_BILINEAR);
*/
  //gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);

  /*int Length        = Spec.tLine * Spec.NumLines * 44100;
  SyncTargetBin = GetBin(1200+CurrentPic.HedrShift, FFTLen);
  Abort         = false;
  SyncSampleNum = 0;*/

  // Loop through signal
  double t = 0;
  for (int PixelIdx = 0; PixelIdx < PixelGrid.size(); PixelIdx++) {

    double Lum = 0;

    if (!Redraw) {

      while (t < PixelGrid[PixelIdx].Time && dsp->is_still_listening()) {
        t += dsp->forward();
      }

      /*** Store the sync band for later adjustments ***/

      /*if (SampleNum == NextSyncTime) {
 
        Praw = Psync = 0;

        memset(fft.in, 0, sizeof(double)*FFTLen);
       
        // Hann window
        for (i = 0; i < 64; i++) fft.in[i] = pcm.Buffer[pcm.WindowPtr+i-32] / 32768.0 * Hann[1][i];

        fftw_execute(fft.Plan1024);

        for (i=GetBin(1500+CurrentPic.HedrShift,FFTLen); i<=GetBin(2300+CurrentPic.HedrShift, FFTLen); i++)
          Praw += power(fft.out[i]);

        for (i=SyncTargetBin-1; i<=SyncTargetBin+1; i++)
          Psync += power(fft.out[i]) * (1- .5*abs(SyncTargetBin-i));

        Praw  /= (GetBin(2300+CurrentPic.HedrShift, FFTLen) - GetBin(1500+CurrentPic.HedrShift, FFTLen));
        Psync /= 2.0;

        // If there is more than twice the amount of power per Hz in the
        // sync band than in the video band, we have a sync signal here
        HasSync[SyncSampleNum] = (Psync > 2*Praw);

        NextSyncTime += 13;
        SyncSampleNum ++;

      }*/

      /*** Estimate SNR ***/

      /*if (SampleNum == NextSNRtime) {
        
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
      }*/



      /*** FM demodulation ***/

      //PrevFreq = Freq;

      // Adapt window size to SNR
      WindowType WinType;
      double SNR = 3;
      bool Adaptive = true;

      if      (!Adaptive)  WinType = WINDOW_HANN47;
      
      else if (SNR >=  20) WinType = WINDOW_HANN47;
      else if (SNR >=  10) WinType = WINDOW_HANN63;
      else if (SNR >=   9) WinType = WINDOW_HANN95;
      else if (SNR >=   3) WinType = WINDOW_HANN127;
      else if (SNR >=  -5) WinType = WINDOW_HANN255;
      else if (SNR >= -10) WinType = WINDOW_HANN511;
      else                 WinType = WINDOW_HANN1023;

      // Minimum winlength can be doubled for Scottie DX
      //if (Mode == MODE_SDX && WinType < WINDOW_HANN511) WinType++;

      double Freq = dsp->get_peak_freq(1500, 2300, WinType);

      // Linear interpolation of (chronologically) intermediate frequencies, for redrawing
      //InterpFreq = PrevFreq + (Freq-PrevFreq) * ...  // TODO!

      // Calculate luminency & store for later use
      Lum = clip((Freq - (1500)) / 3.1372549);
      //StoredLum[SampleNum] = clip((Freq - (1500 + CurrentPic.HedrShift)) / 3.1372549);

    } /* endif (!Redraw) */

    int x = PixelGrid[PixelIdx].X;
    int y = PixelGrid[PixelIdx].Y;
    int Channel = PixelGrid[PixelIdx].Channel;
    
    // Store pixel
    Image[x][y][Channel] = Lum;//StoredLum[SampleNum];

    // Some modes have R-Y & B-Y channels that are twice the height of the Y channel
    //if (Channel > 0 && (Mode == R36 || Mode == R24))
    //  Image[x][y+1][Channel] = StoredLum[SampleNum];

  }

  // Calculate and draw pixels to pixbuf on line change
  guint8 *p;
  guint8 *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();
  //int tx = x;
  //if (x == ModeSpec[Mode].ImgWidth-1 || PixelIdx < PixelGrid.size()-1) {
  for (int tx = 0; tx < ModeSpec[Mode].ImgWidth; tx++) {
    for (int y = 0; y < Spec.NumLines; y++) {
      p = pixels + y * rowstride + tx * 3;

      switch(Spec.ColorEnc) {

        case COLOR_RGB:
          p[0] = Image[tx][y][0];
          p[1] = Image[tx][y][1];
          p[2] = Image[tx][y][2];
          break;

        case COLOR_GBR:
          p[0] = Image[tx][y][2];
          p[1] = Image[tx][y][0];
          p[2] = Image[tx][y][1];
          break;

        case COLOR_YUV:
          p[0] = clip((100 * Image[tx][y][0] + 140 * Image[tx][y][1] - 17850) / 100.0);
          p[1] = clip((100 * Image[tx][y][0] -  71 * Image[tx][y][1] - 33 *
              Image[tx][y][2] + 13260) / 100.0);
          p[2] = clip((100 * Image[tx][y][0] + 178 * Image[tx][y][2] - 22695) / 100.0);
          break;

        case COLOR_MONO:
          p[0] = p[1] = p[2] = Image[tx][y][0];
          break;

      }
    }

    /*if (!Redraw || y % 5 == 0 || PixelIdx == PixelGrid.size()-1) {
      // Scale and update image
      g_object_unref(pixbuf_disp);
      pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
          500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].NumLines * ModeSpec[Mode].LineHeight, GDK_INTERP_BILINEAR);

      gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);
    }*/
  }

  
  /*if (!Redraw && SampleNum % 8820 == 0) {
    setVU(Power, FFTLen, WinIdx, true);
  }*/

  pixbuf_rx->save("testi.png", "png");

  return true;

}
