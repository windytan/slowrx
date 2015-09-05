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
        px.exists = true;

        if (m.family == MODE_MARTIN) {
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
          } else if ((ch==1 || ch==2) && (y%2)==1) {
            px.exists = false;
          }
        }

        else if (m.family == MODE_PASOKON || m.family == MODE_WRAASE2) {
          px.t = y*(m.t_period) + m.t_sync + m.t_porch + ch*(m.t_sep+m.t_scan) +
            (x+0.5)/m.scan_pixels * m.t_scan;
        }

        else if (mode == MODE_R72 || mode == MODE_R24) {
          double line_video_start = y*(m.t_period) + m.t_sync + m.t_porch;
          switch (ch) {
            case (0):
              px.t = line_video_start + (x+.5) / m.scan_pixels * m.t_scan;
              break;
            case (1):
              px.t = line_video_start + m.t_scan + m.t_sep +
               (x+.5) / m.scan_pixels * m.t_scan / 2;
              break;
            case(2):
              px.t = line_video_start + 1.5*m.t_scan + 2*m.t_sep +
               (x+.5) / m.scan_pixels * m.t_scan / 2;
              break;
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
          } else if ((ch==1 || ch==2) && (y%2) == 1) {
            px.exists = false;
          }
        }

        else if (mode == MODE_R8BW || mode == MODE_R12BW) {
          px.t = y*(m.t_period) + m.t_sync + m.t_porch +
            (x+.5) / m.scan_pixels * m.t_scan;
        }

        pixel_grid.push_back(px);
      }
    }
  }

  std::sort(pixel_grid.begin(), pixel_grid.end(), [](PixelSample a, PixelSample b) {
      return a.t < b.t;
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

  fprintf(stderr,"receive %s\n",ModeSpec[mode].name.c_str());

  _ModeSpec m = ModeSpec[mode];
  Picture* pic = new Picture(mode);

  //Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("com.windytan.slowrx");

  double next_sync_sample_time = 0;
  double next_video_sample_time = 0;

  double t_total;
  if (m.family == MODE_SCOTTIE) {
    t_total = m.t_sync + m.num_lines * m.t_period;
  } else if (m.family == MODE_PD) {
    t_total = m.num_lines * m.t_period / 2;
  } else {
    t_total = m.num_lines * m.t_period;
  }
  size_t idx = 0;

  for (double t=0; t < t_total && dsp->is_open(); t += dsp->forward()) {

    if (t >= next_sync_sample_time) {
      double p = dsp->syncPower();
      pic->pushToSyncSignal(p);
      next_sync_sample_time += pic->getSyncDt();
    }

    bool is_adaptive = true;

    if ( t >= next_video_sample_time ) {

      pic->pushToVideoSignal(dsp->lum(pic->getMode(), is_adaptive));

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
        fprintf(stderr,"] %.1f %% (%.1f s / %.1f s)\r",prog*100,t,t_total);
      }

      /*if (dsp->isLive() && (idx+1) % 10000 == 0) {
        fprintf(stderr,"\n(resync)\n");
        pic->resync();
        pic->renderPixbuf();
      }*/
      next_video_sample_time += pic->getVideoDt();
      idx++;
    }
  }

  pic->resync();
  pic->renderPixbuf();

  fprintf(stderr, "\n");

  return true;

}
