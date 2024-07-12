#ifndef _PCM_H
#define _PCM_H

#include <alsa/asoundlib.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct _PcmData PcmData;
struct _PcmData {
  snd_pcm_t *handle;
  int16_t   *Buffer;
  TextStatusCallback OnPCMAbort;
  EventCallback OnPCMDrop;
  void       (*PCMReadCallback)(int32_t numsamples, const int16_t* samples);
  int32_t    WindowPtr;
  uint16_t   SampleRate;
  _Bool      BufferDrop;
};
extern PcmData pcm;

#define PCM_RES_SUCCESS		(0)
#define PCM_RES_SUBOPTIMAL	(-1)
#define PCM_RES_FAILURE		(-2)

/* Compute the number of frames for the given time in msec, rounded to the nearest frame */
#define PCM_MS_FRAMES(ms)	(((((uint32_t)(ms)) * pcm.SampleRate) + 500) / 1000)

int32_t  initPcmDevice (const char *wanteddevname, uint16_t samplerate);
void     readPcm       (int32_t numsamples);

#endif
