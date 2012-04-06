/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include <gtk/gtk.h>
#include <pnglite.h>
#include <pthread.h>
#include <fftw3.h>

#include <alsa/asoundlib.h>

#include "common.h"


void *Listen() {

  int         Skip = 0;
  char        timestr[40], pngfilename[40], lumfilename[40], infostr[60], rctime[8];
  guchar     *pixels, Mode=0;
  double      Rate;
  struct tm  *timeptr = NULL;
  time_t      timet;
  FILE       *LumFile;
  bool        Finished;
  GdkPixbuf  *thumbbuf;
  char        id[20];
  GtkTreeIter iter;

  /** Prepare FFT **/
  in = fftw_malloc(sizeof(double) * 2048);
  if (in == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }
  out = fftw_malloc(sizeof(double) * 2048);
  if (out == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(in);
    exit(EXIT_FAILURE);
  }
  memset(in,  0, sizeof(double) * 2048);
  memset(out, 0, sizeof(double) * 2048);

  Plan1024 = fftw_plan_r2r_1d(1024, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  Plan2048 = fftw_plan_r2r_1d(2048, in, out, FFTW_FORWARD, FFTW_ESTIMATE);


  while (true) {

    gdk_threads_enter        ();
    gtk_widget_set_sensitive (vugrid,   true);
    gtk_widget_set_sensitive (btnabort, false);
    gdk_threads_leave        ();

    HedrShift  = 0;
    PcmPointer = 0;
    Rate       = 44100;
    snd_pcm_prepare(pcm_handle);
    snd_pcm_start  (pcm_handle);

    // Wait for VIS
    Mode = GetVIS();

    if (Mode == 0) exit(EXIT_FAILURE);

    printf("  ==== %s ====\n", ModeSpec[Mode].Name);

    // Store time of reception
    timet = time(NULL);
    timeptr = gmtime(&timet);
    strftime(timestr, sizeof(timestr)-1,"%Y%m%d-%H%M%Sz", timeptr);
    snprintf(pngfilename, sizeof(timestr)-1, "rx/%s_%s.png", timestr, ModeSpec[Mode].ShortName);
    snprintf(lumfilename, sizeof(timestr)-1, "rx-lum/%s_%s-lum", timestr, ModeSpec[Mode].ShortName);
    printf("  \"%s\"\n", pngfilename);
    

    // Allocate space for cached Lum
    StoredLum = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * 44100, sizeof(guchar));
    if (StoredLum == NULL) {
      perror("Listen: Unable to allocate memory for Lum");
      exit(EXIT_FAILURE);
    }

    // Allocate space for sync signal
    HasSync = calloc((int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight / 1.5e-3 +1), sizeof(bool));
    if (HasSync == NULL) {
      perror("Listen: Unable to allocate memory for sync signal");
      exit(EXIT_FAILURE);
    }
  
    // Get video
    strftime(rctime,  sizeof(rctime)-1, "%H:%M", timeptr);
    snprintf(infostr, sizeof(infostr)-1, "%s, %s UTC", ModeSpec[Mode].Name, rctime);
    gdk_threads_enter        ();
    gtk_label_set_text       (GTK_LABEL(idlabel), "");
    gtk_widget_set_sensitive (manualframe, false);
    gtk_widget_set_sensitive (btnabort,    true);
    gtk_statusbar_push       (GTK_STATUSBAR(statusbar), 0, "Receiving video..." );
    gtk_label_set_markup     (GTK_LABEL(infolabel), infostr);
    gdk_threads_leave        ();
    printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %d Hz\n", 44100.0, 0, HedrShift);

    Finished = GetVideo(Mode, 44100, 0, false);

    gdk_threads_enter        ();
    gtk_widget_set_sensitive (btnabort,    false);
    gtk_widget_set_sensitive (manualframe, true);
    gdk_threads_leave        ();
    
    id[0] = '\0';

    if (Finished && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togfsk))) {
      gdk_threads_enter  ();
      gtk_statusbar_push (GTK_STATUSBAR(statusbar), 0, "Receiving FSK ID..." );
      gdk_threads_leave  ();
      GetFSK(id);
      printf("  FSKID \"%s\"\n",id);
      gdk_threads_enter  ();
      gtk_label_set_text (GTK_LABEL(idlabel), id);
      gdk_threads_leave  ();
    }

    snd_pcm_drop(pcm_handle);

    if (Finished && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togslant))) {

      // Fix slant
      setVU(0,-100);
      gdk_threads_enter        ();
      gtk_statusbar_push       (GTK_STATUSBAR(statusbar), 0, "Calculating slant..." );
      gtk_widget_set_sensitive (vugrid, false);
      gdk_threads_leave        ();
      printf("  FindSync @ %.1f Hz\n",Rate);
      Rate = FindSync(Mode, Rate, &Skip);
   
      // Final image  
      gdk_threads_enter  ();
      gtk_statusbar_push (GTK_STATUSBAR(statusbar), 0, "Redrawing..." );
      gdk_threads_leave  ();
      printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %d Hz\n", Rate, Skip, HedrShift);
      GetVideo(Mode, Rate, Skip, true);
    }

    free (HasSync);
    HasSync = NULL;

    // Add thumbnail to iconview
    thumbbuf = gdk_pixbuf_scale_simple (RxPixbuf, 100, 100.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_HYPER);
    gdk_threads_enter                  ();
    gtk_list_store_prepend             (savedstore, &iter);
    gtk_list_store_set                 (savedstore, &iter, 0, thumbbuf, 1, id, -1);
    gdk_threads_leave                  ();
      
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(togsave))) {

      // Save the raw signal
      gdk_threads_enter  ();
      gtk_statusbar_push (GTK_STATUSBAR(statusbar), 0, "Saving..." );
      gdk_threads_leave  ();

      setVU(0,-100);

      LumFile = fopen(lumfilename,"w");
      if (LumFile == NULL) {
        perror("Unable to open luma file for writing");
        exit(EXIT_FAILURE);
      }
      fwrite(StoredLum,1,(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * 44100,LumFile);
      fclose(LumFile);

      // Save the received image as PNG
      png_t png;
      png_init(0,0);

      GdkPixbuf *scaledpb;
      scaledpb = gdk_pixbuf_scale_simple (RxPixbuf, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_HYPER);
      pixels = gdk_pixbuf_get_pixels(scaledpb);

      png_open_file_write(&png, pngfilename);
      png_set_data(&png, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight *  ModeSpec[Mode].YScale, 8, PNG_TRUECOLOR, pixels);
      png_close_file(&png);
      g_object_unref(scaledpb);
    }
    
    free(StoredLum);
    StoredLum = NULL;
    
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

  g_object_unref(RxPixbuf);
  free(StoredLum);

  printf("Clean exit\n");

  return (EXIT_SUCCESS);
}
