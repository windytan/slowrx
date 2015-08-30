#include "common.hh"

// TODO: middle point of sync pulse
void resync (Picture* pic) {
  int line_width = ModeSpec[pic->mode].tLine / pic->sync_dt;

  /* speed */
  std::vector<int> histogram;
  int peak_speed = 0;
  int peak_speed_val = 0;
  double min_spd = 0.998;
  double max_spd = 1.002;
  double spd_step = 0.00005;
  for (double speed = min_spd; speed <= max_spd; speed += spd_step) {
    std::vector<int> acc(line_width);
    int peak_x = 0;
    for (size_t i=1; i<pic->sync_signal.size(); i++) {
      int x = int(i / speed + .5) % line_width;
      acc[x] += pic->sync_signal[i] && !pic->sync_signal[i-1];
      if (acc[x] > acc[peak_x]) {
        peak_x = x;
      }
    }
    histogram.push_back(acc[peak_x]);
    if (acc[peak_x] > peak_speed_val) {
      peak_speed = histogram.size()-1;
      peak_speed_val = acc[peak_x];
    }
  }

  double peak_refined = peak_speed +
    gaussianPeak(histogram[peak_speed-1], histogram[peak_speed], histogram[peak_speed+1]);
  double spd = 1.0/(min_spd + peak_refined*spd_step);

  /* align */
  size_t peak_align = 0;
  std::vector<int> acc(line_width);
  for (size_t i=1;i<pic->sync_signal.size(); i++) {
    int x = int(i * spd + .5) % line_width;
    acc[x] += pic->sync_signal[i] && !pic->sync_signal[i-1];
    if (acc[x] > acc[peak_align])
      peak_align = x;
  }

  double peak_align_refined = peak_align +
    (peak_align == 0 || peak_align == acc.size()-1 ? 0 :
     gaussianPeak(acc[peak_align-1], acc[peak_align], acc[peak_align+1]));

  if (ModeSpec[pic->mode].SyncOrder == SYNC_SCOTTIE)
    peak_align_refined = peak_align_refined -
      (line_width*(ModeSpec[pic->mode].tSync + ModeSpec[pic->mode].tSep*2 + ModeSpec[pic->mode].tScan*2)/ModeSpec[pic->mode].tLine);

  printf("%f\n",peak_align_refined);

  if (peak_align_refined > line_width/2.0)
    peak_align_refined -= line_width;

  fprintf(stderr,"%.5f\n",spd);
  fprintf(stderr, "--> %.1f\n",44100*spd);
  fprintf(stderr,"align = %f = %.3f %% (%.3f ms)\n",
      peak_align_refined,
      1.0*peak_align_refined/line_width*100,
      1.0*peak_align_refined/line_width*ModeSpec[pic->mode].tLine*1000);

  pic->speed = spd;
  pic->starts_at = 1.0*peak_align_refined/line_width*ModeSpec[pic->mode].tLine;

}
