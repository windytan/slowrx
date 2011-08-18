#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>

#include "common.h"

guchar       VISmap[128];
gint16      *PcmBuffer       = NULL;
int          PcmPointer      = 0;
guchar      *StoredLum       = NULL;
double      *in              = NULL;
double      *out             = NULL;
gshort       HedrShift       = 0;
int          PWRdBthresh[10] = {0,  -3, -5, -10, -15, -20, -25, -30, -40, -50};
int          SNRdBthresh[10] = {30, 15, 10,   5,   3,   0,  -3,  -5, -10, -15};
gboolean     Adaptive        = TRUE;
gboolean     ManualActivated = FALSE;
gboolean     Abort           = FALSE;
gboolean    *HasSync         = NULL;

GtkWidget   *mainwindow      = NULL;
GtkWidget   *RxImage         = NULL;
GtkWidget   *statusbar       = NULL;
GtkWidget   *vugrid          = NULL;
GtkWidget   *infolabel       = NULL;
GtkWidget   *aboutdialog     = NULL;
GtkWidget   *prefdialog      = NULL;
GtkWidget   *cardcombo       = NULL;
GtkWidget   *modecombo       = NULL;
GtkWidget   *togslant        = NULL;
GtkWidget   *togsave         = NULL;
GtkWidget   *togadapt        = NULL;
GtkWidget   *togrx           = NULL;
GtkWidget   *togfsk          = NULL;
GtkWidget   *btnabort        = NULL;
GtkWidget   *btnstart        = NULL;
GtkWidget   *manualframe     = NULL;
GtkWidget   *shiftspin       = NULL;
GtkWidget   *pwrimage        = NULL;
GtkWidget   *snrimage        = NULL;
GtkWidget   *idlabel         = NULL;

GdkPixbuf   *RxPixbuf        = NULL;
GdkPixbuf   *DispPixbuf      = NULL;
GdkPixbuf   *pixbufPWR       = NULL;
GdkPixbuf   *pixbufSNR       = NULL;

GtkListStore *savedstore     = NULL;

snd_pcm_t   *pcm_handle      = NULL;

fftw_plan    Plan1024        = NULL;
fftw_plan    Plan2048        = NULL;

// Draw a fancy gradient
void ClearPixbuf(GdkPixbuf *pb, gushort width, gushort height) {

  guint   x,y,rowstride;
  guchar *pixels, *p;
  rowstride = gdk_pixbuf_get_rowstride (pb);
  pixels    = gdk_pixbuf_get_pixels    (pb);

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      p = pixels + y * rowstride + x * 3;
      p[0] = p[2] = p[1] = 96 + (1.0*y/height) * 32;
      p[2] = 0xff;
//      p[0] = p[1] = p[2] = (x % 10 == 0 || y % 10 == 0) ? 255 : 0;
    }
  }

}

// Return the bin index matching the given frequency
guint GetBin (double Freq, int FFTLen) {
  return (Freq / 44100 * FFTLen);
}

// Clip to [0..255]
guchar clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  (unsigned char)round(a);
}


// Convert degrees -> radians
double deg2rad (double Deg) {
  return (Deg / 180) * M_PI;
}

// Quit
void delete_event() {
  gtk_main_quit ();
}

// Transform the NoiseAdapt toggle state into a variable
void GetAdaptive() {
  Adaptive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(togadapt));
}

// Manual Start clicked
void ManualStart() {
  ManualActivated = TRUE;
}

// Abort clicked
void AbortRx() {
  Abort = TRUE;
}
