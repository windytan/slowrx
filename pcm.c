#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <alsa/asoundlib.h>

#include "common.h"


void initPcmDevice() {

  // Select sound card
  
  int card;
  char *cardname;

  int cardnum,numcards;

  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cardcombo), "default");
  numcards = 0;
  card     = -1;
  do {
    snd_card_next(&card);
    if (card != -1) {
      snd_card_get_name(card,&cardname);
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cardcombo), cardname);
      numcards++;
    }
  } while (card != -1);


  if (numcards == 0) {
    printf("No sound cards found!\n");
    exit(EXIT_SUCCESS);
  } else if (numcards == 1) {
    cardnum = 0;
  } else {

    gtk_combo_box_set_active(GTK_COMBO_BOX(cardcombo), 0);
    gtk_widget_show_all (sdialog);

    gint result = gtk_dialog_run(GTK_DIALOG(sdialog));
    if (result == GTK_RESPONSE_CLOSE) exit(EXIT_SUCCESS);
    cardnum = gtk_combo_box_get_active (GTK_COMBO_BOX(cardcombo));
  }

  snd_pcm_stream_t     PcmInStream = SND_PCM_STREAM_CAPTURE;
  snd_pcm_hw_params_t *hwparams;

  gtk_widget_destroy(sdialog);

  char pcm_name[30];
  if (cardnum == 0) {
    sprintf(pcm_name,"default");
  } else {
    sprintf(pcm_name,"hw:%d",cardnum-1);
  }

  snd_pcm_hw_params_alloca(&hwparams);

  if (snd_pcm_open(&pcm_handle, pcm_name, PcmInStream, 0) < 0) {
    fprintf(stderr, "ALSA: Error opening PCM device %s\n", pcm_name);
    exit(EXIT_FAILURE);
  }

  /* Init hwparams with full configuration space */
  if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
    fprintf(stderr, "ALSA: Can not configure this PCM device.\n");
    exit(EXIT_FAILURE);
  }

  unsigned int exact_rate;

  if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    fprintf(stderr, "ALSA: Error setting interleaved access.\n");
    exit(EXIT_FAILURE);
  }
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    fprintf(stderr, "ALSA: Error setting format S16_LE.\n");
    exit(EXIT_FAILURE);
  }

  exact_rate = 44100;
  if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0) < 0) {
    fprintf(stderr, "ALSA: Error setting sample rate.\n");
    exit(EXIT_FAILURE);
  }
  SRate = exact_rate;
  if (exact_rate != 44100) fprintf(stderr, "ALSA: Using %d Hz instead of 44100.\n", exact_rate);

  if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1) < 0) {
    fprintf(stderr, "ALSA: Can't capture mono.\n");
    exit(EXIT_FAILURE);
  }
  if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
    fprintf(stderr, "ALSA: Error setting HW params.\n");
    exit(EXIT_FAILURE);
  }

}
