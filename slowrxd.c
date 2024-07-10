/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

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

/* Exit status codes */
#define DAEMON_EXIT_SUCCESS (0)
#define DAEMON_EXIT_INIT_FFT_ERR (1)
#define DAEMON_EXIT_INIT_PCM_ERR (2)

/* Exit status to use when exiting */
static int daemon_exit_status = DAEMON_EXIT_SUCCESS;

/* Common log message types */
const char* logmsg_receive_start = "RECEIVE_START";
const char* logmsg_vis_detect = "VIS_DETECT";
const char* logmsg_sig_strength = "SIG_STRENGTH";
const char* logmsg_image_finish = "IMAGE_FINISHED";
const char* logmsg_fsk_detect = "FSK_DETECT";
const char* logmsg_fsk_received = "FSK_RECEIVED";
const char* logmsg_receive_end = "RECEIVE_END";
const char* logmsg_status = "STATUS";
const char* logmsg_warning = "WARNING";

#if 0
/* The name of the image-in-progress being received */
static const char* path_inprogress_img = "inprogress.png";
#endif

/* The name of the receive log */
static const char* path_inprogress_log = "inprogress.ndjson";

/* The name of the latest receive log */
static const char* path_latest_log = "latest.ndjson";

/* The FSK ID detected after image transmission */
static const char *fsk_id = NULL;

/* Pointer to the receive log (NDJSON format) */
static FILE* rxlog = NULL;

/* Pointer to the raw image data being received */
#if 0
static gdImagePtr rximg;
#endif

/* Rename and symlink a file */
static int renameAndSymlink(const char* existing_path, const char* new_path, const char* symlink_path) {
  int res = rename(existing_path, new_path);
  if (res < 0) {
    perror("Failed to rename receive log");
    return -errno;
  }
  printf("Renamed %s to %s\n", existing_path, new_path);

  if (symlink_path) {
    res = unlink(symlink_path);
    if (res < 0) {
      /* ENOENT is okay */
      if (errno != ENOENT) {
        perror("Failed to remove old 'latest' symlink");
        return -errno;
      }
    }
    printf("Removed old symlink %s\n", symlink_path);

    res = symlink(new_path, symlink_path);
    if (res < 0) {
      perror("Failed to symlink receive log");
      return -errno;
    }
    printf("Symlinked %s to %s\n", new_path, symlink_path);
  }

  return 0;
}

/* Open the receive log ready for traffic */
static int openReceiveLog(void) {
  assert(rxlog == NULL);
  rxlog = fopen(path_inprogress_log, "w+");
  if (rxlog == NULL) {
    perror("Failed to open receive log");
    return -errno;
  }

  printf("Opened log file: %s\n", path_inprogress_log);
  return 0;
}

/* Close and rename the receive log */
static int closeReceiveLog(const char* new_path) {
  int res;

  assert(rxlog != NULL);
  res = fclose(rxlog);
  rxlog = NULL;
  if (res < 0) {
    perror("Failed to close receive log cleanly");
    return -errno;
  }

  printf("Closed log file: %s\n", path_inprogress_log);
  if (new_path) {
    res = renameAndSymlink(path_inprogress_log, new_path, path_latest_log);
    if (res < 0) {
      return res;
    }
  }

  return 0;
}

/* Write a (null-terminated!) raw string to the file */
static int emitLogRecordRaw(const char* str) {
  assert(rxlog != NULL);
  int res = fputs(str, rxlog);
  if (res < 0) {
    perror("Failed to write to receive log");
    return -errno;
  }
  return 0;
}

/* Log buffer flusher helper */
static int emitLogBufferContent(char* buffer, char** const ptr, uint8_t* rem, uint8_t sz) {
  /* NB: buffer is not necessarily zero terminated! */
  const uint8_t write_sz = sz - *rem;
  assert(rxlog != NULL);

  size_t written = fwrite((void*)buffer, 1, write_sz, rxlog);
  if (written < write_sz) {
    perror("Truncated write whilst emitting to receive log");
    return -errno;
  }

  /* Success, reset the remaining space pointer */
  *ptr = buffer;
  *rem = sz;
  return 0;
}

/* Emit a string */
static int emitLogRecordString(const char* str) {
  int res;
  char buffer[128];
  char* ptr = buffer;
  uint8_t rem = (uint8_t)sizeof(buffer);

  assert(rxlog != NULL);

  // Begin with '"'
  *(ptr++) = '"';
  rem--;

  while (*str) {
    switch (*str) {
      case '\\':
      case '"':
        // Escape with '\' character
        if (!rem) {
          res = emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
          if (res < 0)
            return res;
        }
        *(ptr++) = '\\';
        rem--;
        if (!rem) {
          res = emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
          if (res < 0)
            return res;
        }
        *(ptr++) = *str;
        rem--;
        break;
      case '\n':
        /* Emit '\n' */
        if (!rem) {
          res = emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
          if (res < 0)
            return res;
        }
        *(ptr++) = '\\';
        rem--;
        if (!rem) {
          res = emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
          if (res < 0)
            return res;
        }
        *(ptr++) = 'n';
        rem--;
        break;
       default:
        /* Pass through "safe" ranges */
        if ((*str) < ' ')
          break;

        if ((*str) > '~')
          break;

        if (!rem) {
          res = emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
          if (res < 0)
            return res;
        }
        *(ptr++) = *str;
        rem--;
        break;
    }

    str++;
  }

  // End with '"'
  if (!rem) {
    res = emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
    if (res < 0)
      return res;
  }
  *(ptr++) = '"';
  rem--;

  return emitLogBufferContent(buffer, &ptr, &rem, (uint8_t)sizeof(buffer));
}

/* Begin a log record in the log file */
static int beginReceiveLogRecord(const char* type, const char* msg) {
  int res;
  int64_t time_sec;
  int16_t time_msec;

  assert(rxlog != NULL);
  assert(type != NULL);

  {
    struct timeval tv;
    res = gettimeofday(&tv, NULL);
    if (res < 0) {
      perror("Failed to retrieve current time");
      return -errno;
    }
    time_sec = (int64_t)tv.tv_sec;
    time_msec = (uint16_t)(tv.tv_usec / 1000);
  }

  res = fprintf(rxlog, "{\"timestamp\": %ld%03u, \"type\": ", time_sec, time_msec);
  if (res < 0) {
    perror("Failed to emit record timestamp or type key");
    return -errno;
  }

  res = emitLogRecordString(type);
  if (res < 0) {
    return res;
  }

  if (msg) {
    res = emitLogRecordRaw(", \"msg\": ");
    if (res < 0) {
      return -errno;
    }

    res = emitLogRecordString(msg);
    if (res < 0) {
      return res;
    }
  }

  return 0;
}

/* Finish a log record */
static int finishReceiveLogRecord(const char* endstr) {
  int res;

  if (endstr) {
    res = emitLogRecordRaw(endstr);
    if (res < 0) {
      return res;
    }
  }

  res = emitLogRecordRaw("}\n");
  if (res < 0) {
    return res;
  }

  res = fflush(rxlog);
  if (res < 0) {
    perror("Failed to flush log record");
    return -errno;
  }

  return 0;
}

/* Emit a complete simple log message with no keys. */
static int emitSimpleReceiveLogRecord(const char* type, const char* msg) {
  int res = beginReceiveLogRecord(type, msg);
  if (res < 0)
    return res;
  return finishReceiveLogRecord(NULL);
}

static void showStatusbarMessage(const char* msg) {
  printf("Status: %s\n", msg);
  if (rxlog) {
    if (emitSimpleReceiveLogRecord(logmsg_status, msg) < 0) {
      // Bail here!
      Abort = true;
    }
  }
}

static void onVisIdentified(void) {
  char buffer[128];
  const int idx = VISmap[VIS];
  int res = openReceiveLog();
  if (res < 0) {
    Abort = true;
    return;
  }

  snprintf(buffer, sizeof(buffer), "Detected mode %s (VIS code 0x%02x)", ModeSpec[idx].Name, VIS);
  puts(buffer);
  res = beginReceiveLogRecord(logmsg_vis_detect, buffer);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = fprintf(rxlog, ", \"code\": %d, \"mode\": ", VIS);
  if (res < 0) {
    perror("Failed to write VIS code or mode key");
    Abort = true;
    return;
  }

  res = emitLogRecordString(ModeSpec[idx].ShortName);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = emitLogRecordRaw(", \"desc\": ");
  if (res < 0) {
    Abort = true;
    return;
  }

  res = emitLogRecordString(ModeSpec[idx].Name);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = finishReceiveLogRecord(NULL);
  if (res < 0) {
    Abort = true;
  }

  fsk_id = NULL;
}

static void onVideoInitBuffer(uint8_t mode) {
  printf("Init buffer for mode %d", mode);
}

static void onVideoStartRedraw(void) {
  printf("\n\nBEGIN REDRAW\n\n");
}

static void onVideoWritePixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
  printf("(%d, %d) = #%02x%02x%02x\n", x, y, r, g, b);

  /* For now, emit the pixel data in the receive log */

  int res = beginReceiveLogRecord("PIXELDATA", NULL);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = fprintf(rxlog, ", \"x\": %d, \"y\": %d, \"r\": %d, \"g\": %d, \"b\": %d",
      x, y, r, g, b);
  if (res < 0) {
    perror("Failed to write pixel data");
    Abort = true;
    return;
  }

  res = finishReceiveLogRecord(NULL);
  if (res < 0) {
    Abort = true;
    return;
  }
}

static void onVideoRefresh(void) {
  printf("\n\nREFRESH\n\n");
}

static void onListenerWaiting(void) {
  printf("Listener is waiting\n");
}

static void onListenerReceivedManual(void) {
  printf("Listener received something in manual mode\n");
}

static void onListenerReceiveFSK(void) {
  printf("Listener is now receiving FSK");
  if (emitSimpleReceiveLogRecord(logmsg_fsk_detect, NULL) < 0) {
    // Bail here!
    Abort = true;
  }
}

static void onListenerReceivedFSKID(const char *id) {
  printf("Listener got FSK %s", id);
  fsk_id = id;

  int res = beginReceiveLogRecord(logmsg_fsk_received, NULL);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = emitLogRecordRaw(", \"id\": ");
  if (res < 0) {
    Abort = true;
    return;
  }

  res = emitLogRecordString(id);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = finishReceiveLogRecord(NULL);
  if (res < 0) {
    Abort = true;
    return;
  }
}

static void onListenerReceiveStarted(void) {
  static char rctime[8];
  strftime(rctime,  sizeof(rctime)-1, "%H:%Mz", ListenerReceiveStartTime);
  printf("Receive started at %s", rctime);
  if (emitSimpleReceiveLogRecord(logmsg_receive_start, "Receive started") < 0) {
    // Bail here!
    Abort = true;
  }
}

static void onListenerAutoSlantCorrect(void) {
  printf("Performing slant correction");
  if (emitSimpleReceiveLogRecord(logmsg_status, "Performing slant correction") < 0) {
    // Bail here!
    Abort = true;
  }
}

static void onListenerReceiveFinished(void) {
  char timestamp[20];
  strftime(timestamp,  sizeof(timestamp)-1, "%Y-%m-%dT%H-%MZ", ListenerReceiveStartTime);

  char output_path_log[128];
#if 0
  char output_path_img[128];
#endif
  size_t output_path_rem = sizeof(output_path_log);
  size_t next_len;

  next_len = strlen(timestamp);
  if (next_len <= output_path_rem) {
    strncpy(output_path_log, timestamp, output_path_rem);
    output_path_rem -= next_len;
  }

  next_len = strlen(ModeSpec[CurrentPic.Mode].ShortName) + 1;
  if (next_len <= output_path_rem) {
    strncat(output_path_log, "-", output_path_rem);
    strncat(output_path_log, ModeSpec[CurrentPic.Mode].ShortName, output_path_rem);
    output_path_rem -= next_len;
  }

  if (fsk_id && output_path_rem) {
    next_len = strlen(fsk_id) + 1;
    if (next_len <= output_path_rem) {
      strncat(output_path_log, "-", output_path_rem);
      strncat(output_path_log, fsk_id, output_path_rem);
      output_path_rem -= next_len;
    }
  }

#if 0
  strncpy(output_path_img, output_path_log, sizeof(output_path_img));
  if (output_path_rem < 5) {
    /* Truncate to make room for ".png\0" */
    output_path_img[sizeof(output_path_img) - 5] = 0;
  }
  strncat(output_path_img, ".png", sizeof(output_path_img) - strlen(output_path_img));
#endif

  if (output_path_rem < 7) {
    /* Truncate to make room for ".ndjson\0" */
    output_path_log[sizeof(output_path_log) - 5] = 0;
  }
  strncat(output_path_log, ".ndjson", sizeof(output_path_log) - strlen(output_path_log));

  int res = emitSimpleReceiveLogRecord(logmsg_receive_end, NULL);
  if (res == 0)
    res = closeReceiveLog(output_path_log);
  if (res < 0) {
    Abort = true;
    return;
  }

  printf("Received %d × %d pixel image\n",
      ModeSpec[CurrentPic.Mode].ImgWidth,
      ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight);
}

static void showVU(double *Power, int FFTLen, int WinIdx) {
  if (!rxlog) {
    /* No log, so do nothing */
    return;
  }

  int res = beginReceiveLogRecord(logmsg_sig_strength, NULL);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = fprintf(rxlog, ", \"win\": %d, \"fft\": [", WinIdx);
  if (res < 0) {
    perror("Failed to write window index or start of FFT array");
    Abort = true;
    return;
  }

  for (int i = 0; i < FFTLen; i++) {
    if (i > 0) {
      res = emitLogRecordRaw(", ");
      if (res < 0) {
        Abort = true;
        return;
      }
    }

    res = fprintf(rxlog, "%f", Power[i]);
    if (res < 0) {
      Abort = true;
      return;
    }
  }

  res = finishReceiveLogRecord("]");
}

static void showPCMError(const char* error) {
  if (rxlog) {
    if (emitSimpleReceiveLogRecord(logmsg_warning, error) < 0) {
      // Bail here!
      Abort = true;
    }
  }

  printf("\n\nPCM Error: %s\n\n", error);
}

static void showPCMDropWarning(void) {
  if (rxlog) {
    if (emitSimpleReceiveLogRecord(logmsg_warning, "PCM frames dropped") < 0) {
      // Bail here!
      Abort = true;
    }
  }

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
    exit(DAEMON_EXIT_INIT_FFT_ERR);
  }

  if (initPcmDevice("default") < 0) {
    exit(DAEMON_EXIT_INIT_PCM_ERR);
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

  return (daemon_exit_status);
}
