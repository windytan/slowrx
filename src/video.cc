#include "common.hh"

typedef struct {
  int X;
  int Y;
  int Channel;
  double Time;
} PixelSample;

bool sortPixelSamples(PixelSample a, PixelSample b) { return a.Time < b.Time; }

// Map to RGB & store in pixbuf
void toPixbufRGB(guint8 Image[800][800][3], Glib::RefPtr<Gdk::Pixbuf> pixbuf, SSTVMode Mode) {
  guint8 *p;
  guint8 *pixels;
  pixels = pixbuf->get_pixels();
  int rowstride = pixbuf->get_rowstride();
  for (int x = 0; x < ModeSpec[Mode].ScanPixels; x++) {
    for (int y = 0; y < ModeSpec[Mode].NumLines; y++) {
      p = pixels + y * rowstride + x * 3;

      switch(ModeSpec[Mode].ColorEnc) {

        case COLOR_RGB:
          p[0] = Image[x][y][0];
          p[1] = Image[x][y][1];
          p[2] = Image[x][y][2];
          break;

        case COLOR_GBR:
          p[0] = Image[x][y][2];
          p[1] = Image[x][y][0];
          p[2] = Image[x][y][1];
          break;

        case COLOR_YUV:
          // TODO chroma filtering
          p[0] = clip((100 * Image[x][y][0] + 140 * Image[x][y][1] - 17850) / 100.0);
          p[1] = clip((100 * Image[x][y][0] -  71 * Image[x][y][1] - 33 *
              Image[x][y][2] + 13260) / 100.0);
          p[2] = clip((100 * Image[x][y][0] + 178 * Image[x][y][2] - 22695) / 100.0);
          break;

        case COLOR_MONO:
          p[0] = p[1] = p[2] = Image[x][y][0];
          break;

      }
    }
  }
}

// Time instants for all pixels
std::vector<PixelSample> getPixelSamplingPoints(SSTVMode Mode) {
  _ModeSpec s = ModeSpec[Mode];
  std::vector<PixelSample> PixelGrid;
  for (int y=0; y<s.NumLines; y++) {
    for (int x=0; x<s.ScanPixels; x++) {
      for (int Chan=0; Chan < (s.ColorEnc == COLOR_MONO ? 1 : 3); Chan++) {
        PixelSample px;
        px.X = x;
        px.Y = y;
        px.Channel = Chan;
        switch(s.SubSampling) {

          case (SUBSAMP_444):
            px.Time = y*(s.tLine) + (x+0.5)/s.ScanPixels * s.tScan;

            switch (s.SyncOrder) {

              case (SYNC_SIMPLE):
                px.Time += s.tSync + s.tPorch + Chan*(s.tScan + s.tSep);
                break;

              case (SYNC_SCOTTIE):
                px.Time += s.tSync + (Chan+1) * s.tSep + Chan*s.tScan +
                  (Chan == 2 ? s.tSync : 0);
                break;

            }
            break;

          case (SUBSAMP_420_YUYV):
            switch (Chan) {

              case (0):
                px.Time = y*(s.tLine) + s.tSync + s.tPorch +
                  (x+.5)/s.ScanPixels * s.tScan;
                break;

              case (1):
                px.Time = (y-(y % 2)) * (s.tLine) + s.tSync + s.tPorch +
                  s.tScan + s.tSep + 0.5*(x+0.5)/s.ScanPixels * s.tScan;
                break;

              case (2):
                px.Time = (y+1-(y % 2)) * (s.tLine) + s.tSync + s.tPorch +
                  s.tScan + s.tSep + 0.5*(x+0.5)/s.ScanPixels * s.tScan;
                break;

            }
            break;

          case (SUBSAMP_422_YUV):
            switch (Chan) {

              case (0):
                px.Time = y*(s.tLine) + s.tSync + s.tPorch +
                  (x+.5)/s.ScanPixels * s.tScan;
                break;

              case (1):
                px.Time = y*(s.tLine) + s.tSync + s.tPorch + s.tScan
                 + s.tSep + (x+.5)/s.ScanPixels * s.tScan/2;
                break;

              case (2):
                px.Time = y*(s.tLine) + s.tSync + s.tPorch + s.tScan
                  + s.tSep + s.tScan/2 + s.tSep + (x+.5)/s.ScanPixels *
                  s.tScan/2;
                break;

            }
            break;

          case (SUBSAMP_440_YUVY):
            switch (Chan) {

              case (0):
                px.Time = (y/2)*(s.tLine) + s.tSync + s.tPorch +
                  ((y%2 == 1 ? s.ScanPixels*3 : 0)+x+.5)/s.ScanPixels *
                  s.tScan;
                break;

              case (1):
                px.Time = (y/2)*(s.tLine) + s.tSync + s.tPorch +
                  (s.ScanPixels+x+.5)/s.ScanPixels * s.tScan;
                break;

              case (2):
                px.Time = (y/2)*(s.tLine) + s.tSync + s.tPorch +
                  (s.ScanPixels*2+x+.5)/s.ScanPixels * s.tScan;
                break;

            }
            break;
        }
        PixelGrid.push_back(px);
      }
    }
  }

  std::sort(PixelGrid.begin(), PixelGrid.end(), sortPixelSamples);

  return PixelGrid;
}

/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    false = Apply windowing and FFT to the signal, true = Redraw from cached FFT data
 *  returns:   true when finished, false when aborted
 */
bool GetVideo(SSTVMode Mode, DSPworker* dsp) {

  printf("receive %s\n",ModeSpec[Mode].Name.c_str());

  _ModeSpec s = ModeSpec[Mode];

  guint8 Image[800][800][3];
  guint8 Imagesnr[800][800][3];

  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("com.windytan.slowrx");
  
  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, s.ScanPixels, s.NumLines);
  pixbuf_rx->fill(0x000000ff);
  
  Glib::RefPtr<Gdk::Pixbuf> pixbuf_snr;
  pixbuf_snr = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, s.ScanPixels, s.NumLines);
  pixbuf_snr->fill(0x000000ff);

  double next_sync_sample_time = 0;
  std::vector<bool> has_sync;

  /*g_object_unref(pixbuf_disp);
  pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
      500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].NumLines * ModeSpec[Mode].LineHeight, GDK_INTERP_BILINEAR);
*/
  //gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);

  /*SyncTargetBin = GetBin(1200+CurrentPic.HedrShift, FFTLen);
  Abort         = false;
  SyncSampleNum = 0;*/

  // Loop through signal
  std::vector<PixelSample> PixelGrid = getPixelSamplingPoints(Mode);
  double t = 0;
  for (int PixelIdx = 0; PixelIdx < PixelGrid.size(); PixelIdx++) {

    /*printf("expecting %d (%d,%d,%d)\n",PixelIdx,
        PixelGrid[PixelIdx].X,
        PixelGrid[PixelIdx].Y,
        PixelGrid[PixelIdx].Channel
        );*/

    while (t < PixelGrid[PixelIdx].Time && dsp->is_open()) {
      t += dsp->forward();
    }

    /*if (dsp->is_open()) {
      printf("got it\n");
    } else {
      printf("didn't get it\n");
    }*/

    /*** Store the sync band for later adjustments ***/

    if (dsp->get_t() >= next_sync_sample_time) {

      int line_width = ModeSpec[Mode].NumLines;

      std::vector<double> bands = dsp->bandPowerPerHz({{1150,1250}, {1500,2300}});

      // If there is more than twice the amount of power per Hz in the
      // sync band than in the video band, we have a sync signal here
      has_sync.push_back(bands[0] > bands[1]);


      next_sync_sample_time += ModeSpec[Mode].tLine / line_width;

    }

    /*** Estimate SNR ***/

    double SNR;
    bool Adaptive = true;

    if (PixelIdx == 0 || (Adaptive && PixelGrid[PixelIdx].X == s.ScanPixels/2)) {
      std::vector<double> bands = dsp->bandPowerPerHz({{300,1100}, {1500,2300}, {2500, 2700}});
      double Pvideo_plus_noise = bands[1];
      double Pnoise_only       = (bands[0] + bands[2]) / 2;
      double Psignal = Pvideo_plus_noise - Pnoise_only;

      SNR = ((Pnoise_only == 0 || Psignal / Pnoise_only < .01) ? -20 : 10 * log10(Psignal / Pnoise_only));

    }


    /*** FM demodulation ***/

    //PrevFreq = Freq;

    // Adapt window size to SNR
    WindowType WinType;

    if   (Adaptive) WinType = dsp->bestWindowFor(Mode, SNR);
    else            WinType = dsp->bestWindowFor(Mode);

    double Freq = dsp->peakFreq(1500, 2300, WinType);

    // Linear interpolation of (chronologically) intermediate frequencies, for redrawing
    //InterpFreq = PrevFreq + (Freq-PrevFreq) * ...  // TODO!

    // Calculate luminency & store for later use
    guint8 Lum = freq2lum(Freq);
    //measured.push_back({t, Lum});
    //StoredLum[SampleNum] = clip((Freq - (1500 + CurrentPic.HedrShift)) / 3.1372549);

    int x = PixelGrid[PixelIdx].X;
    int y = PixelGrid[PixelIdx].Y;
    int Channel = PixelGrid[PixelIdx].Channel;
    
    // Store pixel
    Image[x][y][Channel] = Lum;//StoredLum[SampleNum];
    Imagesnr[x][y][Channel] = WinType;//StoredLum[SampleNum];

  }

  /* sync */
  findSyncRansac(Mode, has_sync);
  
  toPixbufRGB(Image, pixbuf_rx, Mode);
  toPixbufRGB(Imagesnr, pixbuf_snr, Mode);
    /*if (!Redraw || y % 5 == 0 || PixelIdx == PixelGrid.size()-1) {
      // Scale and update image
      g_object_unref(pixbuf_disp);
      pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
          500.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].NumLines * ModeSpec[Mode].LineHeight, GDK_INTERP_BILINEAR);

      gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);
    }*/

  
  pixbuf_rx->save("testi.png", "png");
  pixbuf_snr->save("snr.png", "png");

  return true;

}
