#ifndef __size_stack
#define __size_stack
#include <stdint.h>
/**
 *\brief sizes stack
 *
 *this structure is used to bounds-check nested size constraints
 *used mostly for ts table parsing
 *each table has a table_size. this is the top level constraint and would typically form the base of the stack.
 *At some point a nested constraint may show up. this could be a descriptor length, an ES_info loop length or similar.
 *this is pushed onto the stack and checked against the higher level boundaries(table_size etc..)
 *at each step pointer will be size-checked against it's bounds.
 *A failure indicates corrupted data and parsing may be aborted. This avoids out of bounds memory accesses.
 */
typedef struct
{
  uint8_t num_bounds;
  uint8_t *start[32];
  uint8_t *end[32];
  uint8_t *ptr;
} SizeStack;

int sizeStackInit (SizeStack * s, uint8_t * ptr);

uint8_t *sizeStackPtr (SizeStack * s);

/**
 *\brief removes topmost constraint
 *
 *@return 0
 */
int sizeStackPop (SizeStack * s);

/**
 *\brief pushes another size constraint onto the stack
 *
 *Corresponding index is set to zero
 *
 *@return 1 if area exceeds previous one.
 */
int sizeStackPush (SizeStack * s, uint32_t size);

/**
 *\brief call this everytime you advance a pointer
 *
 *@param dist the distance to advance the pointer
 *@param space the space (in bytes) that should be available at that location. should be at least one.
 *@return 1 if we would overrun any bounds or there's not space bytes left after advanced position
 *zero otherwise
 */
int sizeStackAdvance (SizeStack * s, uint32_t dist, uint32_t space);

/**
 *\brief call this to rewind a pointer
 */
int sizeStackRewind (SizeStack * s, uint32_t dist, uint32_t space);

#endif
