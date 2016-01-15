#ifndef PICTURE_H_
#define PICTURE_H_

#include "common.h"
#include <gtkmm.h>
#include <ctime>

class Picture {

  public:

    Picture(SSTVMode mode, int srate);

    void pushToSyncSignal (double s);
    void pushToVideoSignal (double s);

    SSTVMode getMode() const;
    double getTxSpeed () const;
    double getStartsAt () const;
    int getVideoDecimRatio () const;
    int getSyncDecimRatio () const;
    double getSyncSignalAt(int i) const;
    double getVideoSignalAt(int i) const;
    std::string getTimestamp() const;

    Glib::RefPtr<Gdk::Pixbuf> renderPixbuf(int width=320);
    Glib::RefPtr<Gdk::Pixbuf> renderSync(Wave, int width);
    void resync();
    void save(std::string);
    void setProgress(double);

    std::mutex m_mutex;

  private:
    SSTVMode m_mode;
    std::vector<PixelSample> m_pixel_grid;
    std::vector<bool> m_has_line;
    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf_rx;
    double m_progress;
    int    m_original_samplerate;
    Wave   m_video_signal;
    int    m_video_decim_ratio;
    Wave   m_sync_signal;
    int    m_sync_decim_ratio;
    double m_tx_speed;
    double m_starts_at;
    char   m_safe_timestamp[100];
    char   m_timestamp[100];
};

#endif
