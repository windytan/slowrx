#ifndef _PIC_H_
#define _PIC_H_

#include <stdint.h>

#define PIC_TIMESTR_SZ (40)

typedef struct _PicMeta PicMeta;
struct _PicMeta {
  double Rate;
  int32_t Skip;
  int16_t HedrShift;
  uint8_t Mode;
  char   timestr[PIC_TIMESTR_SZ];
};
extern PicMeta CurrentPic;

#endif
