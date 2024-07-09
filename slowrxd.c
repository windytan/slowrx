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
#include "fft.h"
#include "listen.h"
#include "modespec.h"
#include "pcm.h"
#include "pic.h"
#include "video.h"
#include "vis.h"

static const char *fsk_id;

static void showStatusbarMessage(const char* msg) {
  printf("Status: %s\n", msg);
}

static void onVisIdentified(void) {
  int idx = VISmap[VIS];
  printf("Detected mode %s (%d)", ModeSpec[idx].Name, VIS);
}

static void onVideoInitBuffer(uint8_t mode) {
  printf("Init buffer for mode %d", mode);
}

static void onVideoStartRedraw(void) {
  printf("\n\nBEGIN REDRAW\n\n");
}

static void onVideoWritePixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
  printf("(%d, %d) = #%02x%02x%02x\n", x, y, r, g, b);
}

static void onVideoRefresh(void) {
  printf("\n\nREFRESH\n\n");
}

static void onListenerWaiting(void) {
  printf("Listener is waiting");
}

static void onListenerReceivedManual(void) {
  printf("Listener received something in manual mode");
}

static void onListenerReceiveFSK(void) {
  printf("Listener is now receiving FSK");
}

static void onListenerReceivedFSKID(const char *id) {
  printf("Listener got FSK %s", id);
  fsk_id = id;
}

static void onListenerReceiveStarted(void) {
  static char rctime[8];

  strftime(rctime,  sizeof(rctime)-1, "%H:%Mz", ListenerReceiveStartTime);
  printf("Receive started at %s", rctime);
}

static void onListenerAutoSlantCorrect(void) {
  printf("Starting slant correction");
}

static void onListenerReceiveFinished(void) {
  printf("Received %d × %d pixel image\n",
      ModeSpec[CurrentPic.Mode].ImgWidth,
      ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight);
}

static void showVU(double *Power, int FFTLen, int WinIdx) {
  printf("VU Power data: WinIdx=%d\n", WinIdx);
#if 0
  for (int i = 0; i < FFTLen; i++) {
    printf("[%4d] = %f\n", i, Power[i]);
  }
#else
  (void)Power;
  (void)FFTLen;
#endif
}

static void showPCMError(const char* error) {
  printf("\n\nPCM Error: %s\n\n", error);
}

static void showPCMDropWarning(void) {
  printf("\n\nPCM DROP Warning!!!\n\n");
}

/*
 * main
 */

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Prepare FFT
  if (fft_init() < 0) {
    exit(0);
  }

  if (initPcmDevice("default") < 0) {
    exit(1);
  }

  ListenerAutoSlantCorrect = true;
  ListenerEnableFSKID = true;
  VisAutoStart = true;

  OnVideoInitBuffer = onVideoInitBuffer;
  OnVideoStartRedraw = onVideoStartRedraw;
  OnVideoRefresh = onVideoRefresh;
  OnVideoWritePixel = onVideoWritePixel;
  OnVideoPowerCalculated = showVU;
  OnVisIdentified = onVisIdentified;
  OnVisPowerComputed = showVU;
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

  StartListener();
  WaitForListenerStop();

  return (0);
}
