#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <gtk/gtk.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>

#include "common.h"

/*
 * Mode specifications
 *
 * Name          Long, human-readable name for the mode
 * ShortName     Abbreviation for the mode, used in filenames
 * SyncLen       Duration of synchronization pulse in seconds
 * PorchLen      Duration of sync porch pulse in seconds
 * SeparatorLen  Duration of channel separator pulse in seconds
 * PixelLen      Duration of one pixel in seconds
 * LineLen       Time in seconds from the beginning of a sync pulse to the beginning of the next one
 * ImgWidth      Pixels per scanline
 * ImgHeight     Number of scanlines
 * YScale        Height of one scanline in pixels (1 or 2)
 * ColorEnc      Color format (GBR, RGB, YUV, BW)
 *
 *
 * Note that these timings do not fully describe the workings of the different modes.
 *
 * References: 
 *             
 *             JL Barber N7CXI (2000): "Proposal for SSTV Mode Specifications". Presented at the
 *             Dayton SSTV forum, 20 May 2000.
 *
 *             Dave Jones KB4YZ (1999): "SSTV modes - line timing".
 *             <http://www.tima.com/~djones/line.txt>
 */

ModeSpecDef ModeSpec[] = {

  [M1] = {  // N7CXI, 2000
    .Name         = "Martin M1",
    .ShortName    = "M1",
    .SyncLen      = 4.862e-3,
    .PorchLen     = 0.572e-3,
    .SeparatorLen = 0.572e-3,
    .PixelLen     = 0.4576e-3,
    .LineLen      = 446.446e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = GBR },

  [M2] = {  // N7CXI, 2000
    .Name         = "Martin M2",
    .ShortName    = "M2",
    .SyncLen      = 4.862e-3,
    .PorchLen     = 0.572e-3,
    .SeparatorLen = 0.572e-3,
    .PixelLen     = 0.2288e-3,
    .LineLen      = 226.7986e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = GBR },

  [M3] = {   // KB4YZ, 1999
    .Name         = "Martin M3",
    .ShortName    = "M3",
    .SyncLen      = 4.862e-3,
    .PorchLen     = 0.572e-3,
    .SeparatorLen = 0.572e-3,
    .PixelLen     = 0.2288e-3,
    .LineLen      = 446.446e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 128,
    .YScale       = 2,
    .ColorEnc     = GBR },

  [M4] = {   // KB4YZ, 1999
    .Name         = "Martin M4",
    .ShortName    = "M4",
    .SyncLen      = 4.862e-3,
    .PorchLen     = 0.572e-3,
    .SeparatorLen = 0.572e-3,
    .PixelLen     = 0.2288e-3,
    .LineLen      = 226.7986e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 128,
    .YScale       = 2,
    .ColorEnc     = GBR },

  [S1] = {  // N7CXI, 2000
    .Name         = "Scottie S1",
    .ShortName    = "S1",
    .SyncLen      = 9e-3,
    .PorchLen     = 1.5e-3,
    .SeparatorLen = 1.5e-3,
    .PixelLen     = 0.4320e-3,
    .LineLen      = 428.38e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = GBR },

  [S2] = {  // N7CXI, 2000
    .Name         = "Scottie S2",
    .ShortName    = "S2",
    .SyncLen      = 9e-3,
    .PorchLen     = 1.5e-3,
    .SeparatorLen = 1.5e-3,
    .PixelLen     = 0.2752e-3,
    .LineLen      = 277.692e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = GBR },

  [SDX] = {  // N7CXI, 2000
    .Name         = "Scottie DX",
    .ShortName    = "SDX",
    .SyncLen      = 9e-3,
    .PorchLen     = 1.5e-3,
    .SeparatorLen = 1.5e-3,
    .PixelLen     = 1.08053e-3,
    .LineLen      = 1050.3e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = GBR },

  [R72] = {  // N7CXI, 2000
    .Name         = "Robot 72",
    .ShortName    = "R72",
    .SyncLen      = 9e-3,
    .PorchLen     = 3e-3,
    .SeparatorLen = 4.7e-3,
    .PixelLen     = 0.2875e-3,
    .LineLen      = 300e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 240,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [R36] = {  // N7CXI, 2000
    .Name         = "Robot 36",
    .ShortName    = "R36",
    .SyncLen      = 9e-3,
    .PorchLen     = 3e-3,
    .SeparatorLen = 6e-3,
    .PixelLen     = 0.1375e-3,
    .LineLen      = 150e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 240,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [R24] = {  // N7CXI, 2000
    .Name         = "Robot 24",
    .ShortName    = "R24",
    .SyncLen      = 9e-3,
    .PorchLen     = 3e-3,
    .SeparatorLen = 6e-3,
    .PixelLen     = 0.1375e-3,
    .LineLen      = 150e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 240,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [R24BW] = {  // N7CXI, 2000
    .Name         = "Robot 24 B/W",
    .ShortName    = "R24BW",
    .SyncLen      = 7e-3,
    .PorchLen     = 0e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = .291e-3,
    .LineLen      = 100e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 240,
    .YScale       = 1,
    .ColorEnc     = BW },

  [R12BW] = {  // N7CXI, 2000
    .Name         = "Robot 12 B/W",
    .ShortName    = "R12BW",
    .SyncLen      = 7e-3,
    .PorchLen     = 0e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = .291e-3,
    .LineLen      = 100e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 120,
    .YScale       = 2,
    .ColorEnc     = BW },

  [R8BW] = {  // N7CXI, 2000
    .Name         = "Robot 8 B/W",
    .ShortName    = "R8BW",
    .SyncLen      = 7e-3,
    .PorchLen     = 0e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = .188e-3,
    .LineLen      = 67e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 120,
    .YScale       = 2,
    .ColorEnc     = BW },
  
  [W2120] = { // KB4YZ, 1999
    .Name         = "Wraase SC-2 120",
    .ShortName    = "W2120",
    .SyncLen      = 5.5225e-3,
    .PorchLen     = 0.5e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.489039081e-3,
    .LineLen      = 475.530018e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = RGB },

  [W2180] = {  // N7CXI, 2000
    .Name         = "Wraase SC-2 180",
    .ShortName    = "W2180",
    .SyncLen      = 5.5225e-3,
    .PorchLen     = 0.5e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.734532e-3,
    .LineLen      = 711.0225e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = RGB },

  [PD50] = {  // N7CXI, 2000
    .Name         = "PD-50",
    .ShortName    = "PD50",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.286e-3,
    .LineLen      = 388.16e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [PD90] = {  // N7CXI, 2000
    .Name         = "PD-90",
    .ShortName    = "PD90",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.532e-3,
    .LineLen      = 703.04e-3,
    .ImgWidth     = 320,
    .ImgHeight    = 256,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [PD120] = {  // N7CXI, 2000
    .Name         = "PD-120",
    .ShortName    = "PD120",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.19e-3,
    .LineLen      = 508.48e-3,
    .ImgWidth     = 640,
    .ImgHeight    = 496,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [PD160] = {  // N7CXI, 2000
    .Name         = "PD-160",
    .ShortName    = "PD160",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.382e-3,
    .LineLen      = 804.416e-3,
    .ImgWidth     = 512,
    .ImgHeight    = 400,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [PD180] = {  // N7CXI, 2000
    .Name         = "PD-180",
    .ShortName    = "PD180",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.286e-3,
    .LineLen      = 754.24e-3,
    .ImgWidth     = 640,
    .ImgHeight    = 496,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [PD240] = {  // N7CXI, 2000
    .Name         = "PD-240",
    .ShortName    = "PD240",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.382e-3,
    .LineLen      = 1000e-3,
    .ImgWidth     = 640,
    .ImgHeight    = 496,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [PD290] = {  // N7CXI, 2000
    .Name         = "PD-290",
    .ShortName    = "PD290",
    .SyncLen      = 20e-3,
    .PorchLen     = 2.08e-3,
    .SeparatorLen = 0e-3,
    .PixelLen     = 0.286e-3,
    .LineLen      = 937.28e-3,
    .ImgWidth     = 800,
    .ImgHeight    = 616,
    .YScale       = 1,
    .ColorEnc     = YUV },

  [P3] = {  // N7CXI, 2000
    .Name         = "Pasokon P3",
    .ShortName    = "P3",
    .SyncLen      = 5.208e-3,
    .PorchLen     = 1.042e-3,
    .SeparatorLen = 1.042e-3,
    .PixelLen     = 0.2083e-3,
    .LineLen      = 409.375e-3,
    .ImgWidth     = 640,
    .ImgHeight    = 496,
    .YScale       = 1,
    .ColorEnc     = RGB },

  [P5] = {  // N7CXI, 2000
    .Name         = "Pasokon P5",
    .ShortName    = "P5",
    .SyncLen      = 7.813e-3,
    .PorchLen     = 1.563e-3,
    .SeparatorLen = 1.563e-3,
    .PixelLen     = 0.3125e-3,
    .LineLen      = 614.065e-3,
    .ImgWidth     = 640,
    .ImgHeight    = 496,
    .YScale       = 1,
    .ColorEnc     = RGB },

  [P7] = {  // N7CXI, 2000
    .Name         = "Pasokon P7",
    .ShortName    = "P7",
    .SyncLen      = 10.417e-3,
    .PorchLen     = 2.083e-3,
    .SeparatorLen = 2.083e-3,
    .PixelLen     = 0.4167e-3,
    .LineLen      = 818.747e-3,
    .ImgWidth     = 640,
    .ImgHeight    = 496,
    .YScale       = 1,
    .ColorEnc     = RGB }
 
};

/*
 * Mapping of 7-bit VIS codes to modes
 *
 * Reference: Dave Jones KB4YZ (1998): "List of SSTV Modes with VIS Codes".
 *            http://www.tima.com/~djones/vis.txt
 *
 */

//                  0     1     2     3    4     5     6     7     8     9     A     B    C    D    E     F

guchar VISmap[] = { 0,    0,    R8BW, 0,   R24,  0,    R12BW,0,    R36,  0,    R24BW,0,   R72, 0,   0,    0,     // 0
                    0,    0,    0,    0,   0,    0,    0,    0,    0,    0,    0,    0,   0,   0,   0,    0,     // 1
                    M4,   0,    0,    0,   M3,   0,    0,    0,    M2,   0,    0,    0,   M1,  0,   0,    0,     // 2
                    0,    0,    0,    0,   0,    0,    0,    W2180,S2,   0,    0,    0,   S1,  0,   0,    W2120, // 3
                    0,    0,    0,    0,   0,    0,    0,    0,    0,    0,    0,    0,   SDX, 0,   0,    0,     // 4
                    0,    0,    0,    0,   0,    0,    0,    0,    0,    0,    0,    0,   0,   PD50,PD290,PD120, // 5
                    PD180,PD240,PD160,PD90,0,    0,    0,    0,    0,    0,    0,    0,   0,   0,   0,    0,     // 6
                    0,    P3,   P5,   P7,  0,    0,    0,    0,    0,    0,    0,    0,   0,   0,   0,    0 };   // 7

//                  0     1     2     3    4     5     6     7     8     9     A     B    C    D    E     F
