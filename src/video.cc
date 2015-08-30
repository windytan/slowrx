#include "common.hh"


void renderPixbuf(Picture *pic) {

  int upsample_factor = 4;

  std::vector<PixelSample> pixel_grid = getPixelSamplingPoints(pic->mode);

  guint8 lum[800][800][3];

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, ModeSpec[pic->mode].ScanPixels, ModeSpec[pic->mode].NumLines);
  pixbuf_rx->fill(0x000000ff);

  guint8 *p;
  guint8 *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();
  Wave signal_up = upsampleLanczos(pic->video_signal, upsample_factor, 3);

  for (size_t pixel_idx = 0; pixel_idx <= pixel_grid.size(); pixel_idx ++) {

    PixelSample px = pixel_grid[pixel_idx];

    double signal_t = (px.Time/pic->speed + pic->starts_at) / pic->video_dt * upsample_factor;
    double val;
    if (signal_t < 0 || signal_t >= signal_up.size()-1) {
      val = 0;
    } else {
      double d = signal_t - int(signal_t);
      val = (1-d) * signal_up[signal_t] +
                d * signal_up[signal_t+1];
    }
    int x  = pixel_grid[pixel_idx].pt.x;
    int y  = pixel_grid[pixel_idx].pt.y;
    int ch = pixel_grid[pixel_idx].Channel;

    lum[x][y][ch] = clip(val*255);
  }

  if (ModeSpec[pic->mode].SubSampling == SUBSAMP_420_YUYV) {
    for (size_t x=0; x < ModeSpec[pic->mode].ScanPixels; x++) {
      std::vector<double> column_u, column_u_filtered;
      std::vector<double> column_v, column_v_filtered;
      for (size_t y=0; y < ModeSpec[pic->mode].NumLines; y+=2) {
        column_u.push_back(lum[x][y][1]);
        column_v.push_back(lum[x][y][2]);
      }
      column_u_filtered = upsampleLanczos(column_u, 2, 2);
      column_v_filtered = upsampleLanczos(column_v, 2, 2);
      for (size_t y=0; y < ModeSpec[pic->mode].NumLines; y++) {
        lum[x][y][1] = column_u_filtered[y];
        lum[x][y][2] = column_v_filtered[y];
      }
    }
  }

  for (size_t x = 0; x < ModeSpec[pic->mode].ScanPixels; x++) {
    for (size_t y = 0; y < ModeSpec[pic->mode].NumLines; y++) {

      p = pixels + y * rowstride + x * 3;

      switch(ModeSpec[pic->mode].ColorEnc) {

        case COLOR_RGB:
          p[0] = lum[x][y][0];
          p[1] = lum[x][y][1];
          p[2] = lum[x][y][2];
          break;

        case COLOR_GBR:
          p[0] = lum[x][y][2];
          p[1] = lum[x][y][0];
          p[2] = lum[x][y][1];
          break;

        case COLOR_YUV:
          // TODO chroma filtering
          p[0] = clip((100 * lum[x][y][0] + 140 * lum[x][y][1] - 17850) / 100.0);
          p[1] = clip((100 * lum[x][y][0] -  71 * lum[x][y][1] - 33 *
              lum[x][y][2] + 13260) / 100.0);
          p[2] = clip((100 * lum[x][y][0] + 178 * lum[x][y][2] - 22695) / 100.0);
          break;

        case COLOR_MONO:
          p[0] = p[1] = p[2] = lum[x][y][0];
          break;
      }
    }
  }
  pixbuf_rx->save("testi.png", "png");
}

// Time instants for all pixels
std::vector<PixelSample> getPixelSamplingPoints(SSTVMode mode) {
  _ModeSpec s = ModeSpec[mode];
  std::vector<PixelSample> pixel_grid;
  for (size_t y=0; y<s.NumLines; y++) {
    for (size_t x=0; x<s.ScanPixels; x++) {
      for (size_t Chan=0; Chan < (s.ColorEnc == COLOR_MONO ? 1 : 3); Chan++) {
        PixelSample px;
        px.pt = Point(x,y);
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
        pixel_grid.push_back(px);
      }
    }
  }

  std::sort(pixel_grid.begin(), pixel_grid.end(), [](PixelSample a, PixelSample b) {
      return a.Time < b.Time;
  });

  return pixel_grid;
}

/* Demodulate the video signal & store all kinds of stuff for later stages
 *  Mode:      M1, M2, S1, S2, R72, R36...
 *  Rate:      exact sampling rate used
 *  Skip:      number of PCM samples to skip at the beginning (for sync phase adjustment)
 *  Redraw:    false = Apply windowing and FFT to the signal, true = Redraw from cached FFT data
 *  returns:   true when finished, false when aborted
 */
bool rxVideo(SSTVMode mode, DSPworker* dsp) {

  printf("receive %s\n",ModeSpec[mode].Name.c_str());

  _ModeSpec s = ModeSpec[mode];
  Picture pic(mode);

  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("com.windytan.slowrx");

  double next_sync_sample_time = 0;
  double next_video_sample_time = 0;

  double t_total = (s.SyncOrder == SYNC_SCOTTIE ? s.tSync : 0) + s.NumLines * s.tLine;
  size_t idx = 0;

  for (double t=0; t < t_total && dsp->is_open(); t += dsp->forward()) {

    if (t >= next_sync_sample_time) {
      pic.sync_signal.push_back(dsp->hasSync());

      next_sync_sample_time += pic.sync_dt;
    }

    bool is_adaptive = true;

    if ( t >= next_video_sample_time ) {

      pic.video_signal.push_back(dsp->lum(pic.mode, is_adaptive));

      if ((idx+1) % 1000 == 0) {
        size_t prog_width = 50;
        fprintf(stderr,"  [");
        double prog = t / t_total;
        size_t prog_points = prog * prog_width + .5;
        for (size_t i=0;i<prog_points;i++) {
          fprintf(stderr,"=");
        }
        for (size_t i=prog_points;i<prog_width;i++) {
          fprintf(stderr," ");
        }
        fprintf(stderr,"] %.1f %%\r",prog*100);
      }

      if (dsp->isLive() && (idx+1) % 10000 == 0) {
        resync(&pic);
        renderPixbuf(&pic);
      }
      next_video_sample_time += pic.video_dt;
      idx++;
    }
  }

  resync(&pic);
  renderPixbuf(&pic);

    /*if (!Redraw || y % 5 == 0 || PixelIdx == pixel_grid.size()-1) {
      // Scale and update image
      g_object_unref(pixbuf_disp);
      pixbuf_disp = gdk_pixbuf_scale_simple(pixbuf_rx, 500,
          500.0/ModeSpec[mode].ImgWidth * ModeSpec[mode].NumLines * ModeSpec[mode].LineHeight, GDK_INTERP_BILINEAR);

      gtk_image_set_from_pixbuf(GTK_IMAGE(gui.image_rx), pixbuf_disp);
    }*/

  

  fprintf(stderr, "\n");

  return true;

}
