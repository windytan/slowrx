#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <stdint.h>
#include <stdbool.h>

extern void (*OnVideoInitBuffer)(uint8_t Mode);
extern EventCallback OnVideoStartRedraw;
extern void (*OnVideoWritePixel)(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b);
extern EventCallback OnVideoRefresh;
extern UpdateVUCallback OnVideoPowerCalculated;

_Bool GetVideo      (uint8_t Mode, double Rate, int32_t Skip, _Bool Redraw);

#endif
