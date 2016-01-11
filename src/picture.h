#ifndef PICTURE_H_
#define PICTURE_H_

#include "common.h"
#include <gtkmm.h>
#include <ctime>

class Picture {

  public:

    Picture(SSTVMode mode)
      : m_mode(mode), m_pixel_grid(pixelSamplingPoints(mode)), m_video_signal(),
        m_video_dt(getModeSpec(mode).t_scan/getModeSpec(mode).scan_pixels/2), m_sync_signal(),
        m_sync_dt(getModeSpec(mode).t_period / getModeSpec(mode).scan_pixels/3), m_drift(1.0),
        m_starts_at(0.0) {
          std::time_t t = std::time(NULL);
          std::strftime(m_timestamp, sizeof(m_timestamp),"%F %Rz", std::gmtime(&t));
          std::strftime(m_safe_timestamp, sizeof(m_timestamp),"%Y%m%d_%H%M%SZ", std::gmtime(&t));
        }

    void pushToSyncSignal (double s);
    void pushToVideoSignal (double s);

    SSTVMode getMode() const;
    double getDrift () const;
    double getStartsAt () const;
    double getVideoDt () const;
    double getSyncDt () const;
    double getSyncSignalAt(int i) const;
    double getVideoSignalAt(int i) const;
    std::string getTimestamp() const;

    Glib::RefPtr<Gdk::Pixbuf> renderPixbuf(int width=320);
    void resync();
    void saveSync();
    void save(std::string);
    void setProgress(double);

    std::mutex m_mutex;

  private:
    SSTVMode m_mode;
    std::vector<PixelSample> m_pixel_grid;
    Wave m_video_signal;
    double m_video_dt;
    Wave m_sync_signal;
    double m_sync_dt;
    double m_drift;
    double m_starts_at;
    char m_safe_timestamp[100];
    char m_timestamp[100];
};

#endif
