/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "fsk.h"
#include "gui.h"
#include "modespec.h"
#include "pcm.h"
#include "video.h"

// The thread that listens to VIS headers and calls decoders etc
void *Listen() {

  char        rctime[8];

  guchar      Mode=0;
  struct tm  *timeptr = NULL;
  time_t      timet;
  gboolean    Finished;
  char        id[20];
  GtkTreeIter iter;

  while (TRUE) {

    gdk_threads_enter        ();
    gtk_widget_set_sensitive (gui.grid_vu,      TRUE);
    gtk_widget_set_sensitive (gui.button_abort, FALSE);
    gtk_widget_set_sensitive (gui.button_clear, TRUE);
    gdk_threads_leave        ();

    pcm.WindowPtr = 0;
    snd_pcm_prepare(pcm.handle);
    snd_pcm_start  (pcm.handle);
    Abort = FALSE;

    do {

      // Wait for VIS
      Mode = GetVIS();

      // Stop listening on ALSA error
      if (Abort) pthread_exit(NULL);

      // If manual resync was requested, redraw image
      if (ManualResync) {
        ManualResync = FALSE;
        snd_pcm_drop(pcm.handle);
        printf("getvideo at %.2f skip %d\n",CurrentPic.Rate,CurrentPic.Skip);
        GetVideo(CurrentPic.Mode, CurrentPic.Rate, CurrentPic.Skip, TRUE);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_save)))
          saveCurrentPic();
        pcm.WindowPtr = 0;
        snd_pcm_prepare(pcm.handle);
        snd_pcm_start  (pcm.handle);
      }

    } while (Mode == 0);

    // Start reception
    
    CurrentPic.Rate = 44100;
    CurrentPic.Mode = Mode;

    printf("  ==== %s ====\n", ModeSpec[CurrentPic.Mode].Name);

    // Store time of reception
    timet = time(NULL);
    timeptr = gmtime(&timet);
    strftime(CurrentPic.timestr, sizeof(CurrentPic.timestr)-1,"%Y%m%d-%H%M%Sz", timeptr);
    

    // Allocate space for cached Lum
    free(StoredLum);
    StoredLum = calloc( (int)((ModeSpec[CurrentPic.Mode].LineTime * ModeSpec[CurrentPic.Mode].NumLines + 1) * 44100), sizeof(guchar));
    if (StoredLum == NULL) {
      perror("Listen: Unable to allocate memory for Lum");
      exit(EXIT_FAILURE);
    }

    // Allocate space for sync signal
    HasSync = calloc((int)(ModeSpec[CurrentPic.Mode].LineTime * ModeSpec[CurrentPic.Mode].NumLines / (13.0/44100) +1), sizeof(gboolean));
    if (HasSync == NULL) {
      perror("Listen: Unable to allocate memory for sync signal");
      exit(EXIT_FAILURE);
    }
  
    // Get video
    strftime(rctime,  sizeof(rctime)-1, "%H:%Mz", timeptr);
    gdk_threads_enter        ();
    gtk_label_set_text       (GTK_LABEL(gui.label_fskid), "");
    gtk_widget_set_sensitive (gui.frame_manual, FALSE);
    gtk_widget_set_sensitive (gui.frame_slant,  FALSE);
    gtk_widget_set_sensitive (gui.combo_card,   FALSE);
    gtk_widget_set_sensitive (gui.button_abort, TRUE);
    gtk_widget_set_sensitive (gui.button_clear, FALSE);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui.tog_setedge), FALSE);
    gtk_statusbar_push       (GTK_STATUSBAR(gui.statusbar), 0, "Receiving video..." );
    gtk_label_set_markup     (GTK_LABEL(gui.label_lastmode), ModeSpec[CurrentPic.Mode].Name);
    gtk_label_set_markup     (GTK_LABEL(gui.label_utc), rctime);
    gdk_threads_leave        ();
    printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", 44100.0, 0, CurrentPic.HedrShift);

    Finished = GetVideo(CurrentPic.Mode, 44100, 0, FALSE);

    gdk_threads_enter        ();
    gtk_widget_set_sensitive (gui.button_abort, FALSE);
    gdk_threads_leave        ();
    
    id[0] = '\0';

    if (Finished && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_fsk))) {
      gdk_threads_enter  ();
      gtk_statusbar_push (GTK_STATUSBAR(gui.statusbar), 0, "Receiving FSK ID..." );
      gdk_threads_leave  ();
      GetFSK(id);
      printf("  FSKID \"%s\"\n",id);
      gdk_threads_enter  ();
      gtk_label_set_text (GTK_LABEL(gui.label_fskid), id);
      gdk_threads_leave  ();
    }

    snd_pcm_drop(pcm.handle);

    if (Finished && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_slant))) {

      // Fix slant
      //setVU(0,6);
      gdk_threads_enter        ();
      gtk_statusbar_push       (GTK_STATUSBAR(gui.statusbar), 0, "Calculating slant..." );
      gtk_widget_set_sensitive (gui.grid_vu, FALSE);
      gdk_threads_leave        ();
      printf("  FindSync @ %.1f Hz\n",CurrentPic.Rate);
      CurrentPic.Rate = FindSync(CurrentPic.Mode, CurrentPic.Rate, &CurrentPic.Skip);
   
      // Final image  
      printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", CurrentPic.Rate, CurrentPic.Skip, CurrentPic.HedrShift);
      GetVideo(CurrentPic.Mode, CurrentPic.Rate, CurrentPic.Skip, TRUE);
    }

    free (HasSync);
    HasSync = NULL;

    // Add thumbnail to iconview
    CurrentPic.thumbbuf = gdk_pixbuf_scale_simple (pixbuf_rx, 100,
        100.0/ModeSpec[CurrentPic.Mode].ImgWidth * ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight, GDK_INTERP_HYPER);
    gdk_threads_enter                  ();
    gtk_list_store_prepend             (savedstore, &iter);
    gtk_list_store_set                 (savedstore, &iter, 0, CurrentPic.thumbbuf, 1, id, -1);
    gdk_threads_leave                  ();
    
    // Save PNG
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_save))) {
    
      //setVU(0,6);

      /*ensure_dir_exists("rx-lum");
      LumFile = fopen(lumfilename,"w");
      if (LumFile == NULL)
        perror("Unable to open luma file for writing");
      fwrite(StoredLum,1,(ModeSpec[Mode].LineTime * ModeSpec[Mode].NumLines) * 44100,LumFile);
      fclose(LumFile);*/

      saveCurrentPic();
    }
       
    gdk_threads_enter        ();
    gtk_widget_set_sensitive (gui.frame_slant,  TRUE);
    gtk_widget_set_sensitive (gui.frame_manual, TRUE);
    gtk_widget_set_sensitive (gui.combo_card,   TRUE);
    gdk_threads_leave        ();
    
  }
}


/*
 * main
 */

int main(int argc, char *argv[]) {

  FILE        *ConfFile;
  const gchar *confdir;
  GString     *confpath;
  gchar       *confdata;
  gsize       *keylen=NULL;

  gtk_init (&argc, &argv);

  gdk_threads_init ();

  // Load config
  confdir  = g_get_user_config_dir();
  confpath = g_string_new(confdir);
  g_string_append(confpath, "/slowrx.ini");

  config = g_key_file_new();
  if (g_key_file_load_from_file(config, confpath->str, G_KEY_FILE_KEEP_COMMENTS, NULL)) {

  } else {
    printf("No valid config file found\n");
    g_key_file_load_from_data(config, "[slowrx]\ndevice=default", -1, G_KEY_FILE_NONE, NULL);
  }

  // Prepare FFT
  fft.in = fftw_alloc_real(2048);
  if (fft.in == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    exit(EXIT_FAILURE);
  }
  fft.out = fftw_alloc_complex(2048);
  if (fft.out == NULL) {
    perror("GetVideo: Unable to allocate memory for FFT");
    fftw_free(fft.in);
    exit(EXIT_FAILURE);
  }
  memset(fft.in,  0, sizeof(double) * 2048);

  fft.Plan1024 = fftw_plan_dft_r2c_1d(1024, fft.in, fft.out, FFTW_ESTIMATE);
  fft.Plan2048 = fftw_plan_dft_r2c_1d(2048, fft.in, fft.out, FFTW_ESTIMATE);

  createGUI();
  populateDeviceList();

  gtk_main();

  // Save config on exit
  ConfFile = fopen(confpath->str,"w");
  if (ConfFile == NULL) {
    perror("Unable to open config file for writing");
  } else {
    confdata = g_key_file_to_data(config,keylen,NULL);
    fprintf(ConfFile,"%s",confdata);
    fwrite(confdata,1,(size_t)keylen,ConfFile);
    fclose(ConfFile);
  }

  g_object_unref(pixbuf_rx);
  free(StoredLum);
  fftw_free(fft.in);
  fftw_free(fft.out);

  return (EXIT_SUCCESS);
}
