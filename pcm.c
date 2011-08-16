#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <alsa/asoundlib.h>
#include <fftw3.h>

#include "common.h"

/*
 * Stuff related to sound card capture
 *
 */

// Append fresh PCM data to buffer
void readPcm(gint numsamples) {

  int    samplesread, i;
  gint16 tmp[BUFLEN];

  samplesread = snd_pcm_readi(pcm_handle, tmp, (PcmPointer == 0 ? BUFLEN : numsamples));

  if (samplesread < numsamples) {
    if      (samplesread == -EPIPE)    printf("ALSA: buffer overrun\n");
    else if (samplesread == -EBADFD)   printf("ALSA: PCM is not in the right state\n");
    else if (samplesread == -ESTRPIPE) printf("ALSA: a suspend event occurred\n");
    else if (samplesread < 0)          printf("ALSA error %d\n", samplesread);
    else                               printf("Can't read %d samples\n", numsamples);
    exit(EXIT_FAILURE);
  }

  if (PcmPointer == 0) {
    // Fill buffer on first run
    memcpy(PcmBuffer, tmp, BUFLEN);
    PcmPointer = BUFLEN/2;
  } else {
    for (i=0; i<BUFLEN-numsamples; i++) PcmBuffer[i] = PcmBuffer[i+numsamples];
    for (i=0; i<numsamples;        i++) PcmBuffer[BUFLEN-numsamples+i] = tmp[i];

    PcmPointer -= numsamples;
  }

  // Keep track of max power for VU meter
  //if (abs(PcmBuffer[i]) > MaxPcm) MaxPcm = abs(PcmBuffer[i]);

}

// Initialize sound card
void initPcmDevice() {

  int                  card;
  char                *cardname;
  int                  cardnum, numcards;
  snd_pcm_stream_t     PcmInStream = SND_PCM_STREAM_CAPTURE;
  snd_pcm_hw_params_t *hwparams;
  char                 pcm_name[30];
  unsigned int         exact_rate = 44100;

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

  // Select sound card

  if (numcards == 0) {
    printf("No sound cards found!\n");
    exit(EXIT_SUCCESS);
  } else {
    cardnum = 0;
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(cardcombo), 0);

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

  if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    fprintf(stderr, "ALSA: Error setting interleaved access.\n");
    exit(EXIT_FAILURE);
  }
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    fprintf(stderr, "ALSA: Error setting format S16_LE.\n");
    exit(EXIT_FAILURE);
  }
  if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0) < 0) {
    fprintf(stderr, "ALSA: Error setting sample rate.\n");
    exit(EXIT_FAILURE);
  }
  if (exact_rate != 44100) fprintf(stderr, "ALSA: Using %d Hz instead of 44100.\n", exact_rate);

  if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1) < 0) {
    fprintf(stderr, "ALSA: Can't capture mono.\n");
    exit(EXIT_FAILURE);
  }
  if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
    fprintf(stderr, "ALSA: Error setting HW params.\n");
    exit(EXIT_FAILURE);
  }

  PcmBuffer = calloc( BUFLEN, sizeof(gint16));

}
