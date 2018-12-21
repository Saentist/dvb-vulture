#ifndef __pas_h
#define __pas_h
#include <stdint.h>
#include "sec_common.h"
#include "fp_mm.h"

/**
 *\file pas.h
 *\brief Programme Association Section parser
 */

//typedef struct Table_s Table;

///one program association. Associates pnr with PMT pid.
typedef struct
{
  uint16_t pnr;
  uint16_t pid;
} PA;

typedef struct PAS_s PAS;
///one programme association section
struct PAS_s
{
  SecCommon c;
  uint16_t tsid;
  uint16_t pa_count;
  PA *pas;
};

int parse_pas (uint8_t * pat, PAS * p);
void clear_pas (PAS * p);
int write_pas (PAS * p, FILE * f);
int read_pas (PAS * p, FILE * f);

#endif
