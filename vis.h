#ifndef _VIS_H_
#define _VIS_H_

#include <stdint.h>
#include <stdbool.h>

extern TextStatusCallback OnVisStatusChange;
extern EventCallback OnVisIdentified;
extern UpdateVUCallback OnVisPowerComputed;
extern uint8_t VIS;
extern _Bool VisAutoStart;

uint8_t GetVIS();

#endif
