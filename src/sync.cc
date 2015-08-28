#include "common.hh"

void findSyncHough (SSTVMode Mode, std::vector<bool> has_sync) {

}

double findSyncAutocorr (SSTVMode Mode, std::vector<bool> has_sync) {
  int line_width = ModeSpec[Mode].NumLines;
  std::vector<int> jakauma;
  int peak_speed = 0;
  int peak_speed_val = 0;
  int peak_pos = 0;
  double min_spd = 0.998;
  double max_spd = 1.002;
  double spd_step = 0.0002;

  for (double speed = min_spd; speed <= max_spd; speed += spd_step) {
    std::vector<int> acc(line_width);
    int peak_x = 0;
    for (int i=0; i<has_sync.size(); i++) {
      int x = int(i / speed + .5) % line_width;
      acc[x] += has_sync[i];
      if (acc[x] > acc[peak_x]) {
        peak_x = x;
      }
    }
    jakauma.push_back(acc[peak_x]);
    if (acc[peak_x] > peak_speed_val) {
      peak_speed = jakauma.size()-1;
      peak_speed_val = acc[peak_x];
      peak_pos = peak_x;
    }
    printf("(%.5f=%.0f) %d\n",1.0/speed,44100.0/speed,acc[peak_x]);
    
    /*for (int x=0; x<acc.size(); x++) {
      printf("%4d ",acc[x]);
    }
    printf("\n");*/
  }
  double peak_refined = peak_speed + gaussianPeak(jakauma[peak_speed-1], jakauma[peak_speed], jakauma[peak_speed+1]);
  double spd = 1.0/(min_spd + peak_refined*spd_step);
  printf("%.5f\n",spd);
  printf("--> %.1f\n",44100*spd);
  printf("pos = %d (%.1f ms)\n",peak_pos,1.0*peak_pos/line_width*ModeSpec[Mode].tLine*1000);
  return spd;
}

void findSyncRansac(SSTVMode Mode, std::vector<bool> has_sync) {
  int line_width = ModeSpec[Mode].NumLines;//ModeSpec[Mode].tLine / ModeSpec[Mode].tSync * 4;
  std::vector<Point> sync_pixels;
  for (int y=0; y<ModeSpec[Mode].NumLines; y++) {
    for (int x=0; x<line_width; x++) {
      if (y+x>0 && has_sync[y*line_width + x] && !has_sync[y*line_width + x - 1]) {
        sync_pixels.push_back(Point(x,y));
        printf("%d,%d\n",x,y);
      }
    }
  }

  std::pair<Point, Point > best_line;
  double best_dist = -1;
  int it_num = 0;
  while (++it_num < 300) {

    std::random_shuffle(sync_pixels.begin(), sync_pixels.end());

    std::pair<Point, Point > test_line = {sync_pixels[0], sync_pixels[1]};
    int x0 = test_line.first.x;
    int y0 = test_line.first.y;
    int x1 = test_line.second.x;
    int y1 = test_line.second.y;
    double total_sq_dist = 0;
    int total_good = 0;

    for(Point pixel : sync_pixels) {
      int x = pixel.x;
      int y = pixel.y;
      // Point distance to line
      double d = 1.0 * ((y0-y1)*x + (x1-x0)*y + (x0*y1 - x1*y0)) / sqrt(pow(x1-x0,2) + pow(y1-y0,2));

      if (fabs(d) < 6 || fabs(d-line_width) < 6) {
        total_good ++;//+= sqrt(fabs(d));
      }

    }
    if (best_dist < 0 || total_good > best_dist) {
      best_line = test_line;
      best_dist = total_good;
      printf("(it. %d) for (%d,%d) (%d,%d) total_sq_dist=%f, total_good=%d\n",it_num,x0,y0,x1,y1,total_sq_dist,total_good);
    }
  }

}
