#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "utl.h"
#include "pidbuf.h"
#include "debug.h"
#include "dmalloc.h"

int DF_PIDBUF;
#define FILE_DBG DF_PIDBUF

#ifndef dmalloc_tag
#define dmalloc_tag(a_,b_,c_) (1)
#endif

/**
 *\brief merges two pidbuffers into a new one
 *
 * new buffer will have no duplicate pids
 * source buffers stay valid
 * yes, it is slow
 *
 *\return newly allocated pidbuf or NULL
 */
uint16_t *
dmalloc_pidbufMergeUniq (uint16_t * a, uint16_t num_a, uint16_t * b,
                         uint16_t num_b, uint16_t * num_ret,
                         char *file NOTUSED, int line NOTUSED)
{
  uint16_t worst_num = num_a + num_b;
  uint16_t final_num;
  uint16_t *rv;
//  debugMsg("pidbufMergeUniq: a=%p, num_a=%hu, b=%p, num_b=%hu, num_ret=%p\n",
//  a,num_a,b,num_b,num_ret);
  rv = utlCalloc (worst_num, sizeof (uint16_t));
  if (rv)
  {
    int i;
    if (num_a)
      memmove (rv, a, num_a * sizeof (uint16_t));
    final_num = num_a;
    for (i = 0; i < num_b; i++)
    {
      if (!pidbufContains (b[i], rv, final_num))
      {
        rv[final_num] = b[i];
//        debugMsg("put %hu to %hu\n",b[i],final_num);
        final_num++;
      }
    }
    rv = utlRealloc (rv, final_num * sizeof (uint16_t));        //AAAARGGGHHHH
    *num_ret = final_num;
  }
  if (rv)
    dmalloc_tag (rv, file, line);
  return rv;
}

///returns one if array contains element equal to v
int
pidbufContains (uint16_t v, uint16_t * array, uint16_t array_size)
{
  int i;
  if (NULL == array)
    return 0;
  for (i = 0; i < array_size; i++)
  {
    if (array[i] == v)
      return 1;
  }
  return 0;
}

uint16_t *
dmalloc_pidbufAddPid (uint16_t pid, uint16_t ** pidbuf, uint16_t * num_pids,
                      char *file NOTUSED, int line NOTUSED)
{
  uint16_t num2 = *num_pids;
  uint16_t *buf2 = *pidbuf;
  if (!pidbufContains (pid, buf2, num2))
  {
    void *p;
    p = utlRealloc (buf2, sizeof (uint16_t) * (num2 + 1));
    if (!p)
      return NULL;
    buf2 = p;
    buf2[num2] = pid;
    num2++;
    *pidbuf = buf2;
    *num_pids = num2;
    if (buf2)
      dmalloc_tag (buf2, file, line);
    return buf2;
  }
  if (buf2)
    dmalloc_tag (buf2, file, line);
  return buf2;
}

uint16_t *
dmalloc_pidbufRmvPid (uint16_t pid, uint16_t ** pidbuf, uint16_t * num_pids,
                      char *file NOTUSED, int line NOTUSED)
{
  uint16_t num2 = *num_pids;
  uint16_t *buf2 = *pidbuf;
  int i;
  uint16_t *rv = NULL;
  for (i = 0; i < num2; i++)
  {
    if (buf2[i] == pid)
    {

      memmove (buf2 + i, buf2 + i + 1, (num2 - i - 1) * sizeof (uint16_t));
      num2--;
      rv = buf2;
    }
  }
  if (NULL != rv)
  {
    rv = utlRealloc (rv, num2 * sizeof (uint16_t));
    *num_pids = num2;
    *pidbuf = rv;
  }
  if (rv)
    dmalloc_tag (rv, file, line);
  return rv;
}

uint16_t *
dmalloc_pidbufDifference (uint16_t * a_buf, uint16_t a_sz, uint16_t * b_buf,
                          uint16_t b_sz, uint16_t * num_ret,
                          char *file NOTUSED, int line NOTUSED)
{
  int i;
  uint16_t *rv = NULL;
  uint16_t num_rv = 0;
  if (a_buf == NULL)
    a_sz = 0;
  if (b_buf == NULL)
    b_sz = 0;
  for (i = 0; i < a_sz; i++)
  {
    if (!pidbufContains (a_buf[i], b_buf, b_sz))
    {
      if (NULL == pidbufAddPid (a_buf[i], &rv, &num_rv))
      {
//                              errMsg("out of mem\n");
        *num_ret = num_rv;
        if (rv)
          dmalloc_tag (rv, file, line);
        return rv;
      }
    }
  }
  *num_ret = num_rv;
  if (rv)
    dmalloc_tag (rv, file, line);
  return rv;
}

///duplicates a pidbuf
uint16_t *
dmalloc_pidbufDup (uint16_t * a, uint16_t num_a, char *file, int line)
{
  uint16_t *rv = NULL;
  rv = utlCalloc (num_a, sizeof (uint16_t));
  if (rv)
  {
    memmove (rv, a, num_a * sizeof (uint16_t));
    dmalloc_tag (rv, file, line);
  }
  return rv;
}


void
pidbufDump (char *pfx, uint16_t * pids, uint16_t sz)
{
  int i;
  int rv, offs;
  char buf[1024] = { 0 };
  char *p = buf;
  offs = 0;
  for (i = 0; i < sz; i++)
  {
    rv = snprintf (p + offs, 1024 - offs, "%" PRIu16 ",", pids[i]);
    if (rv < 0)
      return;
    offs += rv;
    if (offs >= 1023)
      return;
  }
  debugMsg ("%s: %s\n", pfx, buf);
}
