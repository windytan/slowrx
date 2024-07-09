#ifndef _PCM_H
#define _PCM_H

typedef struct _PcmData PcmData;
struct _PcmData {
  snd_pcm_t *handle;
  gint16    *Buffer;
  void	   (*OnPCMAbort)(const char* reason);
  void	   (*OnPCMDrop)(void);
  int        WindowPtr;
  gboolean   BufferDrop;
};
extern PcmData pcm;

int      initPcmDevice ();
void     readPcm       (gint numsamples);

#endif
