#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

guchar       VISmap[128];
gint16       PcmBuffer[2048] = {0};
int          PcmPointer      = 0;
int          Sample          = 0;
double      *StoredFreq      = NULL;
guint        StoredFreqRate  = 0;
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
    }
  }

}

// Return the bin index matching the given frequency
guint GetBin (double Freq, int FFTLen) {
  return (Freq / SRATE * FFTLen);
}

// Clip to [0..255]
guchar clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  (unsigned char)round(a);
}

// Draw signal level meters according to given values
void setVU (short int PcmValue, double SNRdB) {
  int x,y;
  int PWRdB = (int)round(10 * log10(pow(PcmValue/32767.0,2)));
  guchar *pixelsPWR, *pixelsSNR, *pPWR, *pSNR;
  unsigned int rowstridePWR,rowstrideSNR;

  rowstridePWR = gdk_pixbuf_get_rowstride (pixbufPWR);
  pixelsPWR    = gdk_pixbuf_get_pixels    (pixbufPWR);
  
  rowstrideSNR = gdk_pixbuf_get_rowstride (pixbufSNR);
  pixelsSNR    = gdk_pixbuf_get_pixels    (pixbufSNR);

  for (y=0; y<20; y++) {
    for (x=0; x<100; x++) {

      pPWR = pixelsPWR + y * rowstridePWR + (99-x) * 3;
      pSNR = pixelsSNR + y * rowstrideSNR + (99-x) * 3;

      if (y > 1 && y < 18 && x % 10 > 1 && x % 10 < 8 && x % 2 == 0 && y % 2 == 0) {

        if (PWRdB >= PWRdBthresh[x/10]) {
          pPWR[0] = 0x39;
          pPWR[1] = 0xde;
          pPWR[2] = 0xd4;
        } else {
          pPWR[0] = pPWR[1] = pPWR[2] = 0x80;
        }

        if (SNRdB >= SNRdBthresh[x/10]) {        
          pSNR[0] = 0xef;
          pSNR[1] = 0xe4;
          pSNR[2] = 0x34;
        } else {
          pSNR[0] = pSNR[1] = pSNR[2] = 0x80;
        }

      } else {
        pPWR[0] = pPWR[1] = pPWR[2] = 0x40;
        pSNR[0] = pSNR[1] = pSNR[2] = 0x40;
      }

    }
  }

  gdk_threads_enter();
  gtk_image_set_from_pixbuf(GTK_IMAGE(pwrimage), pixbufPWR);
  gtk_image_set_from_pixbuf(GTK_IMAGE(snrimage), pixbufSNR);
  gdk_threads_leave();

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
