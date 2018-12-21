#ifndef __vtobj_h
#define __vtobj_h
#include <stdint.h>
#include "vtctx.h"
void vtobjParseX26 (VtCtx * ctx, uint8_t * pg, int num_pk_pg);
unsigned vtobjTripBits (unsigned trip, unsigned start, unsigned end);
#endif
