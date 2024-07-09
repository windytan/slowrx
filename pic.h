#ifndef _PIC_H_
#define _PIC_H_

#include <glib.h>
#include <glib/gtypes.h>

typedef struct _PicMeta PicMeta;
struct _PicMeta {
  gshort HedrShift;
  guchar Mode;
  double Rate;
  int    Skip;
  char   timestr[40];
};
extern PicMeta CurrentPic;

#endif
