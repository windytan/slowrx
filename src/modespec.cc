#include "modespec.h"
#include <vector>

/*
 * SSTV mode specifications
 * ========================
 *
 * All timings given in milliseconds.
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

std::pair<bool,ModeSpec> vis2mode(uint16_t vis) {

  // {id, "Name", pixels, lines, header, t_sync, t_porch, t_chansep, t_chanporch,
  //  t_scan, t_period, family, color, vis, parity }

  std::vector<ModeSpec> modes({
    {"Martin M1", 320, 256, 16, 4.862, 0.572, 0.572, 0.0, 446.446, 146.432, MODE_MARTIN, COLOR_GBR, 0x2C, PARITY_EVEN },
    {"Martin M2", 160, 256, 16, 4.862, 0.572, 0.572, 0.0, 226.7980, 73.216, MODE_MARTIN, COLOR_GBR, 0x28, PARITY_EVEN },
  /*{"Martin M3", 320, 128,  8, 4.862, 0.572, 0.572, 0.0, 446.446,  73.216, MODE_MARTIN, COLOR_GBR, 0x24, PARITY_EVEN },
    {"Martin M4", 160, 128,  8, 4.862, 0.572, 0.572, 0.0, 226.7986, 73.216, MODE_MARTIN, COLOR_GBR, 0x20, PARITY_EVEN },*/

    {"Scottie S1", 320, 256, 16, 9.0, 1.5, 1.5, 0.0,  428.22,  138.24,  MODE_SCOTTIE, COLOR_GBR, 0x3C, PARITY_EVEN },
    {"Scottie S2", 160, 256, 16, 9.0, 1.5, 1.5, 0.0,  277.692,  88.064, MODE_SCOTTIE, COLOR_GBR, 0x38, PARITY_EVEN },
    {"Scottie DX", 320, 256, 16, 9.0, 1.5, 1.5, 0.0, 1050.3,   345.6,   MODE_SCOTTIE, COLOR_GBR, 0x4C, PARITY_EVEN },

    {"Robot 72", 320, 240, 0, 9.0, 3.0, 4.5, 1.5, 300.0, 138.0, MODE_ROBOT, COLOR_YUV422, 0x0C, PARITY_EVEN },
    {"Robot 36", 320, 240, 0, 9.0, 3.0, 4.5, 1.5, 150.0,  88.0, MODE_ROBOT, COLOR_YUV420, 0x08, PARITY_EVEN },
    {"Robot 24", 160, 120, 0, 9.0, 0.0, 2.0, 1.5, 200.0,  93.0, MODE_ROBOT, COLOR_YUV422, 0x04, PARITY_EVEN },

    {"Robot 36 B/W", 320, 240, 0, 12.0, 0.0, 0.0, 0.0, 150.0,   140.0, MODE_ROBOTBW, COLOR_MONO, 0x0E, PARITY_EVEN },
    {"Robot 24 B/W", 320, 240, 0, 12.0, 0.0, 0.0, 0.0, 105.0,    93.0, MODE_ROBOTBW, COLOR_MONO, 0x0A, PARITY_EVEN },
    {"Robot 12 B/W", 160, 120, 0,  7.0, 1.5, 0.0, 0.0, 100.0,    93.0, MODE_ROBOTBW, COLOR_MONO, 0x06, PARITY_ODD },
    {"Robot 8 B/W",  160, 120, 0, 10.0, 0.0, 0.0, 0.0,  66.8966, 59.0, MODE_ROBOTBW, COLOR_MONO, 0x02, PARITY_EVEN },

    {"Wraase SC-2 60",  256, 256, 16, 5.5225, 0.5, 0.0, 0.0, 240.833878, 78.3,    MODE_WRAASE2, COLOR_RGB, 0x3B, PARITY_EVEN },
    {"Wraase SC-2 120", 320, 256, 16, 5.5225, 0.5, 0.0, 0.0, 475.52,    156.5025, MODE_WRAASE2, COLOR_RGB, 0x3F, PARITY_EVEN },
    {"Wraase SC-2 180", 512, 256, 16, 5.5225, 0.5, 0.0, 0.0, 711.0437,  235.0,    MODE_WRAASE2, COLOR_RGB, 0x37, PARITY_EVEN },

    {"PD-50",  320, 256, 16, 20.0, 2.08, 0.0, 0.0,  388.1586,   91.52,  MODE_PD, COLOR_YUV420, 0x5D, PARITY_EVEN },
    {"PD-90",  320, 256, 16, 20.0, 2.08, 0.0, 0.0,  703.04124, 170.240, MODE_PD, COLOR_YUV420, 0x63, PARITY_EVEN },
    {"PD-120", 320, 496, 16, 20.0, 2.08, 0.0, 0.0,  508.48,    121.6,   MODE_PD, COLOR_YUV420, 0x5F, PARITY_EVEN },
    {"PD-160", 512, 400, 16, 20.0, 2.08, 0.0, 0.0,  804.416,   195.584, MODE_PD, COLOR_YUV420, 0x62, PARITY_EVEN },
    {"PD-180", 640, 496, 16, 20.0, 2.08, 0.0, 0.0,  754.24,    183.04,  MODE_PD, COLOR_YUV420, 0x60, PARITY_EVEN },
    {"PD-240", 640, 496, 16, 20.0, 2.08, 0.0, 0.0, 1000.0,     244.48,  MODE_PD, COLOR_YUV420, 0x61, PARITY_EVEN },
    {"PD-290", 800, 616, 16, 20.0, 2.08, 0.0, 0.0,  937.28,    228.8,   MODE_PD, COLOR_YUV420, 0x5E, PARITY_EVEN },

    {"Pasokon P3", 320, 496, 16,  5.208, 1.042, 1.042, 0.0, 409.3747, 133.333, MODE_PASOKON, COLOR_RGB, 0x71, PARITY_EVEN },
    {"Pasokon P5", 640, 496, 16,  7.813, 1.563, 1.563, 0.0, 614.065,  200.0,   MODE_PASOKON, COLOR_RGB, 0x72, PARITY_EVEN },
    {"Pasokon P7", 640, 496, 16, 10.417, 2.083, 2.083, 0.0, 818.747,  266.666, MODE_PASOKON, COLOR_RGB, 0x73, PARITY_EVEN }
  });


  bool found = false;
  ModeSpec mode;
  for (ModeSpec m : modes) {
    if (m.vis == vis) {
      found = true;
      mode = m;
      break;
    }
  }

  return {found, mode};
}
