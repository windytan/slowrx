#include "common.h"
#include "gui.h"
#include "dsp.h"
#include "picture.h"

void Picture::pushToSyncSignal(double s) {
  sync_signal_.push_back(s);
}
void Picture::pushToVideoSignal(double s) {
  video_signal_.push_back(s);
}

SSTVMode Picture::getMode() { return mode_; }
double Picture::getDrift () { return drift_; }
double Picture::getStartsAt () { return starts_at_; }
double Picture::getVideoDt () { return video_dt_; }
double Picture::getSyncDt () { return sync_dt_; }
double Picture::getSyncSignalAt (size_t i) { return sync_signal_[i]; }
double Picture::getVideoSignalAt (size_t i) { return video_signal_[i]; }


void Picture::renderPixbuf(unsigned min_width) {

  int upsample_factor = 4;
  _ModeSpec m = ModeSpec[mode_];


  std::vector<PixelSample> pixel_grid = pixelSamplingPoints(mode_);
  
  std::vector<std::vector<std::vector<uint8_t>>> img(m.scan_pixels);
  for (size_t x=0; x < m.scan_pixels; x++) {
    img[x] = std::vector<std::vector<uint8_t>>(m.num_lines);
    for (size_t y=0; y < m.num_lines; y++) {
      img[x][y] = std::vector<uint8_t>(m.color_enc == COLOR_MONO ? 1 : 3);
    }
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, m.scan_pixels, m.num_lines);
  pixbuf_rx->fill(0x000000ff);

  guint8 *p;
  guint8 *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();
  Wave* signal_up = upsample(video_signal_, upsample_factor, KERNEL_TENT);

  for (size_t pixel_idx = 0; pixel_idx < pixel_grid.size(); pixel_idx ++) {

    PixelSample px = pixel_grid[pixel_idx];

    if (px.exists) {

      double signal_t = (px.t/drift_ + starts_at_) / video_dt_ * upsample_factor;
      double val;
      if (signal_t < 0 || signal_t >= signal_up->size()-1) {
        val = 0;
      } else {
        val = signal_up->at(signal_t);
      }
      
      int x  = pixel_grid[pixel_idx].pt.x;
      int y  = pixel_grid[pixel_idx].pt.y;
      int ch = pixel_grid[pixel_idx].ch;

      img[x][y][ch] = clip(round(val*255));
    }
  }

  if (mode_ == MODE_R36 || m.family == MODE_PD) {
    for (size_t x=0; x < m.scan_pixels; x++) {
      Wave column_u, column_v;
      Wave* column_u_filtered;
      Wave* column_v_filtered;
      for (size_t y=0; y < m.num_lines; y+=2) {
        column_u.push_back(img[x][y][1]);
        column_v.push_back(img[x][y][2]);
      }
      column_u_filtered = upsample(column_u, 2, KERNEL_TENT);
      column_v_filtered = upsample(column_v, 2, KERNEL_TENT);
      for (size_t y=0; y < m.num_lines; y++) {
        img[x][y][1] = column_u_filtered->at(y+1);
        img[x][y][2] = column_v_filtered->at(y+1);
      }
    }
  }

  for (size_t x = 0; x < m.scan_pixels; x++) {
    for (size_t y = 0; y < m.num_lines; y++) {

      p = pixels + y * rowstride + x * 3;

#ifdef RGBONLY
      p[0] = lum[x][y][0];
      p[1] = lum[x][y][1];
      p[2] = lum[x][y][2];
#else
      switch(m.color_enc) {

        case COLOR_RGB: {
          p[0] = img[x][y][0];
          p[1] = img[x][y][1];
          p[2] = img[x][y][2];
          break;
        }
        case COLOR_GBR: {
          p[0] = img[x][y][2];
          p[1] = img[x][y][0];
          p[2] = img[x][y][1];
          break;
        }
        case COLOR_YUV: {
          double r = (298.082/256)*img[x][y][0] + (408.583/256) * img[x][y][1] - 222.921;
          double g = (298.082/256)*img[x][y][0] - (100.291/256) * img[x][y][2] - (208.120/256) * img[x][y][1] + 135.576;
          double b = (298.082/256)*img[x][y][0] + (516.412/256) * img[x][y][2] - 276.836;
          p[0] = clip(r);
          p[1] = clip(g);
          p[2] = clip(b);
          break;
        }
        case COLOR_MONO: {
          p[0] = p[1] = p[2] = img[x][y][0];
          break;
        }
      }
#endif
    }
  }

  unsigned img_width = std::max(min_width, m.scan_pixels);
  double scale = 1.0*img_width/m.scan_pixels;
  unsigned img_height = round(m.num_lines * scale);

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled;
  pixbuf_scaled = pixbuf_rx->scale_simple(img_width, img_height, Gdk::INTERP_BILINEAR);

  pixbuf_scaled->save("testi.png", "png");
}


void Picture::saveSync () {
  _ModeSpec m = ModeSpec[mode_];
  int line_width = m.t_period / sync_dt_;
  int numlines = 240;
  int upsample_factor = 2;

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, line_width, numlines);
  pixbuf_rx->fill(0x000000ff);

  guint8 *p;
  guint8 *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();

  Wave* big_sync = upsample(sync_signal_, 2, KERNEL_TENT);

  for (size_t i=1; i<sync_signal_.size(); i++) {
    int x = i % line_width;
    int y = i / line_width;
    int signal_idx = i * upsample_factor / drift_ + .5;
    if (y < numlines) {
      uint8_t val = clip((big_sync->at(signal_idx))*127);
      p = pixels + y * rowstride + x * 3;
      p[0] = p[1] = p[2] = val;
    }
  }

  pixbuf_rx->save("sync.png", "png");
}


void Picture::resync () {

#ifdef NOSYNC
  drift_ = 1.0;
  starts_at_ = 0.0;
#else
  _ModeSpec m = ModeSpec[mode_];
  int line_width = m.t_period / sync_dt_;

  size_t upsample_factor = 2;
  Wave* sync_up = upsample(sync_signal_, upsample_factor, KERNEL_TENT);

  /* slant */
  std::vector<double> histogram;
  int peak_drift_at = 0;
  double peak_drift_val = 0;
  double min_drift  = 0.998;
  double max_drift  = 1.002;
  double drift_step = 0.00005;
  
  for (double d = min_drift; d <= max_drift; d += drift_step) {
    std::vector<double> acc(line_width);
    int peak_x = 0;
    for (size_t i=1; i<sync_signal_.size(); i++) {
      int x = i % line_width;
      size_t signal_idx = round(i * upsample_factor / d);
      double delta = (signal_idx < sync_up->size() ? sync_up->at(signal_idx) - sync_up->at(signal_idx-1) : 0);
      if (delta >= 0.0)
        acc[x] += delta;
      if (acc[x] > acc[peak_x]) {
        peak_x = x;
      }
    }
    histogram.push_back(acc[peak_x]);
    if (acc[peak_x] > peak_drift_val) {
      peak_drift_at = histogram.size()-1;
      peak_drift_val = acc[peak_x];
    }
  }

  double peak_refined = peak_drift_at +
    gaussianPeak(histogram[peak_drift_at-1], histogram[peak_drift_at], histogram[peak_drift_at+1]);
  double drift = min_drift + peak_refined*drift_step;

  /* align */
  std::vector<double> acc(line_width);
  for (size_t i=1;i<sync_signal_.size()-1; i++) {
    int x = i % line_width;
    size_t signal_idx = round(i * upsample_factor / drift);
    acc[x] += (signal_idx < sync_up->size() ? sync_up->at(signal_idx) : 0);
  }

  int kernel_len = round(m.t_sync / m.t_period * line_width);
  kernel_len += 1-(kernel_len%2);
  Wave sync_kernel(kernel_len, 1);
  Wave sc = convolve(acc, sync_kernel, true);
  size_t m_i = maxIndex(sc);
  double peak_align = m_i + gaussianPeak(sc[m_i-1], sc[m_i], sc[m_i+1]);

  printf("peak_align = %f\n",peak_align);

  double align_time = peak_align/line_width*m.t_period - m.t_sync*0.5;
  if (m.family == MODE_SCOTTIE)
    align_time -= m.t_sync + m.t_sep*2 + m.t_scan * 2;

  fprintf(stderr,"drift = %.5f\n",drift);

  delete sync_up;

  drift_ = drift;
  starts_at_ = align_time;

#endif

  //saveSync(pic);
}
