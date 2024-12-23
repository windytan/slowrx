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

#include <pthread.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>

#include "common.h"
#include "fsk.h"
#include "listen.h"
#include "modespec.h"
#include "pcm.h"
#include "pic.h"
#include "sync.h"
#include "video.h"
#include "vis.h"

static pthread_t listener_thread;
TextStatusCallback OnListenerStatusChange;
EventCallback OnListenerWaiting;
EventCallback OnListenerReceivedManual;
EventCallback OnListenerReceiveStarted;
EventCallback OnListenerReceiveFSK;
EventCallback OnListenerAutoSlantCorrect;
EventCallback OnListenerReceiveFinished;
TextStatusCallback OnListenerReceivedFSKID;
_Bool ListenerAutoSlantCorrect;
_Bool ListenerEnableFSKID;
struct tm *ListenerReceiveStartTime = NULL;

void StartListener(void) {
  pthread_create(&listener_thread, NULL, Listen, NULL);
}

void WaitForListenerStop(void) {
  pthread_join(listener_thread, NULL);
}

// The thread that listens to VIS headers and calls decoders etc
void *Listen() {

  uint8_t     Mode=0;
  time_t      timet;
  _Bool       Finished;
  char        id[20];

  while (true) {
    if (OnListenerWaiting) {
      OnListenerWaiting();
    }

    pcm.WindowPtr = 0;
    snd_pcm_prepare(pcm.handle);
    snd_pcm_start  (pcm.handle);
    Abort = false;

    do {

      // Wait for VIS
      Mode = GetVIS();

      // Stop listening on ALSA error
      if (Abort) pthread_exit(NULL);

      // If manual resync was requested, redraw image
      if (ManualResync) {
        ManualResync = false;
        snd_pcm_drop(pcm.handle);
        printf("getvideo at %.2f skip %d\n",CurrentPic.Rate,CurrentPic.Skip);
        GetVideo(CurrentPic.Mode, CurrentPic.Rate, CurrentPic.Skip, true);
        if (OnListenerReceivedManual) {
          OnListenerReceivedManual();
        }
        pcm.WindowPtr = 0;
        snd_pcm_prepare(pcm.handle);
        snd_pcm_start  (pcm.handle);
      }

    } while (Mode == 0);

    // Start reception
    
    CurrentPic.Rate = (double)pcm.SampleRate;
    CurrentPic.Mode = Mode;

    printf("  ==== %s ====\n", ModeSpec[CurrentPic.Mode].Name);

    // Store time of reception
    timet = time(NULL);
    ListenerReceiveStartTime = gmtime(&timet);
    strftime(CurrentPic.timestr, sizeof(CurrentPic.timestr)-1,"%Y%m%d-%H%M%Sz", ListenerReceiveStartTime);
    

    // Allocate space for cached Lum
    free(StoredLum);
    StoredLum = calloc( (int)((ModeSpec[CurrentPic.Mode].LineTime * ModeSpec[CurrentPic.Mode].NumLines + 1) * pcm.SampleRate), sizeof(uint8_t));
    if (StoredLum == NULL) {
      perror("Listen: Unable to allocate memory for Lum");
      exit(EXIT_FAILURE);
    }

    // Allocate space for sync signal
    HasSync = calloc((int)(ModeSpec[CurrentPic.Mode].LineTime * ModeSpec[CurrentPic.Mode].NumLines / (13.0/pcm.SampleRate) +1), sizeof(_Bool));
    if (HasSync == NULL) {
      perror("Listen: Unable to allocate memory for sync signal");
      exit(EXIT_FAILURE);
    }
  
    // Get video
    if (OnListenerStatusChange) {
      OnListenerStatusChange("Receiving video...");
    }
    if (OnListenerReceiveStarted) {
      OnListenerReceiveStarted();
    }
    printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", (double)pcm.SampleRate, 0, CurrentPic.HedrShift);

    Finished = GetVideo(CurrentPic.Mode, pcm.SampleRate, 0, false);

    if (OnListenerReceiveFSK) {
      OnListenerReceiveFSK();
    }

    id[0] = '\0';

    if (Finished && ListenerEnableFSKID && OnListenerReceivedFSKID) {
      if (OnListenerStatusChange) {
        OnListenerStatusChange("Receiving FSK ID...");
      }
      GetFSK(id, sizeof(id));
      printf("  FSKID \"%s\"\n",id);
      OnListenerReceivedFSKID(id);
    }

    snd_pcm_drop(pcm.handle);

    if (Finished && ListenerAutoSlantCorrect) {

      // Fix slant
      //setVU(0,6);
      if (OnListenerStatusChange) {
        OnListenerStatusChange("Calculating slant...");
      }
      if (OnListenerAutoSlantCorrect) {
        OnListenerAutoSlantCorrect();
      }
      printf("  FindSync @ %.1f Hz\n",CurrentPic.Rate);
      CurrentPic.Rate = FindSync(CurrentPic.Mode, CurrentPic.Rate, &CurrentPic.Skip);
   
      // Final image  
      printf("  getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", CurrentPic.Rate, CurrentPic.Skip, CurrentPic.HedrShift);
      GetVideo(CurrentPic.Mode, CurrentPic.Rate, CurrentPic.Skip, true);
    }

    free (HasSync);
    HasSync = NULL;

    if (OnListenerReceiveFinished) {
      OnListenerReceiveFinished();
    }
    
  }
}
