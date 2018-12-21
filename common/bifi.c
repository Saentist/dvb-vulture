#include "bifi.h"
#include "utl.h"
#include "dmalloc_loc.h"

void
bifiInit (bifi field, uint32_t size_bytes)
{
  uint32_t i;
  for (i = 0; i < size_bytes; i++)
  {
    field[i] = 0;
  }
}

uint8_t *
bifiAlloc (uint32_t size_in_bits)
{
  return utlCalloc ((size_in_bits + 7) >> 3, 1);
}

uint8_t
bifiGet (bifi field, uint32_t bit_idx)
{
  return field[bit_idx >> 3] & (1 << (bit_idx & 7));

}

void
bifiSet (bifi field, uint32_t bit_idx)
{
  field[bit_idx >> 3] |= (1 << (bit_idx & 7));
}

void
bifiClr (bifi field, uint32_t bit_idx)
{
  field[bit_idx >> 3] &= ~(1 << (bit_idx & 7));
}

void
bifiAnd (bifi dest, bifi a, bifi b, uint32_t size_bytes)
{
  uint32_t i;
  for (i = 0; i < size_bytes; i++)
  {
    dest[i] = a[i] & b[i];
  }
}

void
bifiOr (bifi dest, bifi a, bifi b, uint32_t size_bytes)
{
  uint32_t i;
  for (i = 0; i < size_bytes; i++)
  {
    dest[i] = a[i] | b[i];
  }
}

void
bifiXor (bifi dest, bifi a, bifi b, uint32_t size_bytes)
{
  uint32_t i;
  for (i = 0; i < size_bytes; i++)
  {
    dest[i] = a[i] ^ b[i];
  }
}

uint8_t
bifiIsNonzero (bifi field, uint32_t size_bytes)
{
  uint32_t i;
  uint8_t rv = 0;
  for (i = 0; i < size_bytes; i++)
  {
    rv |= field[i];
  }
  return rv;
}
