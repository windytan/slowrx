#include "common.h"
#include "dsp.h"
#include "picture.h"
#include "gui.h"

void Picture::pushToSyncSignal(double s) {
  sync_signal_.push_back(s);
}
void Picture::pushToVideoSignal(double s) {
  video_signal_.push_back(s);
}

SSTVMode Picture::getMode          () const   { return mode_; }
double   Picture::getDrift         () const   { return drift_; }
double   Picture::getStartsAt      () const   { return starts_at_; }
double   Picture::getVideoDt       () const   { return video_dt_; }
double   Picture::getSyncDt        () const   { return sync_dt_; }
double   Picture::getSyncSignalAt  (size_t i) const { return sync_signal_[i]; }
double   Picture::getVideoSignalAt (size_t i) const { return video_signal_[i]; }
std::string Picture::getTimestamp() const { return std::string(timestamp_); }


Glib::RefPtr<Gdk::Pixbuf> Picture::renderPixbuf(unsigned min_width, int upsample_factor) {

  ModeSpec m = getModeSpec(mode_);

  std::vector<std::vector<std::vector<uint8_t>>> img(m.scan_pixels);
  for (size_t x=0; x < m.scan_pixels; x++) {
    img[x] = std::vector<std::vector<uint8_t>>(m.num_lines);
    for (size_t y=0; y < m.num_lines; y++) {
      img[x][y] = std::vector<uint8_t>(m.color_enc == COLOR_MONO ? 1 : 3);
    }
  }

  Wave signal_up =
    (upsample_factor > 1 ? upsample(video_signal_, upsample_factor, KERNEL_TENT) : video_signal_);

  for (PixelSample px : pixel_grid_) {

    double signal_t = (px.t/drift_ + starts_at_) / video_dt_ * upsample_factor;
    double val;
    if (signal_t < 0 || signal_t >= signal_up.size()-1) {
      val = 0;
    } else {
      val = signal_up[signal_t];
    }

    int x  = px.pt.x;
    int y  = px.pt.y;
    int ch = px.ch;

    img[x][y][ch] = clip(round(val*255));
  }

  /* chroma reconstruction from 4:2:0 */
  if (mode_ == MODE_R36 || m.family == MODE_PD) {
    for (size_t x=0; x < m.scan_pixels; x++) {
      Wave column_u, column_v;
      Wave column_u_filtered;
      Wave column_v_filtered;
      for (size_t y=0; y < m.num_lines; y+=2) {
        column_u.push_back(img[x][y][1]);
        column_v.push_back(img[x][y][2]);
      }
      column_u_filtered = upsample(column_u, 2, KERNEL_TENT);
      column_v_filtered = upsample(column_v, 2, KERNEL_TENT);
      for (size_t y=0; y < m.num_lines; y++) {
        img[x][y][1] = column_u_filtered[y+1];
        img[x][y][2] = column_v_filtered[y+1];
      }
    }
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, m.scan_pixels, m.num_lines);
  pixbuf_rx->fill(0x000000ff);

  guint8 *p;
  guint8 *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();

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
          p[0] = p[1] = p[2] = clip((img[x][y][0] - 15.5) * 1.172); // mmsstv test images
          break;
        }
      }
#endif
    }
  }

  double rx_aspect     = 4.0/3.0;
  //double pad_to_aspect = 5.0/4.0;

  unsigned img_width = round((m.num_lines - m.header_lines) * rx_aspect);
  if (img_width < min_width)
    img_width = min_width;
  unsigned img_height = round(1.0*img_width/rx_aspect + m.header_lines);

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled;
  pixbuf_scaled = pixbuf_rx->scale_simple(img_width, img_height, Gdk::INTERP_HYPER);

  //pixbuf_rx->save("testi.png", "png");

  return pixbuf_scaled;
}


void Picture::saveSync () {
  ModeSpec m = getModeSpec(mode_);
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

  Wave big_sync = upsample(sync_signal_, 2, KERNEL_TENT);

  for (size_t i=1; i<sync_signal_.size(); i++) {
    int x = i % line_width;
    int y = i / line_width;
    int signal_idx = i * upsample_factor / drift_ + .5;
    if (y < numlines) {
      uint8_t val = clip((big_sync[signal_idx])*127);
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
  return;
#endif

  ModeSpec m = getModeSpec(mode_);
  int line_width = m.t_period / sync_dt_;

  size_t upsample_factor = 2;
  Wave sync_up = upsample(Wave(sync_signal_), upsample_factor, KERNEL_TENT);

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
      double delta = (signal_idx < sync_up.size() ? sync_up[signal_idx] - sync_up[signal_idx-1] : 0);
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
    acc[x] += (signal_idx < sync_up.size() ? sync_up[signal_idx] : 0);
  }

  int kernel_len = round(m.t_sync / m.t_period * line_width);
  kernel_len += 1-(kernel_len%2);
  Wave sync_kernel(kernel_len, 1);
  Wave sc = convolve(acc, sync_kernel, true);
  size_t m_i = maxIndex(sc);
  double peak_align = m_i + gaussianPeak(sc[m_i-1], sc[m_i], sc[m_i+1]);

  //printf("align = %f\n",peak_align);

  double align_time = peak_align/line_width*m.t_period - m.t_sync*0.5;
  if (m.family == MODE_SCOTTIE)
    align_time -= m.t_sync + m.t_sep*2 + m.t_scan * 2;

  //fprintf(stderr,"drift = %.5f\n",drift);

  drift_ = drift;
  starts_at_ = align_time;

  //saveSync(pic);
}

// Time instants for all pixels
std::vector<PixelSample> pixelSamplingPoints(SSTVMode mode) {
  ModeSpec m = getModeSpec(mode);
  std::vector<PixelSample> pixel_grid;
  for (size_t y=0; y<m.num_lines; y++) {
    for (size_t x=0; x<m.scan_pixels; x++) {
      for (size_t ch=0; ch < (m.color_enc == COLOR_MONO ? 1 : 3); ch++) {
        PixelSample px;
        px.pt = Point(x,y);
        px.ch = ch;
        bool exists = true;

        if (m.family == MODE_MARTIN  || m.family == MODE_PASOKON ||
            m.family == MODE_WRAASE2 || m.family == MODE_ROBOTBW ) {
          px.t = y*(m.t_period) + m.t_sync + m.t_porch + ch*(m.t_scan + m.t_sep) +
            (x+0.5)/m.scan_pixels * m.t_scan;
        }

        else if (m.family == MODE_SCOTTIE) {
          px.t = y*(m.t_period) + (x+0.5)/m.scan_pixels * m.t_scan +
            m.t_sync + (ch+1) * m.t_sep + ch*m.t_scan +
            (ch == 2 ? m.t_sync : 0);
        }

        else if (m.family == MODE_PD) {
          double line_video_start = (y/2)*(m.t_period) + m.t_sync + m.t_porch;
          if (ch == 0) {
            px.t = line_video_start + (y%2 == 1 ? 3*m.t_scan : 0) +
              (x+.5)/m.scan_pixels * m.t_scan;
          } else if (ch == 1 && (y%2) == 0) {
            px.t = line_video_start + m.t_scan +
              (x+.5)/m.scan_pixels * m.t_scan;
          } else if (ch == 2 && (y%2) == 0) {
            px.t = line_video_start + 2*m.t_scan +
              (x+.5)/m.scan_pixels * m.t_scan;
          } else {
            exists = false;
          }
        }

        else if (mode == MODE_R72 || mode == MODE_R24) {
          double line_video_start = y*(m.t_period) + m.t_sync + m.t_porch;
          if (ch == 0) {
            px.t = line_video_start + (x+.5) / m.scan_pixels * m.t_scan;
          } else if (ch == 1) {
            px.t = line_video_start + m.t_scan + m.t_sep +
             (x+.5) / m.scan_pixels * m.t_scan / 2;
          } else if (ch == 2) {
            px.t = line_video_start + 1.5*m.t_scan + 2*m.t_sep +
             (x+.5) / m.scan_pixels * m.t_scan / 2;
          }
        }

        else if (mode == MODE_R36) {
          double line_video_start = y*(m.t_period) + m.t_sync + m.t_porch;
          if (ch == 0) {
            px.t = line_video_start + (x+.5) / m.scan_pixels * m.t_scan;
          } else if (ch == 1 && (y % 2) == 0) {
            px.t = line_video_start + m.t_scan + m.t_sep +
              (x+.5) / m.scan_pixels * m.t_scan / 2;
          } else if (ch == 2 && (y % 2) == 0) {
            px.t = (y+1)*(m.t_period) + m.t_sync + m.t_porch + m.t_scan + m.t_sep +
              (x+.5) / m.scan_pixels * m.t_scan / 2;
          } else {
            exists = false;
          }
        }

        if (exists)
          pixel_grid.push_back(px);
      }
    }
  }

  std::sort(pixel_grid.begin(), pixel_grid.end(), [](PixelSample a, PixelSample b) {
      return a.t < b.t;
  });

  return pixel_grid;
}

void Picture::save() {
  std::string filename = std::string("slowrx_") + safe_timestamp_ + ".png";
  std::cout << filename << "\n";
}
