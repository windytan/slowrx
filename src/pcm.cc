#include "common.hh"

/*
 * Stuff related to sound card capture
 *
 */

PCMworker::PCMworker() : Mutex(), please_stop(false) {

  short win_lens[7] = { 47, 63, 95, 127, 255, 511, 1023 };
  for (int j = 0; j < 7; j++)
    for (int i = 0; i < win_lens[j]; i++)
      window[j][i] = 0.5 * (1 - cos( (2 * M_PI * i) / (win_lens[j] - 1)) );

  open_audio_file("/Users/windy/Movies/sstv_ebeju_f_mono.wav");
}

void PCMworker::open_audio_file (string fname) {

  file = SndfileHandle(fname.c_str()) ;

  printf ("Opened file '%s'\n", fname.c_str()) ;
  printf ("    Sample rate : %d\n", file.samplerate ()) ;
  printf ("    Channels    : %d\n", file.channels ()) ;

  GetVIS();


  //puts ("") ;

  /* RAII takes care of destroying SndfileHandle object. */
}


// read more stuff in
void PCMworker::read_more () {
  printf( "read more\n");
  short read_buffer[READ_CHUNK_LEN];

  file.read(read_buffer, READ_CHUNK_LEN);

  int cirbuf_fits = max(CIRBUF_LEN - cirbuf_head, READ_CHUNK_LEN);

  memcpy(&cirbuf[cirbuf_head], read_buffer, cirbuf_fits * sizeof(read_buffer[0]));

  // wrapped around
  memcpy(&cirbuf[0], &read_buffer[cirbuf_fits], (READ_CHUNK_LEN - cirbuf_fits) * sizeof(read_buffer[0]));

  // mirror
  memcpy(&cirbuf[CIRBUF_LEN], &cirbuf[0], CIRBUF_LEN);
  
  cirbuf_head = (cirbuf_head + READ_CHUNK_LEN) % CIRBUF_LEN;

}

// move processing window
void PCMworker::next_moment () {
  cirbuf_tail = (cirbuf_tail + 1) % CIRBUF_LEN;
  if ((cirbuf_tail + MOMENT_LEN) % CIRBUF_LEN >= cirbuf_head) {
    read_more();
  }
}

// the current moment, windowed
vector<short> PCMworker::get_windowed_moment (WindowType win_type) {
  vector<short> result(MOMENT_LEN);

  for (int i = 0; i < MOMENT_LEN; i++) {

    int win_i = i - MOMENT_LEN/2 + win_lens[win_type]/2 ;

    if (win_i >= 0 && win_i < win_lens[win_type]) {
      result[i] = cirbuf[cirbuf_tail + i] * window[win_type][win_i];
    } else {
      result[i] = 0;
    }
  }

  return result;

}

/*
// Capture fresh PCM data to buffer
void readPcm(int numsamples) {

  int    samplesread, i;
  gint32 tmp[BUFLEN];  // Holds one or two 16-bit channels, will be ANDed to single channel

  //samplesread = snd_pcm_readi(pcm.handle, tmp, (pcm.WindowPtr == 0 ? BUFLEN : numsamples));

  if (samplesread < numsamples) {
    
    if      (samplesread == -EPIPE)
      printf("ALSA: buffer overrun\n");
    else if (samplesread < 0) {
      //printf("ALSA error %d (%s)\n", samplesread, snd_strerror(samplesread));
      gtk_widget_set_tooltip_text(gui.image_devstatus, "ALSA error");
      Abort = TRUE;
      pthread_exit(NULL);
    }
    else
      printf("Can't read %d samples\n", numsamples);
    
    // On first appearance of error, update the status icon
    if (!pcm.BufferDrop) {
      gtk_image_set_from_stock(GTK_IMAGE(gui.image_devstatus),GTK_STOCK_DIALOG_WARNING,GTK_ICON_SIZE_SMALL_TOOLBAR);
      gtk_widget_set_tooltip_text(gui.image_devstatus, "Device is dropping samples");
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

void populateDeviceList() {
  int                  card;
  char                *cardname;
  int                  numcards, row;

  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.combo_card), "default");

  numcards = 0;
  card     = -1;
  row      = 0;
  do {
    //snd_card_next(&card);
    if (card != -1) {
      row++;
      //snd_card_get_name(card,&cardname);
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gui.combo_card), cardname);
      char *dev = g_key_file_get_string(config,"slowrx","device",NULL);
      if (dev == NULL || strcmp(cardname, dev) == 0)
        gtk_combo_box_set_active(GTK_COMBO_BOX(gui.combo_card), row);

      numcards++;
    }
  } while (card != -1);

  if (numcards == 0) {
    perror("No sound devices found!\n");
    exit(EXIT_FAILURE);
  }
  
}*/

// Initialize sound card
// Return value:
//   0 = opened ok
//  -1 = opened, but suboptimal
//  -2 = couldn't be opened
int initPcmDevice(string wanted_dev_name) {

  //snd_pcm_hw_params_t *hwparams;
  void         *hwparams;
  char          pcm_name[30];
  unsigned int  exact_rate = 44100;
  int           card;
  bool          found;
  char         *cardname;

  //pcm.BufferDrop = false;

  //snd_pcm_hw_params_alloca(&hwparams);

  card  = -1;
  found = false;
  if (wanted_dev_name.compare("default") == 0) {
    found=true;
  } else {
    do {
      //snd_card_next(&card);
      if (card != -1) {
        //snd_card_get_name(card,&cardname);
        if (strcmp(cardname, wanted_dev_name.c_str()) == 0) {
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

  if (wanted_dev_name.compare("default") == 0) {
    sprintf(pcm_name,"default");
  } else {
    sprintf(pcm_name,"hw:%d",card);
  }
  
  /*if (snd_pcm_open(&pcm.handle, pcm_name, SND_PCM_STREAM_CAPTURE, 0) < 0) {
    perror("ALSA: Error opening PCM device");
    return(-2);
  }*/

  /* Init hwparams with full configuration space */
  /*if (snd_pcm_hw_params_any(pcm.handle, hwparams) < 0) {
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
  }*/

  // Try stereo first
  /*if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 2) < 0) {
    // Fall back to mono
    if (snd_pcm_hw_params_set_channels(pcm.handle, hwparams, 1) < 0) {
      perror("ALSA: Error setting channels.");
      return(-2);
    }
  }
  if (snd_pcm_hw_params(pcm.handle, hwparams) < 0) {
    perror("ALSA: Error setting HW params.");
    return(-2);
  }*/

  /*pcm.Buffer = calloc( BUFLEN, sizeof(gint16));
  memset(pcm.Buffer, 0, BUFLEN);*/
  
  if (exact_rate != 44100) {
    fprintf(stderr, "ALSA: Got %d Hz instead of 44100. Expect artifacts.\n", exact_rate);
    return(-1);
  }

  return(0);

}
