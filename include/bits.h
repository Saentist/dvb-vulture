#ifndef __bits_h
#define __bits_h
#include <stdint.h>
/**
  -- get bits out of buffer  (max 48 bit)
  -- extended bitrange, so it's slower
  -- return: value
  (stolen from dvbsnoop)
 */
long long bitsGet48 (uint8_t * buf, int byte_offset, int startbit,
                     int bitlen);


/**
 *(stolen from dvbsnoop)
 */
unsigned long bitsGet (uint8_t * buf, int byte_offset, int startbit,
                       int bitlen);
/**
 *\brief reverses 16 bit endianness
 *
 *may not work on big endian archs
uint16_t bitsSwap16(uint16_t v);
 *\brief reverses 32 bit endianness
 *
 *may not work on big endians archs
uint32_t bitsSwap32(uint32_t v);
*/

uint8_t bitsDecodeBcd8 (uint8_t bcd);
#endif
