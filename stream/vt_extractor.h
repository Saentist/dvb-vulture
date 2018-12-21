#ifndef __vt_extractor_h
#define __vt_extractor_h
#include <stdint.h>
#include "btre.h"
#include "vt_pk.h"
/**
 *\file vt_extractor.h
 *\brief extracts vt packets from pes data
 *
 *calls user defined function for every packet
 *
 *TODO: use seperate extractors for every pid, so we do not need state lookup/creation for every packet.
 */
typedef struct
{
  uint8_t *scratch;             ///<scratch buffer for assembling complete PES packets
  int scratch_size;             ///<buffer size, reallocate if too small
  int scratch_idx;              ///<index into scratch buffer
  uint16_t pid;
//      VtPk buf;///<output buffer. \see vt_pk.h
//      int idx;
  uint8_t st;                   ///<state
  uint16_t plen;                ///<length without header
} VtExState;


typedef struct
{
  BTreeNode *states;
  void *x;
  void (*process) (void *x, VtPk * buf);
} VtEx;

int vtExInit (VtEx * ex, void *x, void (*process) (void *x, VtPk * buf));
void vtExClear (VtEx * ex);
void vtExPut (VtEx * ex, uint8_t * buf, uint16_t pid, uint8_t len,
              uint8_t starts);
void vtExFlush (VtEx * ex);

#endif
