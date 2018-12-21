#include "bits.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"

int DF_BITS;
#define FILE_DBG DF_BITS

uint8_t
bitsDecodeBcd8 (uint8_t bcd)
{
  return (bcd & 0x0f) + (10 * (bcd >> 4));
}

/*
  -- get bits out of buffer  (max 48 bit)
  -- extended bitrange, so it's slower
  -- return: value
 */

long long
bitsGet48 (uint8_t * buf, int byte_offset, int startbit, int bitlen)
{
  uint8_t *b;
  unsigned i;
  unsigned max_byte;
  unsigned long long v;
  unsigned long long mask;
  unsigned long long tmp;

  if (bitlen > 48)
  {
    errMsg (" Error: getBits48() request out of bound!!!! (report!!) \n");
    return 0xFEFEFEFEFEFEFEFELL;
  }


  b = &buf[byte_offset + (startbit / 8)];
  startbit %= 8;

  max_byte = 56 - startbit - bitlen;
  max_byte /= 8;
  max_byte = 7 - max_byte;
  tmp = 0;
  //calc max address to avoid segfaults
  for (i = 0; i < max_byte; i++)
  {
    tmp |= ((unsigned long long) *(b + i) << (48 - i * 8));
  }
  // -- safe is 48 bitlen 
  //this segfaults.. seems the allocator unmapped a block right after this allocation
  //the *(b + n) for higher n are out of bounds
  //but it's technically valid for small bitlengths etc...
//  tmp = (unsigned long long) (((unsigned long long) *(b) << 48) +
//                              ((unsigned long long) *(b + 1) << 40) +
//                              ((unsigned long long) *(b + 2) << 32) +
//                              ((unsigned long long) *(b + 3) << 24) +
//                              (*(b + 4) << 16) + (*(b + 5) << 8) + *(b + 6));

  startbit = 56 - startbit - bitlen;
  tmp = tmp >> startbit;
  mask = (1ULL << bitlen) - 1;  // 1ULL !!!
  v = tmp & mask;

  return v;
}

unsigned long
bitsGet (uint8_t * buf, int byte_offset, int startbit, int bitlen)
{
  uint8_t *b;
  unsigned i;
  unsigned max_byte;
  unsigned long v;
  unsigned long mask;
  unsigned long tmp_long;
  int bitHigh;


  b = &buf[byte_offset + (startbit >> 3)];
  startbit %= 8;

  switch ((bitlen - 1) >> 3)
  {
  case -1:                     // -- <=0 bits: always 0
    return 0L;
    break;

  case 0:                      // -- 1..8 bit
    bitHigh = 16;
    break;

  case 1:                      // -- 9..16 bit
    bitHigh = 24;
    break;

  case 2:                      // -- 17..24 bit
    bitHigh = 32;
    break;

  case 3:                      // -- 25..32 bit
    // -- to be safe, we need 32+8 bit as shift range 
    return (unsigned long) bitsGet48 (b, 0, startbit, bitlen);
    break;
  default:                     // -- 33.. bits: fail, deliver constant fail value
    errMsg (" Error: getBits() request out of bound!!!! (report!!) \n");
    return (unsigned long) 0xFEFEFEFE;
    break;
  }

  max_byte = bitHigh - startbit - bitlen;
  max_byte /= 8;
  max_byte = bitHigh / 8 - max_byte;
  tmp_long = 0;
  //calc max address to avoid segfaults
  for (i = 0; i < max_byte; i++)
  {
    tmp_long |= ((unsigned long long) *(b + i) << (bitHigh - 8 - i * 8));
  }

  startbit = bitHigh - startbit - bitlen;
  tmp_long = tmp_long >> startbit;
  mask = (1ULL << bitlen) - 1;  // 1ULL !!!
  v = tmp_long & mask;

  return v;
}
