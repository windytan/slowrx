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
#include "config.h"
#include "fft.h"
#include "gui.h"
#include "listen.h"
#include "modespec.h"
#include "pcm.h"
#include "pic.h"
#include "vis.h"

static const char *fsk_id;

static void onListenerWaiting(void) {
  gdk_threads_enter        ();
  gtk_widget_set_sensitive (gui.grid_vu,      TRUE);
  gtk_widget_set_sensitive (gui.button_abort, FALSE);
  gtk_widget_set_sensitive (gui.button_clear, TRUE);
  gdk_threads_leave        ();
}

static void onListenerReceivedManual(void) {
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(gui.tog_save)))
    saveCurrentPic();
}

static void onListenerReceiveFSK(void) {
  gdk_threads_enter        ();
  gtk_widget_set_sensitive (gui.button_abort, FALSE);

  // Refresh ListenerEnableFSKID and ListenerAutoSlantCorrect while we're here
  ListenerEnableFSKID = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_fsk));
  ListenerAutoSlantCorrect = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui.tog_slant));

  gdk_threads_leave        ();
}

static void onListenerReceivedFSKID(const char *id) {
  gdk_threads_enter  ();
  fsk_id = id;
  gtk_label_set_text (GTK_LABEL(gui.label_fskid), id);
  gdk_threads_leave  ();
}

static void onListenerReceiveStarted(void) {
  static char rctime[8];

  strftime(rctime,  sizeof(rctime)-1, "%H:%Mz", ListenerReceiveStartTime);
  gdk_threads_enter        ();
  gtk_label_set_text       (GTK_LABEL(gui.label_fskid), "");
  gtk_widget_set_sensitive (gui.frame_manual, FALSE);
  gtk_widget_set_sensitive (gui.frame_slant,  FALSE);
  gtk_widget_set_sensitive (gui.combo_card,   FALSE);
  gtk_widget_set_sensitive (gui.button_abort, TRUE);
  gtk_widget_set_sensitive (gui.button_clear, FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gui.tog_setedge), FALSE);
  gtk_label_set_markup     (GTK_LABEL(gui.label_lastmode), ModeSpec[CurrentPic.Mode].Name);
  gtk_label_set_markup     (GTK_LABEL(gui.label_utc), rctime);
  gdk_threads_leave        ();
}

static void onListenerAutoSlantCorrect(void) {
  gdk_threads_enter        ();
  gtk_widget_set_sensitive (gui.grid_vu, FALSE);
  gdk_threads_leave        ();
}

static void onListenerReceiveFinished(void) {
  GtkTreeIter iter;

  // Add thumbnail to iconview
  CurrentPic.thumbbuf = gdk_pixbuf_scale_simple (pixbuf_rx, 100,
      100.0/ModeSpec[CurrentPic.Mode].ImgWidth * ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight, GDK_INTERP_HYPER);
  gdk_threads_enter                  ();
  gtk_list_store_prepend             (savedstore, &iter);
  gtk_list_store_set                 (savedstore, &iter, 0, CurrentPic.thumbbuf, 1, fsk_id, -1);
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

/*
 * main
 */

int main(int argc, char *argv[]) {
  GString     *confpath;

  gtk_init (&argc, &argv);

  gdk_threads_init ();

  // Load config
  load_config_settings(&confpath);

  // Prepare FFT
  if (fft_init() < 0) {
    exit(EXIT_FAILURE);
  }

  createGUI();
  OnListenerStatusChange = showStatusbarMessage;
  OnListenerWaiting = onListenerWaiting;
  OnListenerReceivedManual = onListenerReceivedManual;
  OnListenerReceiveStarted = onListenerReceiveStarted;
  OnListenerReceiveFSK = onListenerReceiveFSK;
  OnListenerAutoSlantCorrect = onListenerAutoSlantCorrect;
  OnListenerReceiveFinished = onListenerReceiveFinished;
  OnListenerReceivedFSKID = onListenerReceivedFSKID;
  OnVisStatusChange = showStatusbarMessage;
  pcm.OnPCMAbort = showPCMError;
  pcm.OnPCMDrop = showPCMDropWarning;
  populateDeviceList();

  gtk_main();

  // Save config on exit
  save_config_settings(confpath);

  g_object_unref(pixbuf_rx);
  free(StoredLum);
  fftw_free(fft.in);
  fftw_free(fft.out);

  return (EXIT_SUCCESS);
}
