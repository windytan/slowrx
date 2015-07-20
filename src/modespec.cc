#include <gtkmm.h>

#include <fftw3.h>

#include "common.h"

/*
 * Mode specifications
 *
 * Name          Long, human-readable name for the mode
 * ShortName     Abbreviation for the mode, used in filenames
 * SyncTime      Duration of synchronization pulse in seconds
 * PorchTime     Duration of sync porch pulse in seconds
 * SeptrTime     Duration of channel separator pulse in seconds
 * PixelTime     Duration of one pixel in seconds
 * LineTime      Time in seconds from the beginning of a sync pulse to the beginning of the next one
 * ImgWidth      Pixels per scanline
 * NumLines      Number of scanlines
 * LineHeight    Height of one scanline in pixels (1 or 2)
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

_ModeSpec ModeSpec[] = {

  [M1] = {  // N7CXI, 2000
    .Name         = "Martin M1",
    .ShortName    = "M1",
    .SyncTime     = 4.862e-3,
    .PorchTime    = 0.572e-3,
    .SeptrTime    = 0.572e-3,
    .PixelTime    = 0.4576e-3,
    .LineTime     = 446.446e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = GBR },

  [M2] = {  // N7CXI, 2000
    .Name         = "Martin M2",
    .ShortName    = "M2",
    .SyncTime     = 4.862e-3,
    .PorchTime    = 0.572e-3,
    .SeptrTime    = 0.572e-3,
    .PixelTime    = 0.2288e-3,
    .LineTime     = 226.7986e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = GBR },

  [M3] = {   // KB4YZ, 1999
    .Name         = "Martin M3",
    .ShortName    = "M3",
    .SyncTime     = 4.862e-3,
    .PorchTime    = 0.572e-3,
    .SeptrTime    = 0.572e-3,
    .PixelTime    = 0.2288e-3,
    .LineTime     = 446.446e-3,
    .ImgWidth     = 320,
    .NumLines     = 128,
    .LineHeight   = 2,
    .ColorEnc     = GBR },

  [M4] = {   // KB4YZ, 1999
    .Name         = "Martin M4",
    .ShortName    = "M4",
    .SyncTime     = 4.862e-3,
    .PorchTime    = 0.572e-3,
    .SeptrTime    = 0.572e-3,
    .PixelTime    = 0.2288e-3,
    .LineTime     = 226.7986e-3,
    .ImgWidth     = 320,
    .NumLines     = 128,
    .LineHeight   = 2,
    .ColorEnc     = GBR },

  [S1] = {  // N7CXI, 2000
    .Name         = "Scottie S1",
    .ShortName    = "S1",
    .SyncTime     = 9e-3,
    .PorchTime    = 1.5e-3,
    .SeptrTime    = 1.5e-3,
    .PixelTime    = 0.4320e-3,
    .LineTime     = 428.38e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = GBR },

  [S2] = {  // N7CXI, 2000
    .Name         = "Scottie S2",
    .ShortName    = "S2",
    .SyncTime     = 9e-3,
    .PorchTime    = 1.5e-3,
    .SeptrTime    = 1.5e-3,
    .PixelTime    = 0.2752e-3,
    .LineTime     = 277.692e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = GBR },

  [SDX] = {  // N7CXI, 2000
    .Name         = "Scottie DX",
    .ShortName    = "SDX",
    .SyncTime     = 9e-3,
    .PorchTime    = 1.5e-3,
    .SeptrTime    = 1.5e-3,
    .PixelTime    = 1.08053e-3,
    .LineTime     = 1050.3e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = GBR },

  [R72] = {  // N7CXI, 2000
    .Name         = "Robot 72",
    .ShortName    = "R72",
    .SyncTime     = 9e-3,
    .PorchTime    = 3e-3,
    .SeptrTime    = 4.7e-3,
    .PixelTime    = 0.2875e-3,
    .LineTime     = 300e-3,
    .ImgWidth     = 320,
    .NumLines     = 240,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [R36] = {  // N7CXI, 2000
    .Name         = "Robot 36",
    .ShortName    = "R36",
    .SyncTime     = 9e-3,
    .PorchTime    = 3e-3,
    .SeptrTime    = 6e-3,
    .PixelTime    = 0.1375e-3,
    .LineTime     = 150e-3,
    .ImgWidth     = 320,
    .NumLines     = 240,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [R24] = {  // N7CXI, 2000
    .Name         = "Robot 24",
    .ShortName    = "R24",
    .SyncTime     = 9e-3,
    .PorchTime    = 3e-3,
    .SeptrTime    = 6e-3,
    .PixelTime    = 0.1375e-3,
    .LineTime     = 150e-3,
    .ImgWidth     = 320,
    .NumLines     = 240,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [R24BW] = {  // N7CXI, 2000
    .Name         = "Robot 24 B/W",
    .ShortName    = "R24BW",
    .SyncTime     = 7e-3,
    .PorchTime    = 0e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.291e-3,
    .LineTime     = 100e-3,
    .ImgWidth     = 320,
    .NumLines     = 240,
    .LineHeight   = 1,
    .ColorEnc     = BW },

  [R12BW] = {  // N7CXI, 2000
    .Name         = "Robot 12 B/W",
    .ShortName    = "R12BW",
    .SyncTime     = 7e-3,
    .PorchTime    = 0e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.291e-3,
    .LineTime     = 100e-3,
    .ImgWidth     = 320,
    .NumLines     = 120,
    .LineHeight   = 2,
    .ColorEnc     = BW },

  [R8BW] = {  // N7CXI, 2000
    .Name         = "Robot 8 B/W",
    .ShortName    = "R8BW",
    .SyncTime     = 7e-3,
    .PorchTime    = 0e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.188e-3,
    .LineTime     = 67e-3,
    .ImgWidth     = 320,
    .NumLines     = 120,
    .LineHeight   = 2,
    .ColorEnc     = BW },
  
  [W2120] = { // KB4YZ, 1999
    .Name         = "Wraase SC-2 120",
    .ShortName    = "W2120",
    .SyncTime     = 5.5225e-3,
    .PorchTime    = 0.5e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.489039081e-3,
    .LineTime     = 475.530018e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = RGB },

  [W2180] = {  // N7CXI, 2000
    .Name         = "Wraase SC-2 180",
    .ShortName    = "W2180",
    .SyncTime     = 5.5225e-3,
    .PorchTime    = 0.5e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.734532e-3,
    .LineTime     = 711.0225e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = RGB },

  [PD50] = {  // N7CXI, 2000
    .Name         = "PD-50",
    .ShortName    = "PD50",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.286e-3,
    .LineTime     = 388.16e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [PD90] = {  // N7CXI, 2000
    .Name         = "PD-90",
    .ShortName    = "PD90",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.532e-3,
    .LineTime     = 703.04e-3,
    .ImgWidth     = 320,
    .NumLines     = 256,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [PD120] = {  // N7CXI, 2000
    .Name         = "PD-120",
    .ShortName    = "PD120",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.19e-3,
    .LineTime     = 508.48e-3,
    .ImgWidth     = 640,
    .NumLines     = 496,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [PD160] = {  // N7CXI, 2000
    .Name         = "PD-160",
    .ShortName    = "PD160",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.382e-3,
    .LineTime     = 804.416e-3,
    .ImgWidth     = 512,
    .NumLines     = 400,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [PD180] = {  // N7CXI, 2000
    .Name         = "PD-180",
    .ShortName    = "PD180",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.286e-3,
    .LineTime     = 754.24e-3,
    .ImgWidth     = 640,
    .NumLines     = 496,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [PD240] = {  // N7CXI, 2000
    .Name         = "PD-240",
    .ShortName    = "PD240",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.382e-3,
    .LineTime     = 1000e-3,
    .ImgWidth     = 640,
    .NumLines     = 496,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [PD290] = {  // N7CXI, 2000
    .Name         = "PD-290",
    .ShortName    = "PD290",
    .SyncTime     = 20e-3,
    .PorchTime    = 2.08e-3,
    .SeptrTime    = 0e-3,
    .PixelTime    = 0.286e-3,
    .LineTime     = 937.28e-3,
    .ImgWidth     = 800,
    .NumLines     = 616,
    .LineHeight   = 1,
    .ColorEnc     = YUV },

  [P3] = {  // N7CXI, 2000
    .Name         = "Pasokon P3",
    .ShortName    = "P3",
    .SyncTime     = 5.208e-3,
    .PorchTime    = 1.042e-3,
    .SeptrTime    = 1.042e-3,
    .PixelTime    = 0.2083e-3,
    .LineTime     = 409.375e-3,
    .ImgWidth     = 640,
    .NumLines     = 496,
    .LineHeight   = 1,
    .ColorEnc     = RGB },

  [P5] = {  // N7CXI, 2000
    .Name         = "Pasokon P5",
    .ShortName    = "P5",
    .SyncTime     = 7.813e-3,
    .PorchTime    = 1.563e-3,
    .SeptrTime    = 1.563e-3,
    .PixelTime    = 0.3125e-3,
    .LineTime     = 614.065e-3,
    .ImgWidth     = 640,
    .NumLines     = 496,
    .LineHeight   = 1,
    .ColorEnc     = RGB },

  [P7] = {  // N7CXI, 2000
    .Name         = "Pasokon P7",
    .ShortName    = "P7",
    .SyncTime     = 10.417e-3,
    .PorchTime    = 2.083e-3,
    .SeptrTime    = 2.083e-3,
    .PixelTime    = 0.4167e-3,
    .LineTime     = 818.747e-3,
    .ImgWidth     = 640,
    .NumLines     = 496,
    .LineHeight   = 1,
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
