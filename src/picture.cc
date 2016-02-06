#include "picture.h"
#include "dsp.h"
#include "gui.h"

Picture::Picture(ModeSpec mode, int srate) :
  m_mode(mode),
  m_pixelsamples(pixelSamplingPoints(mode)),
  m_has_line(mode.num_lines),
  m_img(mode.scan_pixels),
  m_progress(0),
  m_original_samplerate(srate),
  m_video_signal(),
  m_sync_signal(),
  m_sync_decim_ratio(22),
  m_tx_speed(1.0),
  m_starts_at(0.0) {

  printf("new Picture()\n=============\n");

  std::time_t t = std::time(NULL);
  std::strftime(m_timestamp, sizeof(m_timestamp),"%F %Rz", std::gmtime(&t));
  std::strftime(m_safe_timestamp, sizeof(m_safe_timestamp),"%Y%m%d_%H%M%SZ", std::gmtime(&t));

  std::cout << "│  mode: " << m_mode.name << "\n";
  std::cout << "│  timestamp: " << m_timestamp << " (" << m_safe_timestamp << ")\n";
  printf("│  pixbuf: %d lines, %d pixels each\n",m_mode.num_lines, m_mode.scan_pixels);

  for (int x=0; x < m_mode.scan_pixels; x++) {
    m_img.at(x) = std::vector<uint32_t>(m_mode.num_lines);
  }

  m_pixbuf_rx = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, m_mode.scan_pixels, m_mode.num_lines);
  m_pixbuf_rx->fill(0x000000ff);

  m_video_decim_ratio = mode.t_scan / mode.scan_pixels * m_original_samplerate / 3.0;

  printf("│  video decim ratio: %d\n",m_video_decim_ratio);
  printf("└──\n");

}


void Picture::pushToSyncSignal(double s) {
  std::lock_guard<std::mutex> guard(m_mutex);
  m_sync_signal.push_back(s);
}
void Picture::pushToVideoSignal(double s) {
  std::lock_guard<std::mutex> guard(m_mutex);
  m_video_signal.push_back(s);
}

ModeSpec Picture::getMode () const {
  return m_mode;
}
double Picture::getTxSpeed () const {
  return m_tx_speed;
}
double Picture::getStartsAt () const {
  return m_starts_at;
}
int Picture::getVideoDecimRatio () const {
  return m_video_decim_ratio;
}
int Picture::getSyncDecimRatio () const {
  return m_sync_decim_ratio;
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

void decodeRGB (uint32_t src, uint8_t* dst) {
  dst[0] = getChannel(src, 0);
  dst[1] = getChannel(src, 1);
  dst[2] = getChannel(src, 2);
}

void decodeGBR (uint32_t src, uint8_t* dst) {
  dst[0] = getChannel(src, 2);
  dst[1] = getChannel(src, 0);
  dst[2] = getChannel(src, 1);
}

void decodeYCbCr (uint32_t src, uint8_t* dst) {
  uint8_t Y  = getChannel(src, 0);
  uint8_t Cr = getChannel(src, 1);
  uint8_t Cb = getChannel(src, 2);

  double r = (298.082*Y/256)                    + (408.583*Cr/256) - 222.921;
  double g = (298.082*Y/256) - (100.291*Cb/256) - (208.120*Cr/256) + 135.576;
  double b = (298.082*Y/256) + (516.412*Cb/256)                    - 276.836;

  dst[0] = clipToByte(r);
  dst[1] = clipToByte(g);
  dst[2] = clipToByte(b);
}

void decodeMono (uint32_t src, uint8_t* dst) {
  dst[0] = dst[1] = dst[2] = clipToByte((getChannel(src,0) - 15.5) * 1.172); // mmsstv test images
}

void reconstructChroma420 (std::vector<std::vector<uint32_t>>& img, const ModeSpec& m) {
  std::vector<std::vector<uint32_t>> nimg(img);
  //const Kernel reconst_kernel(KERNEL_TENT);
  for (int x=0; x < m.scan_pixels; x++) {
    for (int y=0; y < m.num_lines; y++) {
      setChannel(img[x][y], 1, getChannel(nimg[x/2][y/2], 1));
      setChannel(img[x][y], 2, getChannel(nimg[x/2][y/2], 2));
    }
  }
}

void reconstructChroma422 (std::vector<std::vector<uint32_t>>& img, const ModeSpec& m) {
  std::vector<std::vector<uint32_t>> nimg(img);
  const Kernel reconst_kernel(KERNEL_TENT);

  for (int y=0; y < m.num_lines; y++) {
    Wave row_u, row_v;
    for (int x=0; x < m.scan_pixels/2; x++) {
      row_u.push_back(getChannel(img[x][y], 1));
      row_v.push_back(getChannel(img[x][y], 2));
    }

    for (int x=0; x < m.scan_pixels; x++) {
      double u = convolveSingle(row_u, reconst_kernel, 0.5*x, BORDER_ZERO);
      double v = convolveSingle(row_v, reconst_kernel, 0.5*x, BORDER_ZERO);
      printf("%3.0f->%3.0f ",row_u[x/2],u);
      setChannel(img[x][y], 1, u);//getChannel(nimg[x/2][y], 1));
      setChannel(img[x][y], 2, v);//getChannel(nimg[x/2][y], 2));
    }
  }
}



Glib::RefPtr<Gdk::Pixbuf> Picture::renderPixbuf(int width) {

  assert(width > 0);

  std::vector<std::vector<uint32_t>> img(m_mode.scan_pixels);
  for (int x=0; x < m_mode.scan_pixels; x++) {
    img.at(x) = std::vector<uint32_t>(m_mode.num_lines);
  }

  const Kernel sample_kern(KERNEL_LANCZOS3);

  {
    std::lock_guard<std::mutex> guard(m_mutex);
    double dt = 1.0 * m_video_decim_ratio / m_original_samplerate;
    for (PixelSample px : m_pixel_grid) {

      double signal_x = (m_starts_at + px.t/m_tx_speed) / dt;
//      double val = (signal_x < m_video_signal.size() ? m_video_signal.at(signal_x) : 0);
      double val = convolveSingle(m_video_signal, sample_kern, signal_x);

      int x  = px.pt.x;
      int y  = px.pt.y;
      int ch = px.ch;

      //if (m_progress >= 1.0*y/(m.num_lines-1))
        m_has_line.at(y) = true;

      setChannel(img[x][y], ch, clipToByte(round(val*255)));
    }
  }

  if (m_mode == MODE_R36 || m.family == MODE_PD)
    reconstructChroma420(img, m);

  m_pixbuf_rx->fill(0x000000ff);

  uint8_t *p;
  uint8_t *pixels;
  pixels = m_pixbuf_rx->get_pixels();
  int rowstride = m_pixbuf_rx->get_rowstride();

  for (int y = 0; y < m.num_lines; y++) {
    for (int x = 0; x < m.scan_pixels; x++) {

      p = pixels + y * rowstride + x * 3;

#ifdef RGBONLY
      decodeRGB(img[x][y], p);
#else
      if (!m_has_line.at(y)) {
        p[0] = p[1] = p[2] = 0;
      } else if (m.color_enc == COLOR_RGB) {
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

  printf("│  scale to: %dx%d\n",img_width, img_height);
  Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled =
    m_pixbuf_rx->scale_simple(img_width, img_height, Gdk::INTERP_BILINEAR);

  m_pixbuf_rx->save("testi.png", "png");

  printf("└──\n");
  return pixbuf_scaled;
}

Glib::RefPtr<Gdk::Pixbuf> Picture::renderSync(Wave sync_delta, int line_width) {

  const double dt = 1.0 * m_sync_decim_ratio / m_original_samplerate;

  const int graph_height = round(line_width * 0.2);
  const int waterfall_height = line_width / 3;

  Glib::RefPtr<Gdk::Pixbuf> pixbuf_sy;
  pixbuf_sy = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, line_width, waterfall_height + graph_height);
  pixbuf_sy->fill(0x000000ff);

  uint8_t *p;
  uint8_t *pixels;
  pixels = pixbuf_sy->get_pixels();
  int rowstride = pixbuf_sy->get_rowstride();

  Wave acc(line_width);
  for (int i=0; i<line_width*m_mode.num_lines; i++) {
    int x = i % line_width;
    double signal_t = 1.0 * i / line_width * m_mode.t_period / m_tx_speed;
    int signal_idx = round(signal_t / dt);
    if (signal_idx < (int)sync_delta.size())
      acc.at(x) += sync_delta.at(signal_idx);
  }

  int m_i = maxIndex(acc);

  printf("line = %.4f ms (%.6f lpm)\n",m_mode.t_period / m_tx_speed * 1e3, 60.0 / (m_mode.t_period / m_tx_speed));

  for (int y=0; y<waterfall_height; y++) {
    for (int x=0; x<line_width; x++) {
      int line = std::round(1.0 * y / waterfall_height * m_mode.num_lines);
      double signal_t = 1.0 * (m_i+(line+.5)*line_width+x) / line_width * m_mode.t_period / m_tx_speed;
      int signal_idx = round(signal_t / dt);
      double val = 0;
      p = pixels + y * rowstride + x * 3;
      p[0] = p[1] = p[2] = 0;
      if (signal_idx < (int)sync_delta.size()) {
        val = sync_delta.at(signal_idx)*4;
        if (val > 0) {
          p[1] = clipToByte(val);
        } else {
          p[0] = clipToByte(-val);
        }
      }
      if (x == line_width/2) {
        p[2] = 255;
      }
    }
  }
  for (int y=0; y<graph_height; y++) {
    for (int x=0; x<line_width; x++) {
      p = pixels + (waterfall_height + y) * rowstride + x * 3;
      p[0] = p[1] = p[2] = 32;
      if (acc.at(m_i) != 0 && 1.2-0.8*y/graph_height <= acc.at((x+m_i+line_width/2)%line_width)/acc.at(m_i) )
        p[1] = p[2] = 255;
    }
  }

  return pixbuf_sy;

}

void Picture::resync () {

#ifdef NOSYNC
  m_drift = 1.0;
  m_starts_at = 0.0;
  return;
#endif

  printf("\nresync\n======\n");

  const double dt = 1.0 * m_sync_decim_ratio / m_original_samplerate;
  const int line_width = 500;//round(m.t_period * m_original_samplerate / m_sync_decim_ratio);

  Wave sync_delta;

  double kernel_scale = m_mode.t_sync / dt;
  printf("│  kernel_scale: %g\n",kernel_scale);
  printf("│  sync signal has: %ld samples\n",m_sync_signal.size());
  Kernel sync_kernel(KERNEL_PULSE, kernel_scale);

  {
    std::lock_guard<std::mutex> guard(m_mutex);

    double kernel_scale = m.t_sync / dt;
    printf("kernel_scale = %f\n",kernel_scale);
    Kernel sync_kernel(KERNEL_PULSE, kernel_scale);
    Wave sync_delta = convolve(m_sync_signal, sync_kernel);

    /* slant */
    std::vector<double> histogram;
    int peak_speed_at = 0;
    double peak_speed_val = 0.0;
    const double min_speed  = 0.998;
    const double max_speed  = 1.002;
    const double speed_step = 0.00005;

    for (double spd = min_speed; spd <= max_speed; spd += speed_step) {
      Wave acc(line_width);
      int peak_x = 0;
      for (int i=0; i<line_width*m.num_lines; i++) {
        int x = i % line_width;
        double signal_t = 1.0 * i / line_width * m.t_period / spd;
        int signal_idx = round(signal_t / dt);
        if (signal_idx >= 0 && signal_idx < (int)sync_delta.size())
          acc.at(x) += sync_delta.at(signal_idx);
        if (acc[x] > acc[peak_x]) {
          peak_x = x;
        }
      }
      histogram.push_back(acc[peak_x]);
      if (acc[peak_x] > peak_speed_val) {
        peak_speed_at = histogram.size()-1;
        peak_speed_val = acc[peak_x];
      }
    }

    double peak_refined = gaussianPeak(histogram, peak_speed_at);
    double txspeed = min_speed + peak_refined*speed_step;

    /* align */
    Wave acc(line_width);
    for (int i=0; i<line_width*m.num_lines; i++) {
      int x = i % line_width;
      double signal_t = 1.0 * i / line_width * m.t_period / txspeed;
      int signal_idx = round(signal_t / dt);
      if (signal_idx < (int)sync_delta.size())
        acc.at(x) += sync_delta.at(signal_idx);

      //if (thresh > 0.0)
      //  acc.at(x) = acc.at(x) >= thresh ? 1.0 : 0.0;
    }

    int m_i = maxIndex(acc);
    double peak_align = gaussianPeak(acc, m_i, true);

    double align_time = peak_align/line_width*m.t_period;// - m.t_sync*0.5;
    if (m.family == MODE_SCOTTIE)
      align_time -= m.t_sync + m.t_sep*2 + m.t_scan * 2;

    if (align_time > m.t_period/2)
      align_time -= m.t_period;

    //fprintf(stderr,"drift = %.5f\n",drift);

    m_tx_speed = txspeed;
    m_starts_at = align_time;

#define WRITE_SYNC
#ifdef WRITE_SYNC
    renderSync(sync_delta, line_width)->save("sync.png", "png");
#endif
  }

  printf("│  align: %f/%d (%f ms)\n│  tx speed: %f (error = %+.0f ppm)\n",peak_align,line_width,m_starts_at*1e3,txspeed,(txspeed-1) * 1e6);
  //m_starts_at = 0;

  printf("└──\n");
}

// Time instants for all pixels
std::vector<PixelSample> pixelSamplingPoints(ModeSpec mode) {
  std::vector<PixelSample> pixelsamples;
  for (int y=0; y<mode.num_lines; y++) {
    for (int x=0; x<mode.scan_pixels; x++) {
      for (int ch=0; ch < (mode.color_enc == COLOR_MONO ? 1 : 3); ch++) {
        PixelSample px;
        px.pt = Point(x,y);
        px.ch = ch;
        px.t  = -1;

        if (mode.family == MODE_SCOTTIE) {
          px.t = y*(mode.t_period) + (x+0.5)/mode.scan_pixels * mode.t_scan +
            mode.t_sync + (ch+1) * mode.t_sep + ch*mode.t_scan +
            (ch == 2 ? mode.t_sync : 0);
        }

        else if (mode.family == MODE_PD) {
          if (ch == 0) {
            double line_video_start = (y/2)*(mode.t_period) + mode.t_sync + mode.t_porch;
            px.t = line_video_start + (y%2 == 1 ? 3*mode.t_scan : 0) +
              (x+0.0)/mode.scan_pixels * mode.t_scan;
          } else if (ch == 1 && y < mode.num_lines*.5 && x < mode.scan_pixels*.5) {
            double line_video_start = y*(mode.t_period) + mode.t_sync + mode.t_porch;
            px.t = line_video_start + mode.t_scan +
              (x+0.0)/mode.scan_pixels * mode.t_scan * 2;
          } else if (ch == 2 && y < mode.num_lines*.5 && x < mode.scan_pixels*.5) {
            double line_video_start = y*(mode.t_period) + mode.t_sync + mode.t_porch;
            px.t = line_video_start + 2*mode.t_scan +
              (x+0.0)/mode.scan_pixels * mode.t_scan * 2;
          }
        }

        else if (mode.family == MODE_ROBOT && mode.color_enc == COLOR_YUV422) {
          double line_video_start = y*(mode.t_period) + mode.t_sync + mode.t_porch;
          if (ch == 0) {
            px.t = line_video_start + (x+0.0) / mode.scan_pixels * mode.t_scan;
          } else if (ch == 1 && x < mode.scan_pixels*.5) {
            px.t = line_video_start + mode.t_scan + mode.t_sep + mode.t_chanporch +
             (x+0.0) / (mode.scan_pixels-1) * mode.t_scan;
          } else if (ch == 2 && x < mode.scan_pixels*.5) {
            px.t = line_video_start + 1.5*mode.t_scan + 2*(mode.t_sep + mode.t_chanporch) +
             (x+0.0) / (mode.scan_pixels-1) * mode.t_scan;
          }
        }

        else if (mode.family == MODE_ROBOT && mode.color_enc == COLOR_YUV420) {
          if (ch == 0) {
            double line_video_start = y*(mode.t_period) + mode.t_sync + mode.t_porch;
            px.t = line_video_start + (x+.5) / mode.scan_pixels * mode.t_scan;
          } else if (ch == 1 && y < mode.num_lines*.5 && x < mode.scan_pixels*.5) {
            double line_video_start = 2*y*(mode.t_period) + mode.t_sync + mode.t_porch;
            px.t = line_video_start + mode.t_scan + mode.t_sep + mode.t_chanporch +
              (x+0.0) / mode.scan_pixels * mode.t_scan;
          } else if (ch == 2 && y < mode.num_lines*.5 && x < mode.scan_pixels*.5) {
            double line_video_start = (2*y+1)*(mode.t_period) + mode.t_sync + mode.t_porch;
            px.t = line_video_start + mode.t_scan + mode.t_sep + mode.t_chanporch +
              (x+0.0) / mode.scan_pixels * mode.t_scan;
          }

        } else {
          px.t = y*(mode.t_period) + mode.t_sync + mode.t_porch + ch*(mode.t_scan + mode.t_sep) +
            (x+0.0)/mode.scan_pixels * mode.t_scan;
        }

        if (px.t >= 0) {
          pixelsamples.push_back(px);
        }
      }
    }
  }

  std::sort(pixelsamples.begin(), pixelsamples.end(), [](PixelSample a, PixelSample b) {
      return a.t < b.t;
  });

  return pixelsamples;
}

void Picture::save(std::string dir) {
  std::string filename = dir + "/" + std::string("slowrx_") + m_safe_timestamp + ".png";
  Glib::RefPtr<Gdk::Pixbuf> pb = renderPixbuf(500);
  std::cout << "saving " << filename << "\n";
  pb->save(filename, "png");
}
