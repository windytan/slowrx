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

#include "common.h"

void wavdemod () {
  return;
}

/*
void populate_recent() {


}*/

void *Cam() {

  double     Rate = 44100;
  int        Skip = 0, i=0;
  int        Mode = 0;
  time_t     timet;
  char       dest[40];
  char       pngfilename[40];
  char       lumfilename[40];
  struct tm *timeptr = NULL;
  char       infostr[60];
  char       rctime[8];


  while (1) {
 
    //PcmInStream = popen( "sox -q -t alsa hw:0 -t .raw -b 16 -c 1 -e signed-integer -r 44100 -L - 2>/dev/null", "r");
    PcmInStream = popen( "sox -q iss.ogg -t raw -b 16 -c 1 -e signed-integer -r 44100 -L - 2>/dev/null", "r");

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
    PCM = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * 44100, sizeof(double));
    if (PCM == NULL) {
      perror("Cam: Unable to allocate memory for PCM");
      pclose(PcmInStream);
      exit(EXIT_FAILURE);
    }

    // Allocate space for cached FFT
    StoredFreq = calloc( (int)(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight + 1) * 44100, sizeof(double));
    if (StoredFreq == NULL) {
      perror("Cam: Unable to allocate memory for demodulated signal");
      pclose(PcmInStream);
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
    PcmPointer = 0;
    Sample     = 0;
    Rate       = 44100;
    Skip       = 0;
    printf("  getvideo @ %.02f Hz, Skip %d, FShift %.0f Hz\n", Rate, Skip, HedrShift);

    GetVideo(Mode, Rate, Skip, 0, TRUE, FALSE);

    // Done with the input stream
    pclose(PcmInStream);
    PcmInStream = 0;

    // Fix slant
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Calculating slant" );
    gtk_widget_set_sensitive(vutable, FALSE);
    gdk_threads_leave();
    printf("  FindSync @ %.02f Hz\n",Rate);
    Rate = FindSync(PcmPointer, Mode, Rate, &Skip);
   
    free(PCM);
    PCM = NULL;

    if (Rate != 44100) {
      // Final image  
      gdk_threads_enter();
      gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Redrawing" );
      gdk_threads_leave();
      printf("  getvideo @ %.02f Hz, Skip %d, FShift %.0f Hz\n", Rate, Skip, HedrShift);
      GetVideo(Mode, Rate, Skip, 0, TRUE, TRUE);
    }

    // Write cam image to PNG
    gdk_threads_enter();
    gtk_statusbar_push( GTK_STATUSBAR(statusbar), 0, "Saving" );
    gdk_threads_leave();

    FILE *LumFile;
    LumFile = fopen(lumfilename,"w");
    if (LumFile == NULL) {
      perror("Unable to open luma file for writing");
      exit(EXIT_FAILURE);
    }

    char Lum[1] = {0};

    for (i=0; i<(ModeSpec[Mode].LineLen * ModeSpec[Mode].ImgHeight) * 44100; i++) {
      Lum[0] = clip((StoredFreq[i] - (1500 + HedrShift)) / 3.1372549);
      fwrite(Lum,1,1,LumFile);
    }
    fclose(LumFile);

    png_t png;

    png_init(0,0);

    unsigned char data[800 * 616 * 3] = {0};
    int x=0,y=0;
    int rowstride = gdk_pixbuf_get_rowstride (CamPixbuf);
    guchar *pixels, *p;
    pixels = gdk_pixbuf_get_pixels(CamPixbuf);

    for (y = 0; y < ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale; y++) {
      for (x = 0; x < ModeSpec[Mode].ImgWidth; x++) {
        p = pixels + y * rowstride + x * 3;
        data[y*ModeSpec[Mode].ImgWidth*3 + x*3]     = p[0];
        data[y*ModeSpec[Mode].ImgWidth*3 + x*3 + 1] = p[1];
        data[y*ModeSpec[Mode].ImgWidth*3 + x*3 + 2] = p[2];
      }
    }
    
    png_open_file_write(&png, pngfilename);
    png_set_data(&png, ModeSpec[Mode].ImgWidth, ModeSpec[Mode].ImgHeight * ModeSpec[Mode].YScale, 8, PNG_TRUECOLOR, data);
    png_close_file(&png);
    
    free(StoredFreq);
    StoredFreq = NULL;
    
  }
}

/*
 * main
 */

int main(int argc, char *argv[]) {

  int i;

  pthread_t thread1;

  // Set stdout to be line buffered
  setvbuf(stdout, NULL, _IOLBF, 0);
  
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

  createGUI();

  pthread_create (&thread1, NULL, Cam, NULL);

  gtk_main();

  gdk_pixbuf_unref(CamPixbuf);
  free(PCM);
  free(StoredFreq);
  gtk_widget_destroy(notebook);

  printf("Clean exit\n");

  return (EXIT_SUCCESS);
}
