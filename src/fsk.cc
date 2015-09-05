#include "common.h"

/* 
 *
 * Decode FSK ID
 *
 * * The FSK IDs are 6-bit bytes, LSB first, 45.45 baud (22 ms/bit), 1900 Hz = 1, 2100 Hz = 0
 * * Text data starts with 20 2A and ends in 01
 * * Add 0x20 and the data becomes ASCII
 *
 */

string GetFSK (DSPworker *dsp) {

  char *dest;
  guint  i=0, test_num=0, test_ptr=0;
  guchar bit = 0, ascii_byte = 0, byte_ptr = 0, test_bits[24] = {0}, bit_ptr=0;
  bool   is_in_sync = false;

  // bit-reversion lookup table
  static const guint8 bit_rev[] = {
    0x00, 0x20, 0x10, 0x30,   0x08, 0x28, 0x18, 0x38,
    0x04, 0x24, 0x14, 0x34,   0x0c, 0x2c, 0x1c, 0x3c,
    0x02, 0x22, 0x12, 0x32,   0x0a, 0x2a, 0x1a, 0x3a,
    0x06, 0x26, 0x16, 0x36,   0x0e, 0x2e, 0x1e, 0x3e,
    0x01, 0x21, 0x11, 0x31,   0x09, 0x29, 0x19, 0x39,
    0x05, 0x25, 0x15, 0x35,   0x0d, 0x2d, 0x1d, 0x3d,
    0x03, 0x23, 0x13, 0x33,   0x0b, 0x2b, 0x1b, 0x3b,
    0x07, 0x27, 0x17, 0x37,   0x0f, 0x2f, 0x1f, 0x3f };

  while ( true ) {

    vector<double> bands = dsp->getBandPowerPerHz({{1850,1950},{2050,2150}});
    
    bit = (bands[0] > bands[1]) ? 1 : 0;

    if (!is_in_sync) {

      // Wait for 20 2A
      
      test_bits[test_ptr % 24] = bit;

      test_num = 0;
      for (i=0; i<12; i++) test_num |= test_bits[(test_ptr - (23-i*2)) % 24] << (11-i);

      if (bit_rev[(test_num >>  6) & 0x3f] == 0x20 && bit_rev[test_num & 0x3f] == 0x2a) {
        is_in_sync    = true;
        ascii_byte = 0;
        bit_ptr    = 0;
        byte_ptr   = 0;
      }

      if (++test_ptr > 200) break;

    } else {

      ascii_byte |= bit << bit_ptr;

      if (++bit_ptr == 6) {
        if (ascii_byte < 0x0d || byte_ptr > 9) break;
        dest[byte_ptr] = ascii_byte + 0x20;
        bit_ptr        = 0;
        ascii_byte     = 0;
        byte_ptr ++;
      }
    }

  }
    
  dest[byte_ptr] = '\0';

}
