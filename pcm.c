#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <gtk/gtk.h>

#include <alsa/asoundlib.h>

#ifdef GPL
#include <fftw3.h>
#endif

#include "common.h"

/*
 * Stuff related to sound card capture
 *
 */

// Capture fresh PCM data to buffer
void readPcm(gint numsamples) {

  int    samplesread, i;
  gint32 tmp[BUFLEN];  // Holds one or two 16-bit channels, will be ANDed to single channel

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
    for (i=0; i<BUFLEN; i++)
      PcmBuffer[i] = tmp[i] & 0xffff;
    PcmPointer = BUFLEN/2;
  } else {
    for (i=0; i<BUFLEN-numsamples; i++) PcmBuffer[i] = PcmBuffer[i+numsamples];
    for (i=0; i<numsamples;        i++) {
      PcmBuffer[BUFLEN-numsamples+i] = tmp[i] & 0xffff;
      // Keep track of max power for VU meter
      if (abs(PcmBuffer[i]) > MaxPcm) MaxPcm = abs(PcmBuffer[i]);
    }

    PcmPointer -= numsamples;
  }

}

void populateDeviceList() {
  int                  card;
  char                *cardname;
  int                  cardnum, numcards, row;

  gdk_threads_enter();
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cardcombo), "default");
  gdk_threads_leave();

  numcards = 0;
  card     = -1;
  row      = 0;
  do {
    snd_card_next(&card);
    if (card != -1) {
      row++;
      snd_card_get_name(card,&cardname);
      gdk_threads_enter();
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cardcombo), cardname);
      if (strcmp(cardname,g_key_file_get_string(keyfile,"slowrx","device",NULL)) == 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(cardcombo), row);

      gdk_threads_leave();
      numcards++;
    }
  } while (card != -1);

  if (numcards == 0) {
    perror("No sound devices found!\n");
    exit(EXIT_FAILURE);
  }
  
}

// Initialize sound card
// Return value:
//   0 = opened ok
//  -1 = opened, but suboptimal
//  -2 = couldn't be opened
int initPcmDevice(char *wanteddevname) {

  snd_pcm_hw_params_t *hwparams;
  char                 pcm_name[30];
  unsigned int         exact_rate = 44100;
  int                  card;
  bool                 found;
  char                *cardname;

  snd_pcm_hw_params_alloca(&hwparams);

  card  = -1;
  found = false;
  if (strcmp(wanteddevname,"default") == 0) {
    found=true;
  } else {
    do {
      snd_card_next(&card);
      if (card != -1) {
        snd_card_get_name(card,&cardname);
        if (strcmp(cardname, wanteddevname) == 0) {
          found=true;
          break;
        }
      }
    } while (card != -1);
  }

  if (!found) {
    perror("Device disconnected?\n");
    return(-2);
  }

  if (strcmp(wanteddevname,"default") == 0) {
    sprintf(pcm_name,"default");
  } else {
    sprintf(pcm_name,"hw:%d",card);
  }
  
  if (snd_pcm_open(&pcm_handle, pcm_name, SND_PCM_STREAM_CAPTURE, 0) < 0) {
    perror("ALSA: Error opening PCM device");
    return(-2);
  }

  /* Init hwparams with full configuration space */
  if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
    perror("ALSA: Can not configure this PCM device.");
    return(-2);
  }

  if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    perror("ALSA: Error setting interleaved access.");
    return(-2);
  }
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    perror("ALSA: Error setting format S16_LE.");
    return(-2);
  }
  if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0) < 0) {
    perror("ALSA: Error setting sample rate.");
    return(-2);
  }

  // Try stereo first
  if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2) < 0) {
    // Fall back to mono
    if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1) < 0) {
      perror("ALSA: Error setting channels.");
      return(-2);
    }
  }
  if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
    perror("ALSA: Error setting HW params.");
    return(-2);
  }

  PcmBuffer = calloc( BUFLEN, sizeof(gint16));
  
  if (exact_rate != 44100) {
    fprintf(stderr, "ALSA: Got %d Hz instead of 44100. Expect artifacts.\n", exact_rate);
    return(-1);
  }

  return(0);

}
