#include <stdint.h>
uint16_t VT_CHARS[4 * (11 * 96 + 13 * 13) + 2 * 96 + 96][10] = {
#include "vt_fonts/chars.c"
  ,
#include "vt_fonts/mosaics.c"
};
