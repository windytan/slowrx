#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <glib.h>
#include <glib/gtypes.h>

extern void (*OnVideoInitBuffer)(guchar Mode);
extern EventCallback OnVideoStartRedraw;
extern void (*OnVideoWritePixel)(gushort x, gushort y, guchar r, guchar g, guchar b);
extern EventCallback OnVideoRefresh;
extern UpdateVUCallback OnVideoPowerCalculated;

gboolean GetVideo      (guchar Mode, double Rate, int Skip, gboolean Redraw);

#endif
