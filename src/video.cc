#include "common.h"
#include "dsp.h"
#include "gui.h"
#include "picture.h"

// Time instants for all pixels
std::vector<PixelSample> pixelSamplingPoints(SSTVMode mode) {
  _ModeSpec m = ModeSpec[mode];
  std::vector<PixelSample> pixel_grid;
  for (size_t y=0; y<m.num_lines; y++) {
    for (size_t x=0; x<m.scan_pixels; x++) {
      for (size_t ch=0; ch < (m.color_enc == COLOR_MONO ? 1 : 3); ch++) {
        PixelSample px;
        px.pt = Point(x,y);
        px.ch = ch;
        bool exists = true;

        if (m.family == MODE_MARTIN || m.family == MODE_PASOKON ||
            m.family == MODE_WRAASE2 || mode == MODE_R8BW || mode == MODE_R12BW) {
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

