#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "config.h"
#include "pcm.h"

/*
 * Stuff related to sound card capture
 *
 */

PcmData      pcm;

// Capture fresh PCM data to buffer
void readPcm(gint numsamples) {

  int    samplesread, i;
  gint32 tmp[BUFLEN];  // Holds one or two 16-bit channels, will be ANDed to single channel

  samplesread = snd_pcm_readi(pcm.handle, tmp, (pcm.WindowPtr == 0 ? BUFLEN : numsamples));

  if (samplesread < numsamples) {
    
    if      (samplesread == -EPIPE)
      printf("ALSA: buffer overrun\n");
    else if (samplesread < 0) {
      printf("ALSA error %d (%s)\n", samplesread, snd_strerror(samplesread));
      if (pcm.OnPCMAbort) {
        pcm.OnPCMAbort("ALSA Error");
      }
      Abort = TRUE;
      pthread_exit(NULL);
    }
    else
      printf("Can't read %d samples\n", numsamples);
    
    // On first appearance of error, update the status icon
    if (!pcm.BufferDrop) {
      if (pcm.OnPCMDrop) {
        pcm.OnPCMDrop();
      }
      pcm.BufferDrop = TRUE;
    }

  }

  if (pcm.WindowPtr == 0) {
    // Fill buffer on first run
    for (i=0; i<BUFLEN; i++)
      pcm.Buffer[i] = tmp[i] & 0xffff;
    pcm.WindowPtr = BUFLEN/2;
  } else {

    // Move buffer and push samples
    for (i=0; i<BUFLEN-numsamples;      i++) pcm.Buffer[i] = pcm.Buffer[i+numsamples];
    for (i=BUFLEN-numsamples; i<BUFLEN; i++) pcm.Buffer[i] = tmp[i-(BUFLEN-numsamples)] & 0xffff;

    pcm.WindowPtr -= numsamples;
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
  gboolean             found;
  char                *cardname;

  pcm.BufferDrop = FALSE;

  snd_pcm_hw_params_alloca(&hwparams);

  card  = -1;
  found = FALSE;
  if (strcmp(wanteddevname,"default") == 0) {
    found=TRUE;
  } else {
    do {
      snd_card_next(&card);
      if (card != -1) {
        snd_card_get_name(card,&cardname);
        if (strcmp(cardname, wanteddevname) == 0) {
          found=TRUE;
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
  
  if (snd_pcm_open(&pcm.handle, pcm_name, SND_PCM_STREAM_CAPTURE, 0) < 0) {
    perror("ALSA: Error opening PCM device");
    return(-2);
  }

  /* Init hwparams with full configuration space */
  if (snd_pcm_hw_params_any(pcm.handle, hwparams) < 0) {
    perror("ALSA: Can not configure this PCM device.");
    return(-2);
  }

  if (snd_pcm_hw_params_set_access(pcm.handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    perror("ALSA: Error setting interleaved access.");
    return(-2);
  }
  if (snd_pcm_hw_params_set_format(pcm.handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    perror("ALSA: Error setting format S16_LE.");
    return(-2);
  }
  if (snd_pcm_hw_params_set_rate_near(pcm.handle, hwparams, &exact_rate, 0) < 0) {
    perror("ALSA: Error setting sample rate.");
    return(-2);
  }

  // Try stereo first
  if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 2) < 0) {
    // Fall back to mono
    if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 1) < 0) {
      perror("ALSA: Error setting channels.");
      return(-2);
    }
  }
  if (snd_pcm_hw_params(pcm.handle, hwparams) < 0) {
    perror("ALSA: Error setting HW params.");
    return(-2);
  }

  pcm.Buffer = calloc( BUFLEN, sizeof(gint16));
  memset(pcm.Buffer, 0, BUFLEN);
  
  if (exact_rate != 44100) {
    fprintf(stderr, "ALSA: Got %d Hz instead of 44100. Expect artifacts.\n", exact_rate);
    return(-1);
  }

  return(0);

}
