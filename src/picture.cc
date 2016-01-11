#include "common.h"
#include "dsp.h"
#include "picture.h"
#include "gui.h"

void Picture::pushToSyncSignal(double s) {
  m_sync_signal.push_back(s);
}
void Picture::pushToVideoSignal(double s) {
  m_video_signal.push_back(s);
}

SSTVMode Picture::getMode () const {
  return m_mode;
}
double Picture::getDrift () const {
  return m_drift;
}
double Picture::getStartsAt () const {
  return m_starts_at;
}
double Picture::getVideoDt () const {
  return m_video_dt;
}
double Picture::getSyncDt () const {
  return m_sync_dt;
}
double Picture::getSyncSignalAt (int i) const {
  return m_sync_signal.at(i);
}
double Picture::getVideoSignalAt (int i) const {
  return m_video_signal.at(i);
}
std::string Picture::getTimestamp() const {
  return std::string(m_timestamp);
}
void Picture::setProgress(double p) {
  m_progress = p;
}

void setChannel (uint32_t& px, int ch, uint8_t val) {
  assert (ch >= 0 && ch <= 3);
  px &= ~(0xff << (8*ch));
  px |= val << (8*ch);
}

uint8_t getChannel (const uint32_t& px, uint8_t ch) {
  assert (ch <= 3);
  return (px >> (8*ch)) & 0xff;
}

void decodeRGB (uint32_t src, uint8_t *dst) {
  dst[0] = getChannel(src, 0);
  dst[1] = getChannel(src, 1);
  dst[2] = getChannel(src, 2);
}

void decodeGBR (uint32_t src, uint8_t *dst) {
  dst[0] = getChannel(src, 2);
  dst[1] = getChannel(src, 0);
  dst[2] = getChannel(src, 1);
}

void decodeYCbCr (uint32_t src, uint8_t *dst) {
  uint8_t Y  = getChannel(src, 0);
  uint8_t Cr = getChannel(src, 1);
  uint8_t Cb = getChannel(src, 2);

  double r = (298.082*Y/256)                    + (408.583*Cr/256) - 222.921;
  double g = (298.082*Y/256) - (100.291*Cb/256) - (208.120*Cr/256) + 135.576;
  double b = (298.082*Y/256) + (516.412*Cb/256)                    - 276.836;

  dst[0] = clip(r);
  dst[1] = clip(g);
  dst[2] = clip(b);
}

void decodeMono (uint32_t src, uint8_t *dst) {
  dst[0] = dst[1] = dst[2] = clip((getChannel(src,0) - 15.5) * 1.172); // mmsstv test images
}


Glib::RefPtr<Gdk::Pixbuf> Picture::renderPixbuf(int width) {

  ModeSpec m = getModeSpec(m_mode);

  std::vector<std::vector<uint32_t>> img(m.scan_pixels);
  for (int x=0; x < m.scan_pixels; x++) {
    img.at(x) = std::vector<uint32_t>(m.num_lines);
  }

  Wave signal_up =
    (upsample_factor > 1 ? upsample(m_video_signal, upsample_factor, KERNEL_TENT) : m_video_signal);

  for (PixelSample px : m_pixel_grid) {

    double signal_t = (px.t/m_drift + m_starts_at) / m_video_dt * upsample_factor;
    double val;
    if (signal_t < 0 || signal_t >= signal_up.size()-1) {
      val = 0;
    } else {
      val = signal_up[signal_t];
    }

    int x  = px.pt.x;
    int y  = px.pt.y;
    int ch = px.ch;

    setChannel(img[x][y], ch, clip(round(val*255)));
  }

  /* chroma reconstruction from 4:2:0 */
  if (m_mode == MODE_R36 || m.family == MODE_PD) {
    for (int x=0; x < m.scan_pixels; x++) {
      Wave column_u, column_v;
      Wave column_u_filtered;
      Wave column_v_filtered;
      for (int y=0; y < m.num_lines; y+=2) {
        column_u.push_back(getChannel(img[x][y], 1));
        column_v.push_back(getChannel(img[x][y], 2));
      }
      column_u_filtered = upsample(column_u, 2, KERNEL_TENT);
      column_v_filtered = upsample(column_v, 2, KERNEL_TENT);
      for (size_t y=0; y < m.num_lines; y++) {
        setChannel(img[x][y], 1, clip(column_u_filtered[y+1]));
        setChannel(img[x][y], 2, clip(column_v_filtered[y+1]));
      }
    }
  }

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, m.scan_pixels, m.num_lines);
  pixbuf_rx->fill(0x000000ff);

  uint8_t *p;
  uint8_t *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();

  for (int y = 0; y < m.num_lines; y++) {
    for (int x = 0; x < m.scan_pixels; x++) {

      p = pixels + y * rowstride + x * 3;

#ifdef RGBONLY
      decodeRGB(img[x][y], p);
#else
      if (m.color_enc == COLOR_RGB) {
        decodeRGB(img[x][y], p);
      } else if (m.color_enc == COLOR_GBR) {
        decodeGBR(img[x][y], p);
      } else if (m.color_enc == COLOR_YUV) {
        decodeYCbCr(img[x][y], p);
      } else {
        decodeMono(img[x][y], p);
      }
#endif
    }
  }

  double rx_aspect     = 4.0/3.0;
  //double pad_to_aspect = 5.0/4.0;

  int img_width  = width;//round((m.num_lines - m.header_lines) * rx_aspect);
  int img_height = round(1.0*img_width/rx_aspect + m.header_lines);

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled;
  pixbuf_scaled = pixbuf_rx->scale_simple(img_width, img_height, Gdk::INTERP_HYPER);

  //pixbuf_rx->save("testi.png", "png");

  return pixbuf_scaled;
}


void Picture::saveSync () {
  ModeSpec m = getModeSpec(m_mode);
  int line_width = m.t_period / m_sync_dt;
  int numlines = 240;
  int upsample_factor = 2;

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_rx;
  pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, line_width, numlines);
  pixbuf_rx->fill(0x000000ff);

  uint8_t *p;
  uint8_t *pixels;
  pixels = pixbuf_rx->get_pixels();
  int rowstride = pixbuf_rx->get_rowstride();

  Wave big_sync = upsample(m_sync_signal, 2, KERNEL_TENT);

    for (int i=0; i<(int)m_sync_signal.size(); i++) {
      int x = i % line_width;
      int y = i / line_width;
      int signal_idx = i * upsample_factor / m_drift + .5;
      if (y < numlines && signal_idx < (int)big_sync.size()) {
        uint8_t val = clipToByte((big_sync[signal_idx])*127);
        p = pixels + y * rowstride + x * 3;
        p[0] = p[1] = p[2] = val;
      }
    }
  }

  pixbuf_rx->save("sync.png", "png");
}


void Picture::resync () {

#ifdef NOSYNC
  m_drift = 1.0;
  m_starts_at = 0.0;
  return;
#endif

  ModeSpec m = getModeSpec(m_mode);
  int line_width = m.t_period / m_sync_dt;

  size_t upsample_factor = 2;
  Wave sync_up = upsample(Wave(m_sync_signal), upsample_factor, KERNEL_TENT);

  /* slant */
  std::vector<double> histogram;
  int peak_drift_at = 0;
  double peak_drift_val = 0;
  double min_drift  = 0.998;
  double max_drift  = 1.002;
  double drift_step = 0.00005;

  for (double d = min_drift; d <= max_drift; d += drift_step) {
    std::vector<double> acc(line_width);
    for (int i=0; i<(int)m_sync_signal.size(); i++) {
      int x = i % line_width;
      int signal_idx = round(i * upsample_factor / drift);
      acc.at(x) += (signal_idx < (int)sync_up.size() ? sync_up.at(signal_idx) : 0);

      if (thresh > 0.0)
        acc.at(x) = acc.at(x) >= thresh ? 1.0 : 0.0;
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
  for (size_t i=1;i<m_sync_signal.size()-1; i++) {
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

  m_drift = drift;
  m_starts_at = align_time;

  //saveSync(pic);
}

// Time instants for all pixels
std::vector<PixelSample> pixelSamplingPoints(SSTVMode mode) {
  ModeSpec m = getModeSpec(mode);
  std::vector<PixelSample> pixel_grid;
  for (int y=0; y<m.num_lines; y++) {
    for (int x=0; x<m.scan_pixels; x++) {
      for (int ch=0; ch < (m.color_enc == COLOR_MONO ? 1 : 3); ch++) {
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

void Picture::save(std::string dir) {
  std::string filename = dir + "/" + std::string("slowrx_") + m_safe_timestamp + ".png";
  Glib::RefPtr<Gdk::Pixbuf> pb = renderPixbuf(500);
  std::cout << "saving " << filename << "\n";
  pb->save(filename, "png");
}
