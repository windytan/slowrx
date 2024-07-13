/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <libgen.h>
#include <arpa/inet.h>

#include <pthread.h>

#include <alsa/asoundlib.h>

#include <fftw3.h>
#include <gd.h>

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
#define DAEMON_EXIT_INVALID_ARG (1)
#define DAEMON_EXIT_INIT_FFT_ERR (2)
#define DAEMON_EXIT_INIT_PCM_ERR (3)
#define DAEMON_EXIT_INIT_PATH (4)
#define DAEMON_EXIT_INIT_MUTEX (5)
#define DAEMON_EXIT_FORK_FAILURE (6)

/* Receive refresh interval */
#define REFRESH_INTERVAL (5)

/* Receive execute run interval */
#define RX_EXEC_INTERVAL (30)

/* Exit status to use when exiting */
static int daemon_exit_status = DAEMON_EXIT_SUCCESS;

/* Common log message types */
const char* logmsg_receive_start = "RECEIVE_START";
const char* logmsg_vis_detect = "VIS_DETECT";
const char* logmsg_sig_strength = "SIG_STRENGTH";
const char* logmsg_image_refreshed = "IMAGE_REFRESHED";
const char* logmsg_image_finish = "IMAGE_FINISHED";
const char* logmsg_fsk_detect = "FSK_DETECT";
const char* logmsg_fsk_received = "FSK_RECEIVED";
const char* logmsg_receive_end = "RECEIVE_END";
const char* logmsg_status = "STATUS";
const char* logmsg_warning = "WARNING";

/* The name of the image-in-progress being received */
static char* path_inprogress_img = "inprogress.png";

/* The name of the receive log */
static char* path_inprogress_log = "inprogress.ndjson";

/* The name of the audio dump file */
static char* path_inprogress_audio = "inprogress.au";

/* The name of the latest receive log */
static char* path_latest_log = "latest.ndjson";

/* The name of the latest received image */
static char* path_latest_img = "latest.png";

/* The name of the latest received audio file */
static char* path_latest_audio = "latest.au";

/* The name of the directory where all images will be kept */
static const char* path_dir = NULL;

/* The path to an executable to run when an event happens */
static const char* rx_exec = NULL;

/* Time the image was last written to */
static time_t last_refresh = 0;

/* Time the script was last run */
static time_t last_rx_exec = 0;

/* The FSK ID detected after image transmission */
static const char *fsk_id = NULL;

/* Pointer to the receive log (NDJSON format) */
static FILE* rxlog = NULL;
static pthread_mutex_t rxlog_mutex;

/* Pointer to the audio dump (Sun Audio format) */
static FILE* audiofile = NULL;

/* The currently selected mode */
const _ModeSpec* rxmode = NULL;

/* Pointer to the raw image data being received */
static gdImagePtr rximg;

/* Execute a script, passing in the image file and receive log */
static void exec_rx_cmd(const char* event, const char* img_path, const char* log_path, const char* audio_path) {
  if (rx_exec) {
    printf("Running %s %s %s %s %s\n", rx_exec, event, img_path, log_path, (audio_path ? audio_path : ""));
    pid_t child_pid = fork();
    if (child_pid > 0) {
      printf("Waiting for PID %d\n", child_pid);
      waitpid(child_pid, NULL, 0);
    } else if (child_pid == 0) {
      char* _rx_exec = strdup(rx_exec);
      if (!_rx_exec) {
        perror("Failed to strdup process name");
        return;
      }

      char* _event = strdup(event);
      if (!_event) {
        perror("Failed to strdup event");
        return;
      }

      char* _img_path = strdup(img_path);
      if (!_img_path) {
        perror("Failed to strdup image path");
        return;
      }

      char* _log_path = strdup(log_path);
      if (!_log_path) {
        perror("Failed to strdup log path");
        return;
      }

      char* _audio_path = NULL;
      if (audio_path) {
        _audio_path = strdup(audio_path);
        if (!_audio_path) {
          perror("Failed to strdup audio path");
          return;
        }
      }

      char* argv[] = { _rx_exec, _event, _img_path, _log_path, _audio_path, NULL };
      printf("Executing script\n");
      int res = execve(rx_exec, argv, NULL);
      if (res < 0) {
        perror("Failed to exec() receive command");
      }
      exit(DAEMON_EXIT_FORK_FAILURE);
    } else if (child_pid < 0) {
      perror("Failed to fork() for exec");
    }
  }
}

/* Safely concatenate two strings */
static int safe_strncat(char* dest, const char* src, size_t* dest_rem) {
  size_t src_len = strlen(src);
  if (src_len <= *dest_rem) {
    strncat(dest, src, *dest_rem);
    *dest_rem -= src_len;

    return src_len;
  } else {
    errno = E2BIG;
    return -errno;
  }
}

/* Append a path segment */
static int path_append(char* path, const char* filename, size_t* path_rem) {
  char* path_end = &path[strlen(path)];
  int res;

  if ((path_end > path) && (path_end[-1] != '/')) {
    /* Path does not end in a slash, so append one now */
    res = safe_strncat(path, "/", path_rem);
    if (res < 0) {
      return res;
    }
  }

  res = safe_strncat(path, filename, path_rem);
  if (res < 0) {
    /* Wind back concatenations */
    path_end = 0;
    return res;
  }

  return res;
}

/* Rename and symlink a file */
static int renameAndSymlink(const char* existing_path, const char* new_path, const char* symlink_path) {
  int res = rename(existing_path, new_path);
  if (res < 0) {
    perror("Failed to rename receive log");
    return -errno;
  }
  printf("Renamed %s to %s\n", existing_path, new_path);

  if (symlink_path) {
    /* Figure out the target */
    char target[PATH_MAX];
    const char* symlink_target;
    strncpy(target, new_path, sizeof(target) - 1);
    symlink_target = basename(target);

    res = unlink(symlink_path);
    if (res < 0) {
      /* ENOENT is okay */
      if (errno != ENOENT) {
        perror("Failed to remove old 'latest' symlink");
        return -errno;
      }
    }
    printf("Removed old symlink %s\n", symlink_path);

    res = symlink(symlink_target, symlink_path);
    if (res < 0) {
      perror("Failed to symlink receive log");
      return -errno;
    }
    printf("Symlinked %s to %s\n", symlink_target, symlink_path);
  }

  return 0;
}

/* Open the receive log ready for traffic */
static int openAudioDump(void) {
  assert(audiofile == NULL);
  audiofile = fopen(path_inprogress_audio, "wb+");
  if (audiofile == NULL) {
    perror("Failed to open audio dump file");
    return -errno;
  }

  /* https://en.wikipedia.org/wiki/Au_file_format */
  uint32_t hdr[7] = {
    0x2e736e64,     // Magic ".snd"
    28,             // Data offset: 28 bytes (7*4-bytes)
    UINT32_MAX,     // Data size: unknown
    3,              // Encoding: int16_t linear
    pcm.SampleRate, // Sample rate
    1,              // Channels
    0,              // Annotation (unused)
  };
  for (int i = 0; i < 7; i++) {
    // Byte swap to big-endian
    hdr[i] = htonl(hdr[i]);
  }

  // Write the header
  size_t sz = fwrite(hdr, sizeof(uint32_t), 7, audiofile);
  if (sz < 7) {
    perror("Failed to write audio file header");
    int res = fclose(audiofile);
    if (res < 0) {
      perror("Also failed to close partial file");
    }
    return -errno;
  }

  printf("Opened audio dump file: %s\n", path_inprogress_audio);
  return 0;
}

static int closeAudioDump(void) {
  if (audiofile == NULL)
    return 0;

  pcm.PCMReadCallback = NULL;

  int res = fclose(audiofile);
  audiofile = NULL;
  if (res < 0) {
    perror("Failed to clse audio dump file");
    return -errno;
  }

  return 0;
}

static void writeToAudioDump(int32_t numsamples, const int16_t* samples) {
  if (audiofile) {
    int16_t be_samples[512];
    int res, i;
    size_t sz;

    while (numsamples) {
      // Clamp at the block size
      int32_t block_sz = (numsamples > 512) ? 512 : numsamples;
      // Copy to temp buffer
      memcpy(be_samples, samples, sizeof(int16_t)*block_sz);
      // Byteswap buffer
      for (i = 0; i < block_sz; i++) {
        be_samples[i] = htons(be_samples[i]);
      }
      // Write buffer
      sz = fwrite(be_samples, sizeof(int16_t), block_sz, audiofile);
      if (sz < (size_t)block_sz) {
        perror("Failed to write audio block");
        closeAudioDump();
        return;
      }
      // Advance
      numsamples -= block_sz;
      samples += block_sz;
    }

    res = fflush(audiofile);
    if (res < 0) {
      perror("Failed to flush audio file");
      closeAudioDump();
    }
  }
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

  res = pthread_mutex_lock(&rxlog_mutex);
  if (res < 0) {
    perror("Failed to lock output log");
    return -errno;
  }

  res = fprintf(rxlog, "{\"timestamp\": %" PRId64 "%03u, \"type\": ", time_sec, time_msec);
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
  int res, unlock_res;

  if (endstr) {
    res = emitLogRecordRaw(endstr);
    if (res < 0) {
      goto unlock;
    }
  }

  res = emitLogRecordRaw("}\n");
  if (res < 0) {
    goto unlock;
  }

  res = fflush(rxlog);
  if (res < 0) {
    perror("Failed to flush log record");
    res = -errno;
    goto unlock;
  }

  if (res < 0) {
    perror("Failed to lock output log");
    res = -errno;
    goto unlock;
  }

unlock:
  unlock_res = pthread_mutex_unlock(&rxlog_mutex);
  if (unlock_res < 0) {
    perror("FAILED TO UNLOCK OUTPUT LOG!!!");
    assert(0);
  }
  return res;
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

  last_refresh = 0;
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
  exec_rx_cmd(logmsg_vis_detect, path_inprogress_img, path_inprogress_log, path_inprogress_audio);

  if (path_inprogress_audio) {
    res = openAudioDump();
    if (res == 0) {
      // Fetch the initial part that triggered this recording.
      writeToAudioDump(pcm.WindowPtr*2, pcm.Buffer);
    }
    if (res == 0) {
      pcm.PCMReadCallback = writeToAudioDump;
    }
  }
}

static void onVideoInitBuffer(uint8_t mode) {
  rxmode = &ModeSpec[mode];

  printf("Init buffer for mode %s\n", rxmode->Name);

  /* Allocate the image */
  rximg = gdImageCreateTrueColor(rxmode->ImgWidth, rxmode->NumLines * rxmode->LineHeight);
  if (!rximg) {
    perror("Failed to allocate image buffer");
    Abort = true;
  }
}

static void onVideoStartRedraw(void) {
  printf("\n\nBEGIN REDRAW\n\n");
}

static void onVideoWritePixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
  /* Check bounds */
  if (x >= rxmode->ImgWidth)
    return;

  if (y >= rxmode->NumLines)
    return;

  if (!rximg)
    return;

  /* Handle line height */
  y *= rxmode->LineHeight;

  /* Draw pixel or line */
  for (uint16_t yo = 0; yo < rxmode->LineHeight; yo++) {
    gdImageSetPixel(rximg, x, y + yo, gdImageColorResolve(rximg, r, g, b));
  }
}

static int refreshImage(_Bool force) {
  int res;

  if (!force && last_refresh) {
    struct timeval tv;
    res = gettimeofday(&tv, NULL);
    if (res >= 0) {
      uint16_t age = (uint16_t)(tv.tv_sec - last_refresh);

      if (age < REFRESH_INTERVAL) {
        /* Hasn't been long enough, don't bother */
        return 0;
      }
    }
  } else if (force) {
    printf("Forced refresh\n");
  } else if (!last_refresh) {
    printf("First refresh\n");
  }

  /* Open a file for writing. "wb" means "write binary", important
     under MSDOS, harmless under Unix. */
  FILE* pngout = fopen(path_inprogress_img, "wb");
  if (pngout) {
    /* Output the image to the disk file in PNG format. */
    gdImagePng(rximg, pngout);

    /* Close the files. */
    res = fclose(pngout);
    if (res < 0) {
      perror("Failed to write in-progress image");
      return -errno;
    } else {
      struct timeval tv;
      res = gettimeofday(&tv, NULL);
      if (res >= 0) {
        /* Mark refresh time */
        last_refresh = tv.tv_sec;
      }
      printf("Image refreshed\n");
    }
  } else {
    perror("Failed to open in-progress image for writing");
    return -errno;
  }

  if (force || ((last_refresh - last_rx_exec) > RX_EXEC_INTERVAL)) {
    exec_rx_cmd(logmsg_image_refreshed, path_inprogress_img, path_inprogress_log, path_inprogress_audio);
  }
  return 0;
}

static void onVideoRefresh(void) {
  if (refreshImage(false) < 0) {
    Abort = true;
  }
}

static void onListenerWaiting(void) {
  printf("Listener is waiting\n");
}

static void onListenerReceivedManual(void) {
  printf("Listener received something in manual mode\n");
}

static void onListenerReceiveFSK(void) {
  printf("Listener is now receiving FSK\n");
  if (emitSimpleReceiveLogRecord(logmsg_fsk_detect, NULL) < 0) {
    // Bail here!
    Abort = true;
  }
}

static void onListenerReceivedFSKID(const char *id) {
  if (strlen(id)) {
    printf("Listener got FSK %s\n", id);
    fsk_id = id;
  } else {
    printf("No FSK received\n");
    fsk_id = NULL;
    return;
  }

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
  printf("Receive started at %s\n", rctime);
  if (emitSimpleReceiveLogRecord(logmsg_receive_start, "Receive started") < 0) {
    // Bail here!
    Abort = true;
  }
}

static void onListenerAutoSlantCorrect(void) {
  printf("Performing slant correction\n");
  if (emitSimpleReceiveLogRecord(logmsg_status, "Performing slant correction") < 0) {
    // Bail here!
    Abort = true;
  }
}

static void onListenerReceiveFinished(void) {
  char timestamp[20];
  strftime(timestamp,  sizeof(timestamp)-1, "%Y-%m-%dT%H-%MZ", ListenerReceiveStartTime);

  char output_path_log[PATH_MAX];
  char output_path_img[PATH_MAX];
  char output_path_audio[PATH_MAX];
  size_t output_path_rem = sizeof(output_path_log) - 1;

  if (path_dir) {
    strncpy(output_path_log, path_dir, output_path_rem);
    output_path_rem -= strlen(path_dir);
  } else {
    output_path_log[0] = 0;
  }

  int res = path_append(output_path_log, timestamp, &output_path_rem);
  if (res >= 0) {
    res = safe_strncat(output_path_log, "-", &output_path_rem);
    if (res >= 0) {
      res = safe_strncat(output_path_log, ModeSpec[CurrentPic.Mode].ShortName, &output_path_rem);
    }
  }

  if (fsk_id && output_path_rem && (res >= 0)) {
    /* fsk_id might contain characters that are unsafe! */
    char fsk_safe[strlen(fsk_id) + 1];
    char *c = fsk_safe;
    strcpy(fsk_safe, fsk_id);

    while (*c) {
      if (isalpha(*c)) {
        /* Upper-case letters */
        *c = toupper(*c);
      } else if (!isdigit(*c)) {
        /* Convert non-digits to hypens */
        *c = '-';
      }
      c++;
    }

    res = safe_strncat(output_path_log, "-", &output_path_rem);
    if (res >= 0) {
      res = safe_strncat(output_path_log, fsk_safe, &output_path_rem);
    }
  }

  if (path_inprogress_audio) {
    strncpy(output_path_audio, output_path_log, sizeof(output_path_audio));
    if (output_path_rem < 4) {
      /* Truncate to make room for ".au\0" */
      output_path_audio[sizeof(output_path_audio) - 4] = 0;
    }
    strncat(output_path_audio, ".au", sizeof(output_path_audio) - strlen(output_path_audio) - 1);
  }

  strncpy(output_path_img, output_path_log, sizeof(output_path_img));
  if (output_path_rem < 5) {
    /* Truncate to make room for ".png\0" */
    output_path_img[sizeof(output_path_img) - 5] = 0;
  }
  strncat(output_path_img, ".png", sizeof(output_path_img) - strlen(output_path_img) - 1);

  if (output_path_rem < 7) {
    /* Truncate to make room for ".ndjson\0" */
    output_path_log[sizeof(output_path_log) - 5] = 0;
  }
  strncat(output_path_log, ".ndjson", sizeof(output_path_log) - strlen(output_path_log));

  printf("Output files will be %s (image) and %s (log)\n",
      output_path_img, output_path_log);

  /* Refresh one more time, then rename the file */
  refreshImage(true);
  renameAndSymlink(path_inprogress_img, output_path_img, path_latest_img);

  /* Release the image buffer */
  gdImageDestroy(rximg);
  rximg = NULL;

  res = emitSimpleReceiveLogRecord(logmsg_receive_end, NULL);
  if (res == 0)
    res = closeReceiveLog(output_path_log);
  if (res < 0) {
    Abort = true;
    return;
  }

  if (path_inprogress_audio) {
    res = closeAudioDump();
    if (res == 0) {
      res = renameAndSymlink(path_inprogress_audio, output_path_audio, path_latest_audio);
    }
  }

  printf("Received %d × %d pixel image\n",
      ModeSpec[CurrentPic.Mode].ImgWidth,
      ModeSpec[CurrentPic.Mode].NumLines * ModeSpec[CurrentPic.Mode].LineHeight);
  exec_rx_cmd(logmsg_receive_end, output_path_img, output_path_log, output_path_audio);

  /* Wait for all children to exit */
  wait(NULL);
}

static void showVU(double *Power, int FFTLen, int WinIdx) {
  if (!rxlog) {
    /* No log, so do nothing */
    return;
  }

  /* Scan forwards, find the first non-zero record */
  int first_bucket = 0;
  for (int i = first_bucket; i < FFTLen; i++) {
    if (Power[i] != 0.0) {
      first_bucket = i;
      break;
    }
  }

  /* Scan backwards, find the first non-zero record */
  int last_bucket = FFTLen-1;
  for (int i = last_bucket; i >= first_bucket; i--) {
    if (Power[i] != 0.0) {
      last_bucket = i;
      break;
    }
  }

  int res = beginReceiveLogRecord(logmsg_sig_strength, NULL);
  if (res < 0) {
    Abort = true;
    return;
  }

  res = fprintf(rxlog,
      ", \"win\": %d, "
      "\"num\": %d, "
      "\"first\": %d, "
      "\"last\": %d, \"fft\": [",
      WinIdx, FFTLen, first_bucket, last_bucket);
  if (res < 0) {
    perror("Failed to write window index or start of FFT array");
    Abort = true;
    return;
  }

  for (int i = first_bucket; i <= last_bucket; i++) {
    if (i > first_bucket) {
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

char* path_append_dir_dup(const char* filename) {
  char path[PATH_MAX] = {0};
  size_t path_rem = sizeof(path) - 1;

  if (path_dir) {
    strncpy(path, path_dir, path_rem);
    path_rem -= strlen(path_dir);

    int res = path_append(path, filename, &path_rem);
    if (res < 0) {
      return NULL;
    }
  } else {
    strncpy(path, filename, path_rem);
  }

  return strdup(path);
}

static _Bool is_disabled_path(const char* path) {
  return (path[0] == '-') && (path[1] == 0);
}

int main(int argc, char *argv[]) {
  // Initialise mutex
  int res = pthread_mutex_init(&rxlog_mutex, NULL);
  if (res < 0) {
    perror("Failed to create mutex");
    exit(DAEMON_EXIT_INIT_MUTEX);
  }

  // Set up defaults
  const char* pcm_device = "default";
  ListenerAutoSlantCorrect = true;
  ListenerEnableFSKID = true;
  VisAutoStart = true;
  uint16_t sample_rate = 44100;
  uint8_t channel = PCM_CH_LEFT;

  {
    _Bool path_inprogress_img_set = false;
    _Bool path_inprogress_log_set = false;
    _Bool path_inprogress_audio_set = false;
    _Bool path_latest_img_set = false;
    _Bool path_latest_log_set = false;
    _Bool path_latest_audio_set = false;
    int opt;

    while ((opt = getopt(argc, argv, "A:FI:Sh:L:a:c:d:i:l:p:r:x:")) != -1) {
      switch (opt) {
        case 'A': // In-progress audio path
          if (path_inprogress_audio_set) {
            free(path_inprogress_audio);
          } else {
            path_inprogress_audio_set = true;
          }
          if (is_disabled_path(optarg)) {
            path_inprogress_audio = NULL;
          } else {
            path_inprogress_audio = path_append_dir_dup(optarg);
            if (!path_inprogress_audio) {
              perror("Failed to compute full in-progress audio path");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'F': // Disable FSKID
          ListenerEnableFSKID = false;
          break;
        case 'I': // In-progress image path
          if (path_inprogress_img_set) {
            free(path_inprogress_img);
          } else {
            path_inprogress_img_set = true;
          }
          if (is_disabled_path(optarg)) {
            path_inprogress_img = NULL;
          } else {
            path_inprogress_img = path_append_dir_dup(optarg);
            if (!path_inprogress_img) {
              perror("Failed to compute full in-progress image path");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'S': // Disable slant correction
          ListenerAutoSlantCorrect = false;
          break;
        case 'L': // In-progress receive log path
          if (path_inprogress_log_set) {
            free(path_inprogress_log);
          } else {
            path_inprogress_log_set = true;
          }
          if (is_disabled_path(optarg)) {
            path_inprogress_log = NULL;
          } else {
            path_inprogress_log = path_append_dir_dup(optarg);
            if (!path_inprogress_log) {
              perror("Failed to compute full in-progress log path");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'a': // Latest audio path
          if (path_latest_audio_set) {
            free(path_latest_audio);
          } else {
            path_latest_audio_set = true;
          }
          if (is_disabled_path(optarg)) {
            path_latest_audio = NULL;
          } else {
            path_latest_audio = path_append_dir_dup(optarg);
            if (!path_latest_audio) {
              perror("Failed to compute full latest audio path");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'c': // Audio channel selection
          switch (optarg[0]) {
            case 'l':
            case 'L':
              channel = PCM_CH_LEFT;
              break;
            case 'r':
            case 'R':
              channel = PCM_CH_RIGHT;
              break;
            case 'm':
            case 'M':
            default:
              channel = PCM_CH_MONO;
              break;
          }
          break;
        case 'd': // Set the output directory
          {
            char abs_dir[PATH_MAX];
            if (realpath(optarg, abs_dir) == abs_dir) {
              path_dir = strdup(abs_dir);
            } else {
              perror("Failed to determine absolute directory");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'h':
          printf("Usage: %s [-h] [-F] [-S] [-A inprogress.au]\n"
              "[-I inprogress.png] [-L inprogress.ndjson] [-a latest.au]\n"
              "[-c channel] [-d directory] [-i latest.png]\n"
              "[-l latest.ndjson] [-p pcmdevice] [-r samplerate]\n"
              "[-x script]\n"
              "\n"
              "where:\n"
              "  -F : disable FSK ID detection\n"
              "  -S : disable slant correction\n"
              "  -h : display this help and exit\n"
              "  -d : set the directory where images are kept\n"
              "  -A : set the in-progress audio dump path (- to disable)\n"
              "  -I : set the in-progress image path (- to disable)\n"
              "  -L : set the in-progress receive log path (- to disable)\n"
              "  -a : set the latest audio dump path (- to disable)\n"
              "  -c : set the audio channel to use, left, right or mono\n"
              "  -i : set the latest image path (- to disable)\n"
              "  -l : set the latest receive log path (- to disable)\n"
              "  -r : set the ALSA PCM sample rate\n"
              "  -p : set the ALSA PCM capture device\n"
              "  -x : specify a script to run on receive events\n",
              argv[0]);
          exit(DAEMON_EXIT_SUCCESS);
          break;
        case 'i': // Latest image path
          if (path_latest_img_set) {
            free(path_latest_img);
          } else {
            path_latest_img_set = true;
          }
          if (is_disabled_path(optarg)) {
            path_latest_img = NULL;
          } else {
            path_latest_img = path_append_dir_dup(optarg);
            if (!path_latest_img) {
              perror("Failed to compute full latest image path");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'l': // Latest receive log path
          if (path_latest_log_set) {
            free(path_latest_log);
          } else {
            path_latest_log_set = true;
          }
          if (is_disabled_path(optarg)) {
            path_latest_log = NULL;
          } else {
            path_latest_log = path_append_dir_dup(optarg);
            if (!path_latest_log) {
              perror("Failed to compute full latest log path");
              exit(DAEMON_EXIT_INIT_PATH);
            }
          }
          break;
        case 'r': // Sample rate
          {
            char* endptr = NULL;
            uint32_t rate = strtoul(optarg, &endptr, 10);
            if (*endptr) {
              printf("Invalid sample rate: %s\n", endptr);
              exit(DAEMON_EXIT_INVALID_ARG);
            }
            if (rate > 48000) {
              printf(
                  "Sample rate %" PRIu32 " out of range, "
                  "Clamping to 48kHz.\n",
                  rate);
              rate = 48000;
            }
            sample_rate = rate;
          }
          break;
        case 'x': // Execute script on event
          rx_exec = strdup(optarg);
          break;
        default:
          printf("Unrecognised: %s\n", optarg);
          exit(DAEMON_EXIT_INVALID_ARG);
          break;
      }
    }

    if (!path_dir) {
      /* Figure out the current working directory */
      char cwd[PATH_MAX];
      if (getcwd(cwd, sizeof(cwd)) == cwd) {
        path_dir = strdup(cwd);
      } else {
        perror("Failed to determine current working directory");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }

    if (!path_inprogress_img_set && path_inprogress_img) {
      path_inprogress_img = path_append_dir_dup(path_inprogress_img);
      if (!path_inprogress_img) {
        perror("Failed to compute full in-progress image path");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }

    if (!path_inprogress_log_set && path_inprogress_log) {
      path_inprogress_log = path_append_dir_dup(path_inprogress_log);
      if (!path_inprogress_log) {
        perror("Failed to compute full in-progress log path");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }

    if (!path_inprogress_audio_set && path_inprogress_audio) {
      path_inprogress_audio = path_append_dir_dup(path_inprogress_audio);
      if (!path_inprogress_audio) {
        perror("Failed to compute full in-progress audio dump path");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }

    if (!path_latest_img_set && path_latest_img) {
      path_latest_img = path_append_dir_dup(path_latest_img);
      if (!path_latest_img) {
        perror("Failed to compute full latest image path");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }

    if (!path_latest_log_set && path_latest_log) {
      path_latest_log = path_append_dir_dup(path_latest_log);
      if (!path_latest_log) {
        perror("Failed to compute full latest log path");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }

    if (!path_latest_audio_set) {
      path_latest_audio = path_append_dir_dup(path_latest_audio);
      if (!path_latest_audio) {
        perror("Failed to compute full latest audio dump path");
        exit(DAEMON_EXIT_INIT_PATH);
      }
    }
  }

  // Prepare FFT
  if (fft_init() < 0) {
    exit(DAEMON_EXIT_INIT_FFT_ERR);
  }

  res = initPcmDevice(pcm_device, sample_rate, channel);
  switch (res) {
  case PCM_RES_SUCCESS:
    break;
  case PCM_RES_SUBOPTIMAL:
    printf("PCM setup is sub-optimal, doing the best we can.\n");
    break;
  default:
    if (res < 0) {
      exit(DAEMON_EXIT_INIT_PCM_ERR);
    }
  }

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
