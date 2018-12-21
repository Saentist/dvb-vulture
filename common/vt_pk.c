#include "debug.h"
#include "vt_pk.h"
#include "in_out.h"
#include "ipc.h"

static uint8_t hammtab84_r[] = {
  0x01, 0xff, 0xff, 0x08, 0xff, 0x0c, 0x04, 0xff,
  0xff, 0x08, 0x08, 0x08, 0x06, 0xff, 0xff, 0x08,
  0xff, 0x0a, 0x02, 0xff, 0x06, 0xff, 0xff, 0x0f,
  0x06, 0xff, 0xff, 0x08, 0x06, 0x06, 0x06, 0xff,
  0xff, 0x0a, 0x04, 0xff, 0x04, 0xff, 0x04, 0x04,
  0x00, 0xff, 0xff, 0x08, 0xff, 0x0d, 0x04, 0xff,
  0x0a, 0x0a, 0xff, 0x0a, 0xff, 0x0a, 0x04, 0xff,
  0xff, 0x0a, 0x03, 0xff, 0x06, 0xff, 0xff, 0x0e,
  0x01, 0x01, 0x01, 0xff, 0x01, 0xff, 0xff, 0x0f,
  0x01, 0xff, 0xff, 0x08, 0xff, 0x0d, 0x05, 0xff,
  0x01, 0xff, 0xff, 0x0f, 0xff, 0x0f, 0x0f, 0x0f,
  0xff, 0x0b, 0x03, 0xff, 0x06, 0xff, 0xff, 0x0f,
  0x01, 0xff, 0xff, 0x09, 0xff, 0x0d, 0x04, 0xff,
  0xff, 0x0d, 0x03, 0xff, 0x0d, 0x0d, 0xff, 0x0d,
  0xff, 0x0a, 0x03, 0xff, 0x07, 0xff, 0xff, 0x0f,
  0x03, 0xff, 0x03, 0x03, 0xff, 0x0d, 0x03, 0xff,
  0xff, 0x0c, 0x02, 0xff, 0x0c, 0x0c, 0xff, 0x0c,
  0x00, 0xff, 0xff, 0x08, 0xff, 0x0c, 0x05, 0xff,
  0x02, 0xff, 0x02, 0x02, 0xff, 0x0c, 0x02, 0xff,
  0xff, 0x0b, 0x02, 0xff, 0x06, 0xff, 0xff, 0x0e,
  0x00, 0xff, 0xff, 0x09, 0xff, 0x0c, 0x04, 0xff,
  0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0x0e,
  0xff, 0x0a, 0x02, 0xff, 0x07, 0xff, 0xff, 0x0e,
  0x00, 0xff, 0xff, 0x0e, 0xff, 0x0e, 0x0e, 0x0e,
  0x01, 0xff, 0xff, 0x09, 0xff, 0x0c, 0x05, 0xff,
  0xff, 0x0b, 0x05, 0xff, 0x05, 0xff, 0x05, 0x05,
  0xff, 0x0b, 0x02, 0xff, 0x07, 0xff, 0xff, 0x0f,
  0x0b, 0x0b, 0xff, 0x0b, 0xff, 0x0b, 0x05, 0xff,
  0xff, 0x09, 0x09, 0x09, 0x07, 0xff, 0xff, 0x09,
  0x00, 0xff, 0xff, 0x09, 0xff, 0x0d, 0x05, 0xff,
  0x07, 0xff, 0xff, 0x09, 0x07, 0x07, 0x07, 0xff,
  0xff, 0x0b, 0x03, 0xff, 0x07, 0xff, 0xff, 0x0e,
};

uint8_t
hamm84_r (uint8_t v)
{
  return hammtab84_r[v];
}

uint8_t
parity8 (uint8_t v)
{
//      uint8_t orig=v;
  v = (v >> 4) ^ v;
  v = (v >> 2) ^ v;
  v = (v >> 1) ^ v;
//printf("parity %x -> %x\n",orig,v&1);
  return v & 1;
}

uint8_t
bitrev (uint8_t d)
{
  int i;
  uint8_t rv = 0;
  for (i = 0; i < 8; i++)
  {
    rv <<= 1;
    if (d & 1)
      rv |= 1;
    d >>= 1;
  }
//      debugMsg("bitrev %x -> %x\n",*d,rv);
  return rv;
}

uint8_t
get_pk_x (uint8_t * data)       //VtPk * d)
{
  int x;
  x = hamm84_r ( /*d-> */ data[2]);
  if (x != 0xff)
    x &= 7;
  return x;
}

uint8_t
get_pk_y (uint8_t * data)       //VtPk *d)
{
  int x, err;
  err = 0;
  x = (err = hamm84_r ( /*d-> */ data[2]));
  if (err == 0xff)
    return err;
  x = (x >> 3) & 1;
  x |= (err = hamm84_r ( /*d-> */ data[3])) << 1;
  if (err == 0xff)
    return err;
  return x;
}

uint8_t
get_pk_ext (uint8_t * data)     //VtPk *d)
{
  int x;
  x = hamm84_r ( /*d-> */ data[4]);
  return x;
}

uint8_t
get_magno (uint8_t * data)      //VtPk * d)
{
  uint8_t magno = get_pk_x (data);
//      debugMsg("magno: %x\n",magno);
  return magno;
}


///hmm.. page no 0xff may be used as a time filling header....
uint8_t
get_pgno (uint8_t * data)       //VtPk *  d)
{
  int tens, units;
  units = hamm84_r ( /*d-> */ data[4]);
  if (units == 0xff)
    return units;
  tens = hamm84_r ( /*d-> */ data[5]);
  if (tens == 0xff)
    return tens;
/*	if((units==0xff)||(tens==0xff))
		debugMsg("data error \n");*/

//      debugMsg("PAGE: %x%x\n",tens,units);
//      debugMsg("pgno: %x\n",units|(tens<<4));
  return units | (tens << 4);
}

int
has_c4 (uint8_t * data)
{
  int s2;
  s2 = hamm84_r (data[7]);
  if (s2 == 0xff)
    return 0;
  return (s2 >> 3) & 1;
}


uint16_t
get_subno (uint8_t * data)      //VtPk * d)
{
  int s1, s2, s3, s4;
  s1 = hamm84_r ( /*d-> */ data[6]);
  if (s1 == 0xff)
    return 0xffff;
  s2 = hamm84_r ( /*d-> */ data[7]);
  if (s2 == 0xff)
    return 0xffff;
  s2 = s2 & 7;
  s3 = hamm84_r ( /*d-> */ data[8]);
  if (s3 == 0xff)
    return 0xffff;
  s4 = hamm84_r ( /*d-> */ data[9]);
  if (s4 == 0xff)
    return 0xffff;
  s4 = s4 & 3;
//      debugMsg("subno: %x\n",s1|(s2<<4)|(s3<<8)|(s4<<12));
  return s1 | (s2 << 4) | (s3 << 8) | (s4 << 12);
}

uint8_t
is_svc_glob (uint8_t * data)    //VtPk * d)
{
  int y;
  y = get_pk_y (data);
  return ((y == 30) || (y == 31));
}

uint8_t
is_mag_glob (uint8_t * data)    //VtPk * d)
{
  return (get_pk_y (data) == 29);
}

uint8_t
is_pg_hdr (uint8_t * data)      //VtPk * d)
{
  return (get_pk_y (data) == 0);
}


// this table generates the parity checks for hamm24/18 decoding.
// bit 0 is for test A, 1 for B, ...
// thanks to R. Gancarz for this fine table *g*

static char hamm24par[3][256] = {
  {                             // parities of first byte
   0, 33, 34, 3, 35, 2, 1, 32, 36, 5, 6, 39, 7, 38, 37, 4,
   37, 4, 7, 38, 6, 39, 36, 5, 1, 32, 35, 2, 34, 3, 0, 33,
   38, 7, 4, 37, 5, 36, 39, 6, 2, 35, 32, 1, 33, 0, 3, 34,
   3, 34, 33, 0, 32, 1, 2, 35, 39, 6, 5, 36, 4, 37, 38, 7,
   39, 6, 5, 36, 4, 37, 38, 7, 3, 34, 33, 0, 32, 1, 2, 35,
   2, 35, 32, 1, 33, 0, 3, 34, 38, 7, 4, 37, 5, 36, 39, 6,
   1, 32, 35, 2, 34, 3, 0, 33, 37, 4, 7, 38, 6, 39, 36, 5,
   36, 5, 6, 39, 7, 38, 37, 4, 0, 33, 34, 3, 35, 2, 1, 32,
   40, 9, 10, 43, 11, 42, 41, 8, 12, 45, 46, 15, 47, 14, 13, 44,
   13, 44, 47, 14, 46, 15, 12, 45, 41, 8, 11, 42, 10, 43, 40, 9,
   14, 47, 44, 13, 45, 12, 15, 46, 42, 11, 8, 41, 9, 40, 43, 10,
   43, 10, 9, 40, 8, 41, 42, 11, 15, 46, 45, 12, 44, 13, 14, 47,
   15, 46, 45, 12, 44, 13, 14, 47, 43, 10, 9, 40, 8, 41, 42, 11,
   42, 11, 8, 41, 9, 40, 43, 10, 14, 47, 44, 13, 45, 12, 15, 46,
   41, 8, 11, 42, 10, 43, 40, 9, 13, 44, 47, 14, 46, 15, 12, 45,
   12, 45, 46, 15, 47, 14, 13, 44, 40, 9, 10, 43, 11, 42, 41, 8}, {     // parities of second byte
                                                                   0, 41, 42,
                                                                   3, 43, 2,
                                                                   1, 40, 44,
                                                                   5, 6, 47,
                                                                   7, 46, 45,
                                                                   4,
                                                                   45, 4, 7,
                                                                   46, 6, 47,
                                                                   44, 5, 1,
                                                                   40, 43, 2,
                                                                   42, 3, 0,
                                                                   41,
                                                                   46, 7, 4,
                                                                   45, 5, 44,
                                                                   47, 6, 2,
                                                                   43, 40, 1,
                                                                   41, 0, 3,
                                                                   42,
                                                                   3, 42, 41,
                                                                   0, 40, 1,
                                                                   2, 43, 47,
                                                                   6, 5, 44,
                                                                   4, 45, 46,
                                                                   7,
                                                                   47, 6, 5,
                                                                   44, 4, 45,
                                                                   46, 7, 3,
                                                                   42, 41, 0,
                                                                   40, 1, 2,
                                                                   43,
                                                                   2, 43, 40,
                                                                   1, 41, 0,
                                                                   3, 42, 46,
                                                                   7, 4, 45,
                                                                   5, 44, 47,
                                                                   6,
                                                                   1, 40, 43,
                                                                   2, 42, 3,
                                                                   0, 41, 45,
                                                                   4, 7, 46,
                                                                   6, 47, 44,
                                                                   5,
                                                                   44, 5, 6,
                                                                   47, 7, 46,
                                                                   45, 4, 0,
                                                                   41, 42, 3,
                                                                   43, 2, 1,
                                                                   40,
                                                                   48, 25, 26,
                                                                   51, 27, 50,
                                                                   49, 24, 28,
                                                                   53, 54, 31,
                                                                   55, 30, 29,
                                                                   52,
                                                                   29, 52, 55,
                                                                   30, 54, 31,
                                                                   28, 53, 49,
                                                                   24, 27, 50,
                                                                   26, 51, 48,
                                                                   25,
                                                                   30, 55, 52,
                                                                   29, 53, 28,
                                                                   31, 54, 50,
                                                                   27, 24, 49,
                                                                   25, 48, 51,
                                                                   26,
                                                                   51, 26, 25,
                                                                   48, 24, 49,
                                                                   50, 27, 31,
                                                                   54, 53, 28,
                                                                   52, 29, 30,
                                                                   55,
                                                                   31, 54, 53,
                                                                   28, 52, 29,
                                                                   30, 55, 51,
                                                                   26, 25, 48,
                                                                   24, 49, 50,
                                                                   27,
                                                                   50, 27, 24,
                                                                   49, 25, 48,
                                                                   51, 26, 30,
                                                                   55, 52, 29,
                                                                   53, 28, 31,
                                                                   54,
                                                                   49, 24, 27,
                                                                   50, 26, 51,
                                                                   48, 25, 29,
                                                                   52, 55, 30,
                                                                   54, 31, 28,
                                                                   53,
                                                                   28, 53, 54, 31, 55, 30, 29, 52, 48, 25, 26, 51, 27, 50, 49, 24}, {   // parities of third byte
                                                                                                                                     63,
                                                                                                                                     14,
                                                                                                                                     13,
                                                                                                                                     60,
                                                                                                                                     12,
                                                                                                                                     61,
                                                                                                                                     62,
                                                                                                                                     15,
                                                                                                                                     11,
                                                                                                                                     58,
                                                                                                                                     57,
                                                                                                                                     8,
                                                                                                                                     56,
                                                                                                                                     9,
                                                                                                                                     10,
                                                                                                                                     59,
                                                                                                                                     10,
                                                                                                                                     59,
                                                                                                                                     56,
                                                                                                                                     9,
                                                                                                                                     57,
                                                                                                                                     8,
                                                                                                                                     11,
                                                                                                                                     58,
                                                                                                                                     62,
                                                                                                                                     15,
                                                                                                                                     12,
                                                                                                                                     61,
                                                                                                                                     13,
                                                                                                                                     60,
                                                                                                                                     63,
                                                                                                                                     14,
                                                                                                                                     9,
                                                                                                                                     56,
                                                                                                                                     59,
                                                                                                                                     10,
                                                                                                                                     58,
                                                                                                                                     11,
                                                                                                                                     8,
                                                                                                                                     57,
                                                                                                                                     61,
                                                                                                                                     12,
                                                                                                                                     15,
                                                                                                                                     62,
                                                                                                                                     14,
                                                                                                                                     63,
                                                                                                                                     60,
                                                                                                                                     13,
                                                                                                                                     60,
                                                                                                                                     13,
                                                                                                                                     14,
                                                                                                                                     63,
                                                                                                                                     15,
                                                                                                                                     62,
                                                                                                                                     61,
                                                                                                                                     12,
                                                                                                                                     8,
                                                                                                                                     57,
                                                                                                                                     58,
                                                                                                                                     11,
                                                                                                                                     59,
                                                                                                                                     10,
                                                                                                                                     9,
                                                                                                                                     56,
                                                                                                                                     8,
                                                                                                                                     57,
                                                                                                                                     58,
                                                                                                                                     11,
                                                                                                                                     59,
                                                                                                                                     10,
                                                                                                                                     9,
                                                                                                                                     56,
                                                                                                                                     60,
                                                                                                                                     13,
                                                                                                                                     14,
                                                                                                                                     63,
                                                                                                                                     15,
                                                                                                                                     62,
                                                                                                                                     61,
                                                                                                                                     12,
                                                                                                                                     61,
                                                                                                                                     12,
                                                                                                                                     15,
                                                                                                                                     62,
                                                                                                                                     14,
                                                                                                                                     63,
                                                                                                                                     60,
                                                                                                                                     13,
                                                                                                                                     9,
                                                                                                                                     56,
                                                                                                                                     59,
                                                                                                                                     10,
                                                                                                                                     58,
                                                                                                                                     11,
                                                                                                                                     8,
                                                                                                                                     57,
                                                                                                                                     62,
                                                                                                                                     15,
                                                                                                                                     12,
                                                                                                                                     61,
                                                                                                                                     13,
                                                                                                                                     60,
                                                                                                                                     63,
                                                                                                                                     14,
                                                                                                                                     10,
                                                                                                                                     59,
                                                                                                                                     56,
                                                                                                                                     9,
                                                                                                                                     57,
                                                                                                                                     8,
                                                                                                                                     11,
                                                                                                                                     58,
                                                                                                                                     11,
                                                                                                                                     58,
                                                                                                                                     57,
                                                                                                                                     8,
                                                                                                                                     56,
                                                                                                                                     9,
                                                                                                                                     10,
                                                                                                                                     59,
                                                                                                                                     63,
                                                                                                                                     14,
                                                                                                                                     13,
                                                                                                                                     60,
                                                                                                                                     12,
                                                                                                                                     61,
                                                                                                                                     62,
                                                                                                                                     15,
                                                                                                                                     31,
                                                                                                                                     46,
                                                                                                                                     45,
                                                                                                                                     28,
                                                                                                                                     44,
                                                                                                                                     29,
                                                                                                                                     30,
                                                                                                                                     47,
                                                                                                                                     43,
                                                                                                                                     26,
                                                                                                                                     25,
                                                                                                                                     40,
                                                                                                                                     24,
                                                                                                                                     41,
                                                                                                                                     42,
                                                                                                                                     27,
                                                                                                                                     42,
                                                                                                                                     27,
                                                                                                                                     24,
                                                                                                                                     41,
                                                                                                                                     25,
                                                                                                                                     40,
                                                                                                                                     43,
                                                                                                                                     26,
                                                                                                                                     30,
                                                                                                                                     47,
                                                                                                                                     44,
                                                                                                                                     29,
                                                                                                                                     45,
                                                                                                                                     28,
                                                                                                                                     31,
                                                                                                                                     46,
                                                                                                                                     41,
                                                                                                                                     24,
                                                                                                                                     27,
                                                                                                                                     42,
                                                                                                                                     26,
                                                                                                                                     43,
                                                                                                                                     40,
                                                                                                                                     25,
                                                                                                                                     29,
                                                                                                                                     44,
                                                                                                                                     47,
                                                                                                                                     30,
                                                                                                                                     46,
                                                                                                                                     31,
                                                                                                                                     28,
                                                                                                                                     45,
                                                                                                                                     28,
                                                                                                                                     45,
                                                                                                                                     46,
                                                                                                                                     31,
                                                                                                                                     47,
                                                                                                                                     30,
                                                                                                                                     29,
                                                                                                                                     44,
                                                                                                                                     40,
                                                                                                                                     25,
                                                                                                                                     26,
                                                                                                                                     43,
                                                                                                                                     27,
                                                                                                                                     42,
                                                                                                                                     41,
                                                                                                                                     24,
                                                                                                                                     40,
                                                                                                                                     25,
                                                                                                                                     26,
                                                                                                                                     43,
                                                                                                                                     27,
                                                                                                                                     42,
                                                                                                                                     41,
                                                                                                                                     24,
                                                                                                                                     28,
                                                                                                                                     45,
                                                                                                                                     46,
                                                                                                                                     31,
                                                                                                                                     47,
                                                                                                                                     30,
                                                                                                                                     29,
                                                                                                                                     44,
                                                                                                                                     29,
                                                                                                                                     44,
                                                                                                                                     47,
                                                                                                                                     30,
                                                                                                                                     46,
                                                                                                                                     31,
                                                                                                                                     28,
                                                                                                                                     45,
                                                                                                                                     41,
                                                                                                                                     24,
                                                                                                                                     27,
                                                                                                                                     42,
                                                                                                                                     26,
                                                                                                                                     43,
                                                                                                                                     40,
                                                                                                                                     25,
                                                                                                                                     30,
                                                                                                                                     47,
                                                                                                                                     44,
                                                                                                                                     29,
                                                                                                                                     45,
                                                                                                                                     28,
                                                                                                                                     31,
                                                                                                                                     46,
                                                                                                                                     42,
                                                                                                                                     27,
                                                                                                                                     24,
                                                                                                                                     41,
                                                                                                                                     25,
                                                                                                                                     40,
                                                                                                                                     43,
                                                                                                                                     26,
                                                                                                                                     43,
                                                                                                                                     26,
                                                                                                                                     25,
                                                                                                                                     40,
                                                                                                                                     24,
                                                                                                                                     41,
                                                                                                                                     42,
                                                                                                                                     27,
                                                                                                                                     31,
                                                                                                                                     46,
                                                                                                                                     45,
                                                                                                                                     28,
                                                                                                                                     44,
                                                                                                                                     29,
                                                                                                                                     30,
                                                                                                                                     47}
};



// table to extract the lower 4 bit from hamm24/18 encoded bytes

static char hamm24val[256] = {
  0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
  2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 2, 3, 3, 3, 3,
  4, 4, 4, 4, 5, 5, 5, 5, 4, 4, 4, 4, 5, 5, 5, 5,
  6, 6, 6, 6, 7, 7, 7, 7, 6, 6, 6, 6, 7, 7, 7, 7,
  8, 8, 8, 8, 9, 9, 9, 9, 8, 8, 8, 8, 9, 9, 9, 9,
  10, 10, 10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11, 11, 11,
  12, 12, 12, 12, 13, 13, 13, 13, 12, 12, 12, 12, 13, 13, 13, 13,
  14, 14, 14, 14, 15, 15, 15, 15, 14, 14, 14, 14, 15, 15, 15, 15,
  0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
  2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 2, 3, 3, 3, 3,
  4, 4, 4, 4, 5, 5, 5, 5, 4, 4, 4, 4, 5, 5, 5, 5,
  6, 6, 6, 6, 7, 7, 7, 7, 6, 6, 6, 6, 7, 7, 7, 7,
  8, 8, 8, 8, 9, 9, 9, 9, 8, 8, 8, 8, 9, 9, 9, 9,
  10, 10, 10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11, 11, 11,
  12, 12, 12, 12, 13, 13, 13, 13, 12, 12, 12, 12, 13, 13, 13, 13,
  14, 14, 14, 14, 15, 15, 15, 15, 14, 14, 14, 14, 15, 15, 15, 15
};



// mapping from parity checks made by table hamm24par to error
// results return by hamm24.
// (0 = no error, 0x0100 = single bit error, 0x1000 = double error)

static short hamm24err[64] = {
  0x0000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
  0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
  0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
  0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
  0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
  0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
  0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
  0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
};



// mapping from parity checks made by table hamm24par to faulty bit
// in the decoded 18 bit word.

static int hamm24cor[64] = {
  0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
  0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
  0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
  0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
  0x00000, 0x00000, 0x00000, 0x00001, 0x00000, 0x00002, 0x00004, 0x00008,
  0x00000, 0x00010, 0x00020, 0x00040, 0x00080, 0x00100, 0x00200, 0x00400,
  0x00000, 0x00800, 0x01000, 0x02000, 0x04000, 0x08000, 0x10000, 0x20000,
  0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
};


/*
int
hamm16(uint8_t *p, int *err)
{
    int a = hammtab[p[0]];
    int b = hammtab[p[1]];
    *err += a;
    *err += b;
    return (a & 15) | (b & 15) * 16;
}
*/
int
hamm24 (uint8_t * p, int *err)
{
  int e = hamm24par[0][p[0]] ^ hamm24par[1][p[1]] ^ hamm24par[2][p[2]];
  int x = hamm24val[p[0]] + p[1] % 128 * 16 + p[2] % 128 * 2048;

  *err += hamm24err[e];
  return x ^ hamm24cor[e];
}

#define BAD_CHAR -1

int
chk_parity (uint8_t * p, int n)
{
  int err;

  for (err = 0; n--; p++)
    if (hamm24par[0][*p] & 32)
      *p &= 0x7f;
    else
      *p = BAD_CHAR, err++;
  return err;
}

int
vtpkRcv (int fd, VtPk * p)
{
  return (ipcRcvS (fd, p->pid) ||
          ipcRcvS (fd, p->data_id) ||
          ipcRcvS (fd, p->data_unit_id) ||
          ipcRcvS (fd, p->data_unit_len) ||
          (VT_UNIT_LEN != ioBlkRd (fd, p->data, VT_UNIT_LEN)));
}

int
vtpkSnd (int fd, VtPk * p)
{
  return (ipcSndS (fd, p->pid) ||
          ipcSndS (fd, p->data_id) ||
          ipcSndS (fd, p->data_unit_id) ||
          ipcSndS (fd, p->data_unit_len) ||
          (VT_UNIT_LEN != ioBlkWr (fd, p->data, VT_UNIT_LEN)));
}
