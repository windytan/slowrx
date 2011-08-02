#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>
#include <alsa/asoundlib.h>

#include "common.h"

int VISmap[128];

FILE      *PcmInStream    = NULL;
short int PcmBuffer[2048] = {0};
double    *PCM            = NULL;
int       PcmPointer      = 0;
int       Sample          = 0;
int       UseWav          = FALSE;
guchar    *rgbbuf         = NULL;
int       maxpwr          = 0;
int       minpwr          = 0;
unsigned int       SRate           = 44100;
double    PowerAcc[2048]  = {0};
double    MaxPower[2048]  = {0};
double    *StoredFreq     = NULL;
double    StoredFreqRate  = 0;
double    HedrShift       = 0;

GtkWidget *window         = NULL;
GtkWidget *notebook       = NULL;
GdkPixbuf *RxPixbuf      = NULL;
GdkPixbuf *DispPixbuf     = NULL;
GtkWidget *RxImage       = NULL;
GtkWidget *statusbar      = NULL;
GtkWidget *snrbar         = NULL;
GtkWidget *pwrbar         = NULL;
GdkPixbuf *VUpixbufActive = NULL;
GdkPixbuf *VUpixbufDim    = NULL;
GdkPixbuf *VUpixbufSNR    = NULL;
GtkWidget *VUimage[10]    = {NULL};
GtkWidget *SNRimage[10]   = {NULL};
GtkWidget *vutable        = NULL;
GtkWidget *infolabel      = NULL;

snd_pcm_t *pcm_handle     = NULL;

void ClearPixbuf(GdkPixbuf *pb, unsigned int width, unsigned int height) {

  unsigned int x,y,rowstride;
  guchar *pixels, *p;
  rowstride = gdk_pixbuf_get_rowstride (pb);
  pixels    = gdk_pixbuf_get_pixels(pb);

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      p = pixels + y * rowstride + x * 3;
      p[0] = p[1] = p[2] = 192;
    }
  }

}

// Return the bin index matching the given frequency
unsigned int GetBin (double Freq, int FFTLen) {
  return (Freq / SRATE * FFTLen);
}

// Clip to [0..255]
unsigned char clip (double a) {
  if      (a < 0)   return 0;
  else if (a > 255) return 255;
  return  (unsigned char)round(a);
}

void setVU(short int PcmValue, double SNRdB) {
  int i;
  int dBthresh[10]    = {0, -1, -2, -3, -5, -7, -10, -15, -20, -25};
  int SNRdBthresh[10] = {30, 15, 10, 5, 3, 0, -3, -5, -10, -15};
  int dB = (int)round(10 * log10(PcmValue/32767.0));

  gdk_threads_enter();
  for (i=0; i<10; i++) {
    if (dB >= dBthresh[i]) gtk_image_set_from_pixbuf(GTK_IMAGE(VUimage[i]), VUpixbufActive);
    else                   gtk_image_set_from_pixbuf(GTK_IMAGE(VUimage[i]), VUpixbufDim);

    if (SNRdB >= SNRdBthresh[i]) gtk_image_set_from_pixbuf(GTK_IMAGE(SNRimage[i]), VUpixbufSNR);
    else                         gtk_image_set_from_pixbuf(GTK_IMAGE(SNRimage[i]), VUpixbufDim);
  }
  gdk_threads_leave();

}

double deg2rad (double Deg) {
    return (Deg / 180) * M_PI;
}
