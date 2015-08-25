#include "common.hh"

double findSyncHough (SSTVMode Mode, std::vector<bool> has_sync) {

}

void findSyncRansac(SSTVMode Mode, std::vector<bool> has_sync) {
  int line_width = ModeSpec[Mode].NumLines;//ModeSpec[Mode].tLine / ModeSpec[Mode].tSync * 4;
  std::vector<std::pair<int,int> > sync_pixels;
  for (int y=0; y<ModeSpec[Mode].NumLines; y++) {
    for (int x=0; x<line_width; x++) {
      if (y+x>0 && has_sync[y*line_width + x] && !has_sync[y*line_width + x - 1]) {
        sync_pixels.push_back({x,y});
        printf("%d,%d\n",x,y);
      }
    }
  }

  std::pair<std::pair<int,int>, std::pair<int,int> > best_line;
  double best_dist = -1;
  int it_num = 0;
  while (++it_num < 300) {

    std::random_shuffle(sync_pixels.begin(), sync_pixels.end());

    std::pair<std::pair<int,int>, std::pair<int,int> > test_line = {sync_pixels[0], sync_pixels[1]};
    int x0 = test_line.first.first;
    int y0 = test_line.first.second;
    int x1 = test_line.second.first;
    int y1 = test_line.second.second;
    double total_sq_dist = 0;
    int total_good = 0;

    for(std::pair<int,int> pixel : sync_pixels) {
      int x = pixel.first;
      int y = pixel.second;
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
