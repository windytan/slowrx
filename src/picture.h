#ifndef PICTURE_H_
#define PICTURE_H_

#include "common.h"
#include "gui.h"

class Picture {

  public:

    Picture(SSTVMode _mode)
      : mode_(_mode), pixel_grid_(pixelSamplingPoints(_mode)), video_signal_(),
        video_dt_(ModeSpec[_mode].t_scan/ModeSpec[_mode].scan_pixels/2), sync_signal_(),
        sync_dt_(ModeSpec[_mode].t_period / ModeSpec[_mode].scan_pixels/3), drift_(1.0),
        starts_at_(0.0) {}

    void pushToSyncSignal (double s);
    void pushToVideoSignal (double s);

    SSTVMode getMode() const;
    double getDrift () const;
    double getStartsAt () const;
    double getVideoDt () const;
    double getSyncDt () const;
    double getSyncSignalAt(size_t i) const;
    double getVideoSignalAt(size_t i) const;

    Glib::RefPtr<Gdk::Pixbuf> renderPixbuf(unsigned min_width=320, int upsample_factor=4);
    void resync();
    void saveSync();


  private:
    SSTVMode mode_;
    std::vector<PixelSample> pixel_grid_;
    Wave video_signal_;
    double video_dt_;
    Wave sync_signal_;
    double sync_dt_;
    double drift_;
    double starts_at_;
};

#endif
