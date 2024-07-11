#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "pcm.h"

/*
 * Stuff related to sound card capture
 *
 */

PcmData      pcm;

// Capture fresh PCM data to buffer
void readPcm(int32_t numsamples) {

  int32_t samplesread, i;
  int32_t tmp[BUFLEN];  // Holds one or two 16-bit channels, will be ANDed to single channel

  samplesread = snd_pcm_readi(pcm.handle, tmp, (pcm.WindowPtr == 0 ? BUFLEN : numsamples));

  if (samplesread < numsamples) {
    
    if      (samplesread == -EPIPE)
      printf("ALSA: buffer overrun\n");
    else if (samplesread < 0) {
      printf("ALSA error %d (%s)\n", samplesread, snd_strerror(samplesread));
      if (pcm.OnPCMAbort) {
        pcm.OnPCMAbort("ALSA Error");
      }
      Abort = true;
      pthread_exit(NULL);
    }
    else
      printf("Can't read %d samples\n", numsamples);
    
    // On first appearance of error, update the status icon
    if (!pcm.BufferDrop) {
      if (pcm.OnPCMDrop) {
        pcm.OnPCMDrop();
      }
      pcm.BufferDrop = true;
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
//  PCM_RES_SUCCESS = opened ok
//  PCM_RES_SUBOPTIMAL = opened, but suboptimal
//  PCM_RES_FAILURE = couldn't be opened
int32_t initPcmDevice(const char *wanteddevname) {

  snd_pcm_hw_params_t *hwparams;
  char                 pcm_name[30];
  unsigned int         exact_rate = 44100;
  int                  card;
  _Bool                found;
  char                *cardname;

  pcm.BufferDrop = false;

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
    return(PCM_RES_FAILURE);
  }

  if (strcmp(wanteddevname,"default") == 0) {
    sprintf(pcm_name,"default");
  } else {
    sprintf(pcm_name,"hw:%d",card);
  }
  
  if (snd_pcm_open(&pcm.handle, pcm_name, SND_PCM_STREAM_CAPTURE, 0) < 0) {
    perror("ALSA: Error opening PCM device");
    return(PCM_RES_FAILURE);
  }

  /* Init hwparams with full configuration space */
  if (snd_pcm_hw_params_any(pcm.handle, hwparams) < 0) {
    perror("ALSA: Can not configure this PCM device.");
    return(PCM_RES_FAILURE);
  }

  if (snd_pcm_hw_params_set_access(pcm.handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    perror("ALSA: Error setting interleaved access.");
    return(PCM_RES_FAILURE);
  }
  if (snd_pcm_hw_params_set_format(pcm.handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    perror("ALSA: Error setting format S16_LE.");
    return(PCM_RES_FAILURE);
  }
  if (snd_pcm_hw_params_set_rate_near(pcm.handle, hwparams, &exact_rate, 0) < 0) {
    perror("ALSA: Error setting sample rate.");
    return(PCM_RES_FAILURE);
  }

  // Try stereo first
  if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 2) < 0) {
    // Fall back to mono
    if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 1) < 0) {
      perror("ALSA: Error setting channels.");
      return(PCM_RES_FAILURE);
    }
  }
  if (snd_pcm_hw_params(pcm.handle, hwparams) < 0) {
    perror("ALSA: Error setting HW params.");
    return(PCM_RES_FAILURE);
  }

  pcm.Buffer = calloc( BUFLEN, sizeof(int16_t));
  memset(pcm.Buffer, 0, BUFLEN);
  
  if (exact_rate != 44100) {
    fprintf(stderr, "ALSA: Got %d Hz instead of 44100. Expect artifacts.\n", exact_rate);
    return(PCM_RES_SUBOPTIMAL);
  }

  return(PCM_RES_SUCCESS);

}
