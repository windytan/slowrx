#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <glib.h>
#include <glib/gtypes.h>

#define VIDEO_MAX_WIDTH (800)
#define VIDEO_MAX_HEIGHT (616)
#define VIDEO_MAX_CHANNELS (3)


extern guchar VideoImage[VIDEO_MAX_WIDTH][VIDEO_MAX_HEIGHT][VIDEO_MAX_CHANNELS];
extern void (*OnVideoInitBuffer)(guchar Mode);
extern EventCallback OnVideoStartRedraw;
extern void (*OnVideoWritePixel)(gushort x, gushort y, guchar r, guchar g, guchar b);
extern EventCallback OnVideoRefresh;
extern UpdateVUCallback OnVideoPowerCalculated;

gboolean GetVideo      (guchar Mode, double Rate, int Skip, gboolean Redraw);

#endif
