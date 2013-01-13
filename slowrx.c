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
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"

void ensure_dir_exists(const char *dir) {
  struct stat buf;

  int i = stat(dir, &buf);
  if (i != 0) {
    if (mkdir(dir, 0777) != 0) {
      perror("Unable to create directory for output file");
      exit(EXIT_FAILURE);
    }
  }
}

void *Listen() {

  int         Skip = 0;
  char        timestr[40], rctime[8];
  GString    *pngfilename;

  guchar      Mode=0;
  double      Rate;
  struct tm  *timeptr = NULL;
  time_t      timet;
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
    gtk_widget_set_sensitive (gui.grid_vu,      true);
    gtk_widget_set_sensitive (gui.button_abort, false);
    gtk_widget_set_sensitive (gui.button_clear, true);
    gdk_threads_leave        ();

    HedrShift  = 0;
    PcmPointer = 0;
    Rate       = 44100;
    snd_pcm_prepare(pcm_handle);
    snd_pcm_start  (pcm_handle);
    Abort      = false;

    do {

      // Wait for VIS
      Mode = GetVIS();

      // Stop listening on ALSA error
      if (Abort) pthread_exit(NULL);

    } while (Mode == 0);

    printf("  ==== %s ====\n", ModeSpec[Mode].Name);

    // Store time of reception
    timet = time(NULL);
    timeptr = gmtime(&timet);
    strftime(timestr, sizeof(timestr)-1,"%Y%m%d-%H%M%Sz", timeptr);
    

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
    strftime(rctime,  sizeof(rctime)-1, "%H:%Mz", timeptr);
    gdk_threads_enter        ();
    gtk_label_set_text       (GTK_LABEL(gui.label_fskid), "");
    gtk_widget_set_sensitive (gui.frame_manual, false);
    gtk_widget_set_sensitive (gui.combo_card,   false);
    gtk_widget_set_sensitive (gui.button_abort, true);
    gtk_widget_set_sensitive (gui.button_clear, false);
    gtk_statusbar_push       (GTK_STATUSBAR(gui.statusbar), 0, "Receiving video..." );
    gtk_label_set_markup     (GTK_LABEL(gui.label_lastmode), ModeSpec[Mode].Name);
    gtk_label_set_markup     (GTK_LABEL(gui.label_utc), rctime);
    gdk_threads_leave        ();
    printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", 44100.0, 0, HedrShift);

    Finished = GetVideo(Mode, 44100, 0, false);

    gdk_threads_enter        ();
    gtk_widget_set_sensitive (gui.button_abort, false);
    gtk_widget_set_sensitive (gui.frame_manual, true);
    gtk_widget_set_sensitive (gui.combo_card,   true);
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

    snd_pcm_drop(pcm_handle);

    if (Finished && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_slant))) {

      // Fix slant
      setVU(0,-100);
      gdk_threads_enter        ();
      gtk_statusbar_push       (GTK_STATUSBAR(gui.statusbar), 0, "Calculating slant..." );
      gtk_widget_set_sensitive (gui.grid_vu, false);
      gdk_threads_leave        ();
      printf("  FindSync @ %.1f Hz\n",Rate);
      Rate = FindSync(Mode, Rate, &Skip);
   
      // Final image  
      gdk_threads_enter  ();
      gtk_statusbar_push (GTK_STATUSBAR(gui.statusbar), 0, "Redrawing..." );
      gdk_threads_leave  ();
      printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", Rate, Skip, HedrShift);
      GetVideo(Mode, Rate, Skip, true);
    }

    free (HasSync);
    HasSync = NULL;

    // Add thumbnail to iconview
    thumbbuf = gdk_pixbuf_scale_simple (RxPixbuf, 100,
        100.0/ModeSpec[Mode].ImgWidth * ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_HYPER);
    gdk_threads_enter                  ();
    gtk_list_store_prepend             (savedstore, &iter);
    gtk_list_store_set                 (savedstore, &iter, 0, thumbbuf, 1, id, -1);
    gdk_threads_leave                  ();
      
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_save))) {
    
      pngfilename = g_string_new(g_key_file_get_string(keyfile,"slowrx","rxdir",NULL));
      g_string_append_printf(pngfilename, "/%s_%s.png", timestr, ModeSpec[Mode].ShortName);
      printf("  Saving to %s\n", pngfilename->str);

      gdk_threads_enter  ();
      gtk_statusbar_push (GTK_STATUSBAR(gui.statusbar), 0, "Saving..." );
      gdk_threads_leave  ();

      setVU(0,-100);

      /*ensure_dir_exists("rx-lum");
      LumFile = fopen(lumfilename,"w");
      if (LumFile == NULL)
        perror("Unable to open luma file for writing");
      fwrite(StoredLum,1,(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * 44100,LumFile);
      fclose(LumFile);*/

      // Save the received image as PNG
      GdkPixbuf *scaledpb;
      scaledpb = gdk_pixbuf_scale_simple (RxPixbuf, ModeSpec[Mode].ImgWidth,
          ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, GDK_INTERP_HYPER);

      ensure_dir_exists(g_key_file_get_string(keyfile,"slowrx","rxdir",NULL));
      gdk_pixbuf_savev(scaledpb, pngfilename->str, "png", NULL, NULL, NULL);
      g_object_unref(scaledpb);
      g_string_free(pngfilename, true);
    }
    
    free(StoredLum);
    StoredLum = NULL;
    
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

  keyfile = g_key_file_new();
  if (g_key_file_load_from_file(keyfile, confpath->str, G_KEY_FILE_KEEP_COMMENTS, NULL)) {

  } else {
    printf("No valid config file found\n");
    g_key_file_load_from_data(keyfile, "[slowrx]\ndevice=default", -1, G_KEY_FILE_NONE, NULL);
  }

  createGUI();
  populateDeviceList();

  gtk_main();

  // Save config on exit
  ConfFile = fopen(confpath->str,"w");
  if (ConfFile == NULL) {
    perror("Unable to open config file for writing");
  }
  confdata = g_key_file_to_data(keyfile,keylen,NULL);
  fprintf(ConfFile,"%s",confdata);
  fwrite(confdata,1,(size_t)keylen,ConfFile);
  fclose(ConfFile);

  g_object_unref(RxPixbuf);
  free(StoredLum);

  return (EXIT_SUCCESS);
}
