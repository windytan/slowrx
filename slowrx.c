/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <gtk/gtk.h>
#include <pthread.h>
#include <pnglite.h>

#include <alsa/asoundlib.h>

#include "common.h"


void *Listen() {

  double         Rate = SRATE;
  int            Skip = 0, i=0;
  int            Mode = 0;
  time_t         timet;
  char           dest[40];
  char           pngfilename[40];
  char           lumfilename[40];
  struct tm     *timeptr = NULL;
  char           infostr[60];
  char           rctime[8];
  unsigned char *Lum;
  FILE          *LumFile;

  while (1) {

    // Wait for VIS
    HedrShift = 0;
    gdk_threads_enter();
    gtk_widget_set_sensitive(vugrid, TRUE);
    gdk_threads_leave();

    Mode = GetVIS();
    if (Mode == -1) exit(0);

    printf("  ==== %s ====\n", ModeSpec[Mode].Name);

    timet = time(NULL);
    timeptr = gmtime(&timet);
    strftime(dest, sizeof(dest)-1,"%Y%m%d-%H%M%Sz", timeptr);
    snprintf(pngfilename, sizeof(dest)-1, "rx/%s_%s.png", ModeSpec[Mode].ShortName, dest);
    snprintf(lumfilename, sizeof(dest)-1, "rx/%s_%s-lum", ModeSpec[Mode].ShortName, dest);
    printf("  \"%s\"\n", pngfilename);
    
    // Allocate space for PCM
    PCM = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * SRATE, sizeof(double));
    if (PCM == NULL) {
      perror("Listen: Unable to allocate memory for PCM");
      exit(EXIT_FAILURE);
    }

    // Allocate space for cached FFT
    StoredFreq = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * SRATE, sizeof(double));
    if (StoredFreq == NULL) {
      perror("Listen: Unable to allocate memory for demodulated signal");
      free(PCM);
      exit(EXIT_FAILURE);
    }
  
    // Get video
    strftime(rctime, sizeof(rctime)-1, "%H:%Mz", timeptr);
    snprintf(infostr, sizeof(infostr)-1, "%s @ %+.0f Hz, received at %s", ModeSpec[Mode].Name, HedrShift, rctime);
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Receiving video" );
    gtk_label_set_markup(GTK_LABEL(infolabel), infostr);
    gdk_threads_leave();
    PcmPointer = 2048;
    Sample     = 0;
    Rate       = SRATE;
    Skip       = 0;
    printf("  getvideo @ %.02f Hz, Skip %d, HedrShift %.0f Hz\n", Rate, Skip, HedrShift);

    snd_pcm_start(pcm_handle);
    GetVideo(Mode, Rate, Skip, TRUE, FALSE);
    snd_pcm_drop(pcm_handle);

    // Fix slant
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Calculating slant" );
    gtk_widget_set_sensitive(vugrid, FALSE);
    gdk_threads_leave();
    printf("  FindSync @ %.02f Hz\n",Rate);
    Rate = FindSync(PcmPointer, Mode, Rate, &Skip);
   
    free(PCM);
    PCM = NULL;

    // Final image  
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Redrawing" );
    gdk_threads_leave();
    printf("  getvideo @ %.02f Hz, Skip %d, HedrShift %.0f Hz\n", Rate, Skip, HedrShift);
    GetVideo(Mode, Rate, Skip, TRUE, TRUE);
    
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Saving" );
    gdk_threads_leave();

    // Save the raw signal
    Lum = malloc( (ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE );
    if (Lum == NULL) {
      perror("Unable to allocate memory for lum data");
      exit(EXIT_FAILURE);
    }
    for (i=0; i<(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE; i++)
      Lum[i] = clip((StoredFreq[i] - (1500 + HedrShift)) / 3.1372549);

    LumFile = fopen(lumfilename,"w");
    if (LumFile == NULL) {
      perror("Unable to open luma file for writing");
      exit(EXIT_FAILURE);
    }
    fwrite(Lum,1,(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE,LumFile);
    fclose(LumFile);

    // Save the received image as PNG
    png_t png;
    png_init(0,0);

    guchar *pixels;
    pixels = gdk_pixbuf_get_pixels(RxPixbuf);

    png_open_file_write(&png, pngfilename);
    png_set_data(&png, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, 8, PNG_TRUECOLOR, pixels);
    png_close_file(&png);
    
    free(StoredFreq);
    StoredFreq = NULL;
    
  }
}


/*
 * main
 */

int main(int argc, char *argv[]) {

  pthread_t thread1;

  gtk_init (&argc, &argv);

  g_thread_init (NULL);
  gdk_threads_init ();

  createGUI();
  initPcmDevice();

  pthread_create (&thread1, NULL, Listen, NULL);

  gtk_main();

  gdk_pixbuf_unref(RxPixbuf);
  free(PCM);
  free(StoredFreq);

  printf("Clean exit\n");

  return (EXIT_SUCCESS);
}
