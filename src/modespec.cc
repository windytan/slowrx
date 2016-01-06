#include "modespec.h"
#include <map>

/*
 * SSTV mode specifications
 * ========================
 *
 * name         Full human-readable mode identifier
 * short_name   Abbreviation to be used in filenames
 * num_lines    Total number of scanlines
 * header_lines Number of scanlines excluded from the aspect ratio
 * aspect_ratio Image aspect ratio
 * scan_pixels  Pixel samples per scanline and channel
 * t_sync       Duration of horizontal sync pulse
 * t_porch      Duration of sync porch
 * t_sep        Duration of channel separator + channel porch
 * t_scan       Duration of visible part of a channel scan (or Y scan if
 *              chroma subsampling is used)
 * t_period     Time from the beginning of a sync pulse to the beginning
 *              of the next one
 * sync_type    Positioning of sync pulse (SYNC_SIMPLE, SYNC_SCOTTIE, SYNC_PD)
 * subsampling  Chroma subsampling and channel order for YUV
 * color_enc    Color format (COLOR_GBR, COLOR_RGB, COLOR_YUV, COLOR_MONO)
 * vis_parity   Parity mode in VIS (PARITY_EVEN, PARITY_ODD)
 *
 *
 * All timings are in seconds.
 *
 * Sources:
 *
 *             JL Barber N7CXI (2000, "Proposal for SSTV Mode Specifications".
 *             Presented at the Dayton SSTV forum, 20 May 2000.
 *
 *             Dave Jones KB4YZ (1999, "SSTV modes - line timing".
 *             <http://www.tima.com/~djones/line.txt>
 *
 *             Dave Jones KB4YZ (1998, "List of SSTV Modes with VIS Codes".
 *             <http://www.tima.com/~djones/vis.txt>
 *
 *             Martin Bruchanov OK2MNM (2013, "Image Communication on Short
 *             Waves". <http://www.sstv-handbook.com>
 */

ModeSpec getModeSpec(SSTVMode mode) {

  std::map<SSTVMode, ModeSpec> spec = {
    {MODE_M1,    {"Martin M1", 320, 256, 16, 4.0/3.0, 4.862e-3, 0.572e-3, 0.572e-3,
                   146.432e-3, 446.446e-3, MODE_MARTIN, COLOR_GBR, PARITY_EVEN } },

    {MODE_M2,    {"Martin M2", 160, 256, 16, 4.0/3.0, 4.862e-3, 0.572e-3, 0.572e-3,
                   73.216e-3, 226.7980e-3, MODE_MARTIN, COLOR_GBR, PARITY_EVEN } },

    {MODE_M3,    {"Martin M3", 320, 128, 8, 4.0/3.0, 4.862e-3, 0.572e-3, 0.572e-3,
                   73.216e-3, 446.446e-3, MODE_MARTIN, COLOR_GBR, PARITY_EVEN } },

    {MODE_M4,    {"Martin M4", 160, 128, 8, 4.0/3.0, 4.862e-3, 0.572e-3, 0.572e-3,
                   73.216e-3, 226.7986e-3, MODE_MARTIN, COLOR_GBR, PARITY_EVEN } },

    {MODE_S1,    {"Scottie S1", 320, 256, 16, 4.0/3.0, 9e-3, 1.5e-3, 1.5e-3,
                   138.24e-3, 428.22e-3, MODE_SCOTTIE, COLOR_GBR, PARITY_EVEN } },

    {MODE_S2,    {"Scottie S2", 160, 256, 16, 4.0/3.0, 9e-3, 1.5e-3, 1.5e-3,
                   88.064e-3, 277.692e-3, MODE_SCOTTIE, COLOR_GBR, PARITY_EVEN } },

    {MODE_SDX,   {"Scottie DX", 320, 256, 16, 4.0/3.0, 9e-3, 1.5e-3, 1.5e-3,
                   345.6e-3, 1050.3e-3, MODE_SCOTTIE, COLOR_GBR, PARITY_EVEN } },

    {MODE_R72,   {"Robot 72", 320, 240, 0, 4.0/3.0, 9e-3, 3e-3, 6e-3,
                   138e-3, 300e-3, MODE_ROBOT, COLOR_YUV, PARITY_EVEN } },

    {MODE_R36,   {"Robot 36", 320, 240, 0, 4.0/3.0, 9e-3, 1.5e-3, 4.5e-3,
                   90e-3, 150e-3, MODE_ROBOT, COLOR_YUV, PARITY_EVEN } },

    {MODE_R24,   {"Robot 24", 160, 120, 0, 4.0/3.0, 9e-3, 0, 3e-3,
                   93e-3, 200e-3, MODE_ROBOT, COLOR_YUV, PARITY_EVEN } },

    {MODE_R36BW, {"Robot 36 B/W", 320, 240, 0, 4.0/3.0, 12e-3, 0, 0,
                   138e-3, 150e-3, MODE_ROBOTBW, COLOR_MONO, PARITY_EVEN } },

    {MODE_R24BW, {"Robot 24 B/W", 320, 240, 0, 4.0/3.0, 12e-3, 0, 0,
                   93e-3, 105e-3, MODE_ROBOTBW, COLOR_MONO, PARITY_EVEN } },

    {MODE_R12BW, {"Robot 12 B/W", 160, 120, 0, 4.0/3.0, 9e-3, 0, 0,
                   93e-3, 100e-3, MODE_ROBOTBW, COLOR_MONO, PARITY_ODD } },

    {MODE_R8BW,  {"Robot 8 B/W", 160, 120, 0, 4.0/3.0, 10e-3, 0, 0,
                   59e-3, 67e-3, MODE_ROBOTBW, COLOR_MONO, PARITY_EVEN } },

    {MODE_W260,  {"Wraase SC-2 60", 256, 256, 16, 4.0/3.0, 5.5225e-3, 0.5e-3, 0,
                   78.3e-3, 240.833878e-3, MODE_WRAASE2, COLOR_RGB, PARITY_EVEN } },

    {MODE_W2120, {"Wraase SC-2 120", 320, 256, 16, 4.0/3.0, 5.5225e-3, 0.5e-3, 0,
                   156.5025e-3, 475.52e-3, MODE_WRAASE2, COLOR_RGB, PARITY_EVEN } },

    {MODE_W2180, {"Wraase SC-2 180", 512, 256, 16, 4.0/3.0, 5.5225e-3, 0.5e-3, 0,
                   235e-3, 711.0437e-3, MODE_WRAASE2, COLOR_RGB, PARITY_EVEN } },

    {MODE_PD50,  {"PD-50", 320, 256, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   91.52e-3, 388.1586e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_PD90,  {"PD-90", 320, 256, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   170.340e-3, 703.04e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_PD120, {"PD-120", 320, 496, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   121.6e-3, 508.48e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_PD160, {"PD-160", 512, 400, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   195.584e-3, 804.416e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_PD180, {"PD-180", 640, 496, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   183.04e-3, 754.24e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_PD240, {"PD-240", 640, 496, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   244.48e-3, 1000e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_PD290, {"PD-290", 800, 616, 16, 4.0/3.0, 20e-3, 2.08e-3, 0,
                   228.8e-3, 937.28e-3, MODE_PD, COLOR_YUV, PARITY_EVEN } },

    {MODE_P3,    {"Pasokon P3", 320, 496, 16, 4.0/3.0, 5.208e-3, 1.042e-3, 1.042e-3,
                   133.333e-3, 409.3747e-3, MODE_PASOKON, COLOR_RGB, PARITY_EVEN } },

    {MODE_P5,    {"Pasokon P5", 640, 496, 16, 4.0/3.0, 7.813e-3, 1.563e-3, 1.563e-3,
                   200e-3, 614.065e-3, MODE_PASOKON, COLOR_RGB, PARITY_EVEN } },

    {MODE_P7,    {"Pasokon P7", 640, 496, 16, 4.0/3.0, 10.417e-3, 2.083e-3, 2.083e-3,
                   266.666e-3, 818.747e-3, MODE_PASOKON, COLOR_RGB, PARITY_EVEN } },

    {MODE_UNKNOWN,
      {} }
  };

  return spec[mode];
}

SSTVMode vis2mode (uint16_t vis) {

  std::map<int, SSTVMode> vismap = {
    {0x02,MODE_R8BW}, {0x04,MODE_R24},  {0x06,MODE_R12BW}, {0x08,MODE_R36},
    {0x0A,MODE_R24BW},{0x0C,MODE_R72},  {0x0E,MODE_R36BW}, {0x20,MODE_M4},    {0x24,MODE_M3},
    {0x28,MODE_M2},   {0x2C,MODE_M1},   {0x37,MODE_W2180}, {0x38,MODE_S2},
    {0x3C,MODE_S1},   {0x3F,MODE_W2120},{0x4C,MODE_SDX},   {0x5D,MODE_PD50},
    {0x5E,MODE_PD290},{0x5F,MODE_PD120},{0x60,MODE_PD180}, {0x61,MODE_PD240},
    {0x62,MODE_PD160},{0x63,MODE_PD90}, {0x71,MODE_P3},    {0x72,MODE_P5},
    {0x73,MODE_P7}
  };

  if ( vismap.find(vis) == vismap.end() )
    return MODE_UNKNOWN;
  else
    return vismap[vis];
}

