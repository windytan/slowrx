/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <gtk/gtk.h>
#include <pthread.h>
#include <pnglite.h>

#include <alsa/asoundlib.h>

#include "common.h"

void wavdemod () {
  return;
}

/*
void populate_recent() {


}*/

void *DSPlisten() {

  double         Rate = SRATE;
  int            Skip = 0, i=0;
  int            Mode = 0;
  time_t         timet;
  char           dest[40];
  char           pngfilename[40];
  char           lumfilename[40];
  struct tm     *timeptr = NULL;
  char           infostr[60];
  char           rctime[8];
  unsigned char *Lum;
  FILE          *LumFile;



  while (1) {

    // Wait for VIS
    HedrShift = 0;
    gdk_threads_enter();
    gtk_widget_set_sensitive(vutable, TRUE);
    gdk_threads_leave();

    Mode = GetVIS();
    if (Mode == -1) exit(0);

    printf("  ==== %s ====\n", ModeSpec[Mode].Name);

    timet = time(NULL);
    timeptr = gmtime(&timet);
    strftime(dest, sizeof(dest)-1,"%Y%m%d-%H%M%Sz", timeptr);
    snprintf(pngfilename, sizeof(dest)-1, "rx/%s_%s.png", ModeSpec[Mode].ShortName, dest);
    snprintf(lumfilename, sizeof(dest)-1, "rx/%s_%s-lum", ModeSpec[Mode].ShortName, dest);
    printf("  \"%s\"\n", pngfilename);
    
    // Allocate space for PCM
    PCM = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * SRATE, sizeof(double));
    if (PCM == NULL) {
      perror("DSPlisten: Unable to allocate memory for PCM");
      exit(EXIT_FAILURE);
    }

    // Allocate space for cached FFT
    StoredFreq = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * SRATE, sizeof(double));
    if (StoredFreq == NULL) {
      perror("DSPlisten: Unable to allocate memory for demodulated signal");
      free(PCM);
      exit(EXIT_FAILURE);
    }
  
    // Get video
    strftime(rctime, sizeof(rctime)-1, "%H:%Mz", timeptr);
    snprintf(infostr, sizeof(infostr)-1, "%s @ %+.0f Hz, received at %s", ModeSpec[Mode].Name, HedrShift, rctime);
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Receiving video" );
    gtk_label_set_markup(GTK_LABEL(infolabel), infostr);
    gdk_threads_leave();
    PcmPointer = 2048;
    Sample     = 0;
    Rate       = SRATE;
    Skip       = 0;
    printf("  getvideo @ %.02f Hz, Skip %d, HedrShift %.0f Hz\n", Rate, Skip, HedrShift);

    snd_pcm_start(pcm_handle);
    GetVideo(Mode, Rate, Skip, TRUE, FALSE);
    snd_pcm_drop(pcm_handle);

    // Fix slant
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Calculating slant" );
    gtk_widget_set_sensitive(vutable, FALSE);
    gdk_threads_leave();
    printf("  FindSync @ %.02f Hz\n",Rate);
    Rate = FindSync(PcmPointer, Mode, Rate, &Skip);
   
    free(PCM);
    PCM = NULL;

    // Final image  
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Redrawing" );
    gdk_threads_leave();
    printf("  getvideo @ %.02f Hz, Skip %d, HedrShift %.0f Hz\n", Rate, Skip, HedrShift);
    GetVideo(Mode, Rate, Skip, TRUE, TRUE);
    
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Saving" );
    gdk_threads_leave();

    // Save the raw signal
    Lum = malloc( (ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE );
    if (Lum == NULL) {
      perror("Unable to allocate memory for lum data");
      exit(EXIT_FAILURE);
    }
    for (i=0; i<(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE; i++)
      Lum[i] = clip((StoredFreq[i] - (1500 + HedrShift)) / 3.1372549);

    LumFile = fopen(lumfilename,"w");
    if (LumFile == NULL) {
      perror("Unable to open luma file for writing");
      exit(EXIT_FAILURE);
    }
    fwrite(Lum,1,(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * SRATE,LumFile);
    fclose(LumFile);

    // Save the received image as PNG
    png_t png;
    png_init(0,0);

    guchar *pixels;
    pixels = gdk_pixbuf_get_pixels(RxPixbuf);

    png_open_file_write(&png, pngfilename);
    png_set_data(&png, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, 8, PNG_TRUECOLOR, pixels);
    png_close_file(&png);
    
    free(StoredFreq);
    StoredFreq = NULL;
    
  }
}

void initPcm() {

  // Select sound card
  
  GtkWidget *sdialog;
  GtkWidget *cardcombo;
  GtkWidget *contentarea;
  int card;
  char *cardname;

  sdialog = gtk_dialog_new_with_buttons("Select sound card",NULL,GTK_DIALOG_MODAL,GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,GTK_STOCK_CANCEL,GTK_RESPONSE_REJECT,NULL);

  cardcombo   = gtk_combo_box_text_new();
  contentarea = gtk_dialog_get_content_area(GTK_DIALOG(sdialog));
  gtk_box_pack_start(GTK_BOX(contentarea), cardcombo, FALSE, TRUE, 0);

  int cardnum,numcards;

  numcards=0;
  card = -1;
  do {
    snd_card_next(&card);
    if (card != -1) {
      snd_card_get_name(card,&cardname);
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cardcombo), cardname);
      numcards++;
    }
  } while (card != -1);


  if (numcards == 0) {
    printf("No sound cards found!\n");
    exit(EXIT_SUCCESS);
  } else if (numcards == 1) {
    cardnum = 0;
  } else {

    gtk_combo_box_set_active(GTK_COMBO_BOX(cardcombo), 0);
    gtk_widget_show_all (cardcombo);

    gint result = gtk_dialog_run(GTK_DIALOG(sdialog));
    if (result == GTK_RESPONSE_REJECT) exit(EXIT_SUCCESS);
    cardnum = gtk_combo_box_get_active (GTK_COMBO_BOX(cardcombo));
  }

  snd_pcm_stream_t     PcmInStream = SND_PCM_STREAM_CAPTURE;
  snd_pcm_hw_params_t *hwparams;

  gtk_widget_destroy(sdialog);

  char pcm_name[30];
  sprintf(pcm_name,"plug:default:%d",cardnum);

  snd_pcm_hw_params_alloca(&hwparams);

  if (snd_pcm_open(&pcm_handle, pcm_name, PcmInStream, 0) < 0) {
    fprintf(stderr, "ALSA: Error opening PCM device %s\n", pcm_name);
    exit(EXIT_FAILURE);
  }

  /* Init hwparams with full configuration space */
  if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
    fprintf(stderr, "ALSA: Can not configure this PCM device.\n");
    exit(EXIT_FAILURE);
  }

  unsigned int exact_rate;

  if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
    fprintf(stderr, "ALSA: Error setting interleaved access.\n");
    exit(EXIT_FAILURE);
  }
  /* Set sample format */
  if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
    fprintf(stderr, "ALSA: Error setting format S16_LE.\n");
    exit(EXIT_FAILURE);
  }

  exact_rate = 44100;
  if (snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_rate, 0) < 0) {
    fprintf(stderr, "ALSA: Error setting sample rate.\n");
    exit(EXIT_FAILURE);
  }
  SRate = exact_rate;
  if (exact_rate != 44100) fprintf(stderr, "ALSA: Using %d Hz instead of 44100.\n", exact_rate);

  /* Set number of channels */
  if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 1) < 0) {
    fprintf(stderr, "ALSA: Can't set channels to mono.\n");
    exit(EXIT_FAILURE);
  }

  /* Apply HW parameter settings to */
  /* PCM device and prepare device  */
  if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
    fprintf(stderr, "ALSA: Error setting HW params.\n");
    exit(EXIT_FAILURE);
  }

}

/*
 * main
 */

int main(int argc, char *argv[]) {

  int i;

  pthread_t thread1;

  // Set stdout to be line buffered
  //setvbuf(stdout, NULL, _IOLBF, 0);
  
  for (i=0;i<128;i++) VISmap[i] = UNKNOWN;

  // Map VIS codes to modes
  VISmap[0x02] = R8BW;
  VISmap[0x04] = R24;
  VISmap[0x06] = R12BW;
  VISmap[0x08] = R36;
  VISmap[0x0A] = R24BW;
  VISmap[0x0C] = R72;
  VISmap[0x20] = M4;
  VISmap[0x24] = M3;
  VISmap[0x28] = M2;
  VISmap[0x2C] = M1;
  VISmap[0x37] = W2180;
  VISmap[0x38] = S2;
  VISmap[0x3C] = S1;
  VISmap[0x3F] = W2120;
  VISmap[0x4C] = SDX;
  VISmap[0x5D] = PD50;
  VISmap[0x5E] = PD290;
  VISmap[0x5F] = PD120;
  VISmap[0x60] = PD180;
  VISmap[0x61] = PD240;
  VISmap[0x62] = PD160;
  VISmap[0x63] = PD90;
  VISmap[0x71] = P3;
  VISmap[0x72] = P5;
  VISmap[0x73] = P7;

  gtk_init (&argc, &argv);

  g_thread_init (NULL);
  gdk_threads_init ();

  initPcm();
  createGUI();

  pthread_create (&thread1, NULL, DSPlisten, NULL);

  gtk_main();

  gdk_pixbuf_unref(RxPixbuf);
  free(PCM);
  free(StoredFreq);
  gtk_widget_destroy(notebook);

  printf("Clean exit\n");

  return (EXIT_SUCCESS);
}
