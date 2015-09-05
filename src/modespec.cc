#include "modespec.h"
#include <map>

/*
 * SSTV mode specifications
 * ========================
 *
 * name         Full human-readable mode identifier
 * short_name   Abbreviation to be used in filenames
 * num_lines    Total number of scanlines
 * header_lines Number of scanlines excluded from 4:3 ratio
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
 *             JL Barber N7CXI (2000): "Proposal for SSTV Mode Specifications".
 *             Presented at the Dayton SSTV forum, 20 May 2000.
 *
 *             Dave Jones KB4YZ (1999): "SSTV modes - line timing".
 *             <http://www.tima.com/~djones/line.txt>
 *
 *             Dave Jones KB4YZ (1998): "List of SSTV Modes with VIS Codes".
 *             <http://www.tima.com/~djones/vis.txt>
 *
 *             Martin Bruchanov OK2MNM (2013): "Image Communication on Short
 *             Waves". <http://www.sstv-handbook.com>
 */

_ModeSpec ModeSpec[]   = {

  [MODE_M1]   = {  // OK
    .name         = "Martin M1",
    .short_name   = "M1",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_sync       = 4.862e-3,
    .t_porch      = 0.572e-3,
    .t_sep        = 0.572e-3,
    .t_scan       = 146.432e-3,
    .t_period     = 446.446e-3,
    .family       = MODE_MARTIN,
    .color_enc    = COLOR_GBR,
    .vis          = 0x2C,
    .vis_parity   = PARITY_EVEN },

  [MODE_M2]   = {  // OK
    .name         = "Martin M2",
    .short_name   = "M2",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 73.216e-3,
    .t_period     = 226.7980e-3,
    .t_sync       = 4.862e-3,
    .t_porch      = 0.572e-3,
    .t_sep        = 0.572e-3,
    .family       = MODE_MARTIN,
    .color_enc    = COLOR_GBR,
    .vis          = 0x28,
    .vis_parity   = PARITY_EVEN },

  [MODE_M3]   = {   // TODO
    .name         = "Martin M3",
    .short_name   = "M3",
    .scan_pixels  = 320,
    .num_lines    = 128,
    .header_lines = 8,
    .t_scan       = 73.216e-3,
    .t_period     = 446.446e-3,
    .t_sync       = 4.862e-3,
    .t_porch      = 0.572e-3,
    .t_sep        = 0.572e-3,
    .family       = MODE_MARTIN,
    .color_enc    = COLOR_GBR,
    .vis          = 0x24,
    .vis_parity   = PARITY_EVEN },

  [MODE_M4]   = {   // TODO
    .name         = "Martin M4",
    .short_name   = "M4",
    .scan_pixels  = 320,
    .num_lines    = 128,
    .header_lines = 8,
    .t_scan       = 73.216e-3,
    .t_period     = 226.7986e-3,
    .t_sync       = 4.862e-3,
    .t_porch      = 0.572e-3,
    .t_sep        = 0.572e-3,
    .family       = MODE_MARTIN,
    .color_enc    = COLOR_GBR,
    .vis          = 0x20,
    .vis_parity   = PARITY_EVEN },

  [MODE_S1]   = {  // OK
    .name         = "Scottie S1",
    .short_name   = "S1",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_sync       = 9e-3,
    .t_porch      = 1.5e-3,
    .t_sep        = 1.5e-3,
    .t_scan       = 138.24e-3,
    .t_period     = 428.22e-3,
    .family       = MODE_SCOTTIE,
    .color_enc    = COLOR_GBR,
    .vis          = 0x3C,
    .vis_parity   = PARITY_EVEN },

  [MODE_S2]   = {  // OK
    .name         = "Scottie S2",
    .short_name   = "S2",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 88.064e-3,
    .t_period     = 277.692e-3,
    .t_sync       = 9e-3,
    .t_porch      = 1.5e-3,
    .t_sep        = 1.5e-3,
    .family       = MODE_SCOTTIE,
    .color_enc    = COLOR_GBR,
    .vis          = 0x38,
    .vis_parity   = PARITY_EVEN },

  [MODE_SDX]   = {  // OK
    .name         = "Scottie DX",
    .short_name   = "SDX",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 345.6e-3,
    .t_period     = 1050.3e-3,
    .t_sync       = 9e-3,
    .t_porch      = 1.5e-3,
    .t_sep        = 1.5e-3,
    .family       = MODE_SCOTTIE,
    .color_enc    = COLOR_GBR,
    .vis          = 0x4C,
    .vis_parity   = PARITY_EVEN },

  [MODE_R72]   = {  // OK
    .name         = "Robot 72",
    .short_name   = "R72",
    .scan_pixels  = 320,
    .num_lines    = 240,
    .header_lines = 0,
    .t_scan       = 138e-3,
    .t_period     = 300e-3,
    .t_sync       = 9e-3,
    .t_porch      = 3e-3,
    .t_sep        = 6e-3,
    .family       = MODE_ROBOT,
    .color_enc    = COLOR_YUV,
    .vis          = 0x0C,
    .vis_parity   = PARITY_EVEN },

  [MODE_R36]   = {  // OK
    .name         = "Robot 36",
    .short_name   = "R36",
    .scan_pixels  = 320,
    .num_lines    = 240,
    .header_lines = 0,
    .t_scan       = 90e-3,
    .t_period     = 150e-3,
    .t_sync       = 9e-3,
    .t_porch      = 1.5e-3,
    .t_sep        = 4.5e-3,
    .family       = MODE_ROBOT,
    .color_enc    = COLOR_YUV,
    .vis          = 0x08,
    .vis_parity   = PARITY_EVEN },

  [MODE_R24]   = {  // OK
    .name         = "Robot 24",
    .short_name   = "R24",
    .scan_pixels  = 160,
    .num_lines    = 120,
    .header_lines = 0,
    .t_scan       = 93e-3,
    .t_period     = 200e-3,
    .t_sync       = 9e-3,
    .t_porch      = 0,
    .t_sep        = 3e-3,
    .family       = MODE_ROBOT,
    .color_enc    = COLOR_YUV,
    .vis          = 0x04,
    .vis_parity   = PARITY_EVEN },

  [MODE_R24BW]   = {  // TODO
    .name         = "Robot 24 B/W",
    .short_name   = "R24BW",
    .scan_pixels  = 320,
    .num_lines    = 240,
    .header_lines = 0,
    .t_scan       = 93e-3,
    .t_period     = 100e-3,
    .t_sync       = 7e-3,
    .t_porch      = 0,
    .t_sep        = 0,
    .family       = MODE_ROBOT,
    .color_enc    = COLOR_MONO,
    .vis          = 0x0A,
    .vis_parity   = PARITY_EVEN },

  [MODE_R12BW]   = {  // OK
    .name         = "Robot 12 B/W",
    .short_name   = "R12BW",
    .scan_pixels  = 160,
    .num_lines    = 120,
    .header_lines = 0,
    .t_scan       = 93e-3,
    .t_period     = 100e-3,
    .t_sync       = 9e-3,
    .t_porch      = 0,
    .t_sep        = 0,
    .family       = MODE_ROBOT,
    .color_enc    = COLOR_MONO,
    .vis          = 0x06,
    .vis_parity   = PARITY_ODD },

  [MODE_R8BW]   = {  // OK
    .name         = "Robot 8 B/W",
    .short_name   = "R8BW",
    .scan_pixels  = 160,
    .num_lines    = 120,
    .header_lines = 0,
    .t_scan       = 59e-3,
    .t_period     = 67e-3,
    .t_sync       = 10e-3,
    .t_porch      = 0,
    .t_sep        = 0,
    .family       = MODE_ROBOT,
    .color_enc    = COLOR_MONO,
    .vis          = 0x02,
    .vis_parity   = PARITY_EVEN },

  [MODE_W260]   = { // OK
    .name         = "Wraase SC-2 60",
    .short_name   = "SC2_60",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 78.3e-3,
    .t_period     = 240.833878e-3,
    .t_sync       = 5.5225e-3,
    .t_porch      = 0.5e-3,
    .t_sep        = 0,
    .family       = MODE_WRAASE2,
    .color_enc    = COLOR_RGB,
    .vis          = 0x3B,
    .vis_parity   = PARITY_EVEN },


  [MODE_W2120]   = { // OK
    .name         = "Wraase SC-2 120",
    .short_name   = "SC2_120",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 156.5025e-3,
    .t_period     = 475.52e-3,
    .t_sync       = 5.5225e-3,
    .t_porch      = 0.5e-3,
    .t_sep        = 0,
    .family       = MODE_WRAASE2,
    .color_enc    = COLOR_RGB,
    .vis          = 0x3F,
    .vis_parity   = PARITY_EVEN },

  [MODE_W2180]   = {  // OK
    .name         = "Wraase SC-2 180",
    .short_name   = "SC2_180",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 235e-3,
    .t_period     = 711.0437e-3,
    .t_sync       = 5.5225e-3,
    .t_porch      = 0.5e-3,
    .t_sep        = 0,
    .family       = MODE_WRAASE2,
    .color_enc    = COLOR_RGB,
    .vis          = 0x37,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD50]   = {  // OK
    .name         = "PD-50",
    .short_name   = "PD50",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 91.52e-3,
    .t_period     = 388.1586e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x5D,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD90]   = {  // TODO
    .name         = "PD-90",
    .short_name   = "PD90",
    .scan_pixels  = 320,
    .num_lines    = 256,
    .header_lines = 16,
    .t_scan       = 170.340e-3,
    .t_period     = 703.04e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x63,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD120]   = {  // OK
    .name         = "PD-120",
    .short_name   = "PD120",
    .scan_pixels  = 640,
    .num_lines    = 496,
    .header_lines = 16,
    .t_scan       = 121.6e-3,
    .t_period     = 508.48e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x5F,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD160]   = {  // OK
    .name         = "PD-160",
    .short_name   = "PD160",
    .scan_pixels  = 512,
    .num_lines    = 400,
    .header_lines = 16,
    .t_scan       = 195.584e-3,
    .t_period     = 804.416e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x62,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD180]   = { // OK
    .name         = "PD-180",
    .short_name   = "PD180",
    .scan_pixels  = 640,
    .num_lines    = 496,
    .header_lines = 16,
    .t_scan       = 183.04e-3,
    .t_period     = 754.24e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x60,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD240]   = {  // OK
    .name         = "PD-240",
    .short_name   = "PD240",
    .scan_pixels  = 640,
    .num_lines    = 496,
    .header_lines = 16,
    .t_scan       = 244.48e-3,
    .t_period     = 1000e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x61,
    .vis_parity   = PARITY_EVEN },

  [MODE_PD290]   = {  // OK
    .name         = "PD-290",
    .short_name   = "PD290",
    .scan_pixels  = 800,
    .num_lines    = 616,
    .header_lines = 16,
    .t_scan       = 228.8e-3,
    .t_period     = 937.28e-3,
    .t_sync       = 20e-3,
    .t_porch      = 2.08e-3,
    .t_sep        = 0,
    .family       = MODE_PD,
    .color_enc    = COLOR_YUV,
    .vis          = 0x5E,
    .vis_parity   = PARITY_EVEN },

  [MODE_P3]   = {  // OK
    .name         = "Pasokon P3",
    .short_name   = "P3",
    .scan_pixels  = 640,
    .num_lines    = 496,
    .header_lines = 16,
    .t_scan       = 133.333e-3,
    .t_period     = 409.3747e-3,
    .t_sync       = 5.208e-3,
    .t_porch      = 1.042e-3,
    .t_sep        = 1.042e-3,
    .family       = MODE_PASOKON,
    .color_enc    = COLOR_RGB,
    .vis          = 0x71,
    .vis_parity   = PARITY_EVEN },

  [MODE_P5]   = {  // OK
    .name         = "Pasokon P5",
    .short_name   = "P5",
    .scan_pixels  = 640,
    .num_lines    = 496,
    .header_lines = 16,
    .t_scan       = 200e-3,
    .t_period     = 614.065e-3,
    .t_sync       = 7.813e-3,
    .t_porch      = 1.563e-3,
    .t_sep        = 1.563e-3,
    .family       = MODE_PASOKON,
    .color_enc    = COLOR_RGB,
    .vis          = 0x72,
    .vis_parity   = PARITY_EVEN },

  [MODE_P7]   = {  // OK
    .name         = "Pasokon P7",
    .short_name   = "P7",
    .scan_pixels  = 640,
    .num_lines    = 496,
    .header_lines = 16,
    .t_scan       = 266.666e-3,
    .t_period     = 818.747e-3,
    .t_sync       = 10.417e-3,
    .t_porch      = 2.083e-3,
    .t_sep        = 2.083e-3,
    .family       = MODE_PASOKON,
    .color_enc    = COLOR_RGB,
    .vis          = 0x73,
    .vis_parity   = PARITY_EVEN }
 
};

SSTVMode vis2mode (int vis) {
  std::map<int, SSTVMode> vismap = {
    {0x02,MODE_R8BW}, {0x04,MODE_R24},  {0x06,MODE_R12BW}, {0x08,MODE_R36},
    {0x0A,MODE_R24BW},{0x0C,MODE_R72},  {0x20,MODE_M4},    {0x24,MODE_M3},
    {0x28,MODE_M2},   {0x2C,MODE_M1},   {0x37,MODE_W2180}, {0x38,MODE_S2},
    {0x3C,MODE_S1},   {0x3F,MODE_W2120},{0x4C,MODE_SDX},   {0x5D,MODE_PD50},
    {0x5E,MODE_PD290},{0x5F,MODE_PD120},{0x60,MODE_PD180}, {0x61,MODE_PD240},
    {0x62,MODE_PD160},{0x63,MODE_PD90}, {0x71,MODE_P3},    {0x72,MODE_P5},
    {0x73,MODE_P7}
  };
return vismap[vis];
}
