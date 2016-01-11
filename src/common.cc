#include "common.h"
#include <gtkmm.h>
#include <thread>

Glib::KeyFile config;

//std::vector<std::thread> threads(2);

std::string version_string() {
  return "0.7-dev";
}

// Clip to [0..255]
uint8_t clipToByte (double a) {
  if      (a < 0.0)   return 0;
  else if (a > 255.0) return 255;
  return  std::round(a);
}

// Clip to [0..1]
double fclipToByte (double a) {
  if      (a < 0.0) return 0.0;
  else if (a > 1.0) return 1.0;
  return  a;
}

double deg2rad (double Deg) {
  return (Deg / 180.0) * M_PI;
}

double rad2deg (double rad) {
  return (180.0 / M_PI) * rad;
}

int maxIndex (std::vector<double> v) {
  const int n = sizeof(v) / sizeof(double);
  return distance(v.begin(), max_element(v.begin(), v.end()));
}

void ensure_dir_exists(std::string dir) {
  struct stat buf;

  int i = stat(dir.c_str(), &buf);
  if (i != 0) {
    if (mkdir(dir.c_str(), 0777) != 0) {
      perror("Unable to create directory for output file");
      exit(EXIT_FAILURE);
    }
  }
}

template<class T> CirBuffer<T>::CirBuffer(int size) : m_data(size * 2), m_head(0), m_tail(0), m_fill_count(0), m_len(size) {
  assert(size > 0);
}

template<class T> void CirBuffer<T>::moveHead(int n) {
  m_head = (m_head + n) % m_len;
}

template<class T> void CirBuffer<T>::forward(int n) {
  m_tail = (m_tail + n) % m_len;
  m_fill_count -= n;
}

template<class T> int CirBuffer<T>::getFillCount() const {
  return m_fill_count;
}

template<class T> int CirBuffer<T>::size() const {
  return m_data.size();
}

template<class T> T CirBuffer<T>::at(int n) const {
  int i = m_tail + n;
  if (i < 0) {
    i = m_len - (abs(i) % m_len);
  } else {
    i = i % m_len;
  }

  return m_data.at(i);
}

template<class T> void CirBuffer<T>::append(const std::vector<T>& input_data, int num_elems) {

  assert(num_elems <= (int)input_data.size());

  for (int i=0; i<num_elems; i++)
    m_data.at((m_head + i) % m_len) = input_data[i];

  // mirror
  //for (int i=0; i<m_len; i++)
  //  m_data.at(m_len + i) = m_data.at(i);

  m_head = (m_head + num_elems) % m_len;
  m_fill_count += num_elems;
  m_fill_count = std::min(m_fill_count, m_len);

}

template<class T> void CirBuffer<T>::append(T input_element) {

  m_data.at(m_head) = input_element;

  // mirror
  m_data.at(m_head + m_len) = m_data.at(m_head);

  m_head = (m_head + 1) % m_len;
  m_fill_count += 1;
  m_fill_count = std::min(m_fill_count, m_len);

}

// for the linker
template class CirBuffer<double>;
template class CirBuffer<float>;

/*** Gtk+ event handlers ***/


// Quit
void evt_deletewindow() {
  //gtk_main_quit ();
}

// Transform the NoiseAdapt toggle state into a variable
void evt_GetAdaptive() {
  //Adaptive = gui.tog_adapt->get_active();
}

// Manual Start clicked
void evt_ManualStart() {
  //ManualActivated = true;
}

// Manual slant adjust
//void evt_clickimg(Gtk::Widget *widget, GdkEventButton* event, Gdk::WindowEdge edge) {
  /*static double prevx=0,prevy=0,newrate;
  static bool   secondpress=false;
  double        x,y,dx,dy,xic;

  (void)widget;
  (void)edge;

  if (event->type == Gdk::BUTTON_PRESS && event->button == 1 && gui.tog_setedge->get_active()) {

    x = event->x * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0);
    y = event->y * (ModeSpec[CurrentPic.Mode].ImgWidth / 500.0) / ModeSpec[CurrentPic.Mode].LineHeight;

    if (secondpress) {
      secondpress=false;

      dx = x - prevx;
      dy = y - prevy;

      //gui.tog_setedge->set_active(false);

      // Adjust sample rate, if in sensible limits
      newrate = CurrentPic.Rate + CurrentPic.Rate * (dx * ModeSpec[CurrentPic.Mode].PixelTime) / (dy * ModeSpec[CurrentPic.Mode].LineHeight * ModeSpec[CurrentPic.Mode].LineTime);
      if (newrate > 32000 && newrate < 56000) {
        CurrentPic.Rate = newrate;

        // Find x-intercept and adjust skip
        xic = fmod( (x - (y / (dy/dx))), ModeSpec[CurrentPic.Mode].ImgWidth);
        if (xic < 0) xic = ModeSpec[CurrentPic.Mode].ImgWidth - xic;
        CurrentPic.Skip = fmod(CurrentPic.Skip + xic * ModeSpec[CurrentPic.Mode].PixelTime * CurrentPic.Rate,
          ModeSpec[CurrentPic.Mode].LineTime * CurrentPic.Rate);
        if (CurrentPic.Skip > ModeSpec[CurrentPic.Mode].LineTime * CurrentPic.Rate / 2.0)
          CurrentPic.Skip -= ModeSpec[CurrentPic.Mode].LineTime * CurrentPic.Rate;

        // Signal the listener to exit from GetVIS() and re-process the pic
        ManualResync = true;
      }

    } else {
      secondpress = true;
      prevx = x;
      prevy = y;
    }
  } else {
    secondpress=false;
    //gui.tog_setedge->set_active(false);
  }*/
//}

ProgressBar::ProgressBar(double maxval, int width) : m_maxval(maxval), m_val(0), m_width(width) {
  assert(m_maxval > 0);
  assert(m_width > 0);
  set(0);
}

void ProgressBar::set(double val) {
  m_val = val;

  fprintf(stderr,"  [");
  double prog = m_val / m_maxval;
  int prog_points = round(prog * m_width);
  for (int i=0;i<prog_points;i++) {
    fprintf(stderr,"=");
  }
  for (int i=prog_points;i<m_width;i++) {
    fprintf(stderr," ");
  }
  fprintf(stderr,"] %.1f %%\r",prog*100);
}

void ProgressBar::finish() {
  fprintf(stderr, "\n");
}
