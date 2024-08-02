#ifndef _COMMON_H_
#define _COMMON_H_

#define MINSLANT 30
#define MAXSLANT 150
#define BUFLEN   5120
#define SYNCPIXLEN 1.5e-3

#include <stdint.h>
#include <stdbool.h>
#include <complex.h>

extern _Bool   Abort;
extern _Bool   Adaptive;
extern _Bool  *HasSync;
extern _Bool   ManualActivated;
extern _Bool   ManualResync;
extern uint8_t *StoredLum;

// Various callback function types for various events.
typedef void (*EventCallback)(void);
typedef void (*TextStatusCallback)(const char* status);
typedef void (*UpdateVUCallback)(double* Power, int FFTLen, int WinIdx);

double   power     (double complex coeff);
uint8_t  clip      (double a);
double   deg2rad   (double Deg);
uint32_t GetBin    (double Freq, uint32_t FFTLen);

void     ensure_dir_exists(const char *dir);

#endif
