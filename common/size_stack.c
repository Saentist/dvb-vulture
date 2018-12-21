#include <string.h>
#include <inttypes.h>
#include "size_stack.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc_loc.h"
int DF_SIZESTACK;
#define FILE_DBG DF_SIZESTACK

int
sizeStackInit (SizeStack * s, uint8_t * ptr)
{
  if ((!s) || (!ptr))
    return 1;
  memset (s, 0, sizeof (SizeStack));
  s->ptr = ptr;
  return 0;
}

int
sizeStackPop (SizeStack * s)
{
  if (s->num_bounds == 0)
  {
    debugMsg ("SizeStack underflow\n");
    return 1;
  }
  s->num_bounds--;
  return 0;
}

int
sizeStackPush (SizeStack * s, uint32_t size)
{
  if (s->num_bounds >= 32)
  {
    debugMsg ("SizeStack overflow\n");
    return 1;
  }

  if (s->num_bounds)
  {
    if ((s->ptr + size) > s->end[s->num_bounds - 1])
    {
      debugMsg ("SizeStack constraint violation\n");
      return 1;
    }
  }
  s->start[s->num_bounds] = s->ptr;
  s->end[s->num_bounds] = s->ptr + size;
  s->num_bounds++;
  return 0;
}

uint8_t *
sizeStackPtr (SizeStack * s)
{
  return s->ptr;
}

int
sizeStackAdvance (SizeStack * s, uint32_t dist, uint32_t space)
{
  uint16_t i;
  if (s->num_bounds > 0)
  {
    i = s->num_bounds - 1;
    if ((s->ptr + dist + space) > s->end[i])
    {
      debugMsg ("SizeStack advance: dist:%" PRIu32 " space:%" PRIu32
                " overrun by %" PRIu32 "\n", dist, space,
                (s->ptr + dist + space) - s->end[i]);
      return 1;
    }
  }
  s->ptr += dist;
  return 0;
}

int
sizeStackRewind (SizeStack * s, uint32_t dist, uint32_t space)
{
  uint16_t i;
  if (s->num_bounds > 0)
  {
    i = s->num_bounds - 1;
    if ((s->ptr - dist + space) > s->end[i])
    {
      debugMsg ("SizeStack rewind: overrun\n");
      return 1;
    }
    if ((s->ptr - dist) < s->start[i])
    {
      debugMsg ("SizeStack rewind: underrun\n");
      return 1;
    }
  }
  s->ptr -= dist;
  return 0;
}
