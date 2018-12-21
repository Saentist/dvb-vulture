#include <string.h>
#include <inttypes.h>
#include "sit_com.h"
#include "bits.h"
#include "utl.h"
#include "dvb.h"
#include "debug.h"
#include "sec_ex.h"
#include "dmalloc.h"
int DF_SEC_EX;
#define FILE_DBG DF_SEC_EX

int
secExInit (SecEx * ex, void *d, void (*release_element) (void *d, void *p))     //,uint16_t * pids,uint16_t num_pids)
{
  ex->pid_states = NULL;
  ex->release_element = release_element;
  ex->d = d;
  return 0;
}

static void
sec_ex_st_destroy (void *x, BTreeNode * this, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    SecEx *ex = x;
    SecExState *st = btreeNodePayload (this);
    if (NULL != st->aux)
    {
      ex->release_element (ex->d, st->aux);
      st->aux = NULL;
    }
    utlFAN (this);
  }
}

void
secExClear (SecEx * ex)
{
  btreeWalk (ex->pid_states, ex, sec_ex_st_destroy);
  memset (ex, 0, sizeof (SecEx));
}

static int
sec_ex_compar (void *x NOTUSED, const void *a, const void *b)
{
  SecExState const *sa = a, *sb = b;
  return sb->buf.pid - sa->buf.pid;
}

static SecExState *
sec_ex_get_or_ins_state (SecEx * ex, uint16_t pid)
{
  SecExState cv;
  BTreeNode *n;
  cv.buf.pid = pid;
  debugMsg ("Pid: %" PRIu16 "\n", pid);
  n = btreeNodeFind (ex->pid_states, &cv, NULL, sec_ex_compar);
  if (!n)
  {
    SecExState *init;
    n = utlCalloc (1, btreeNodeSize (sizeof (SecExState)));
    if (!n)
    {
      errMsg ("allocation failed\n");
      return NULL;
    }
    init = btreeNodePayload (n);
    init->buf.pid = pid;
    if (btreeNodeInsert (&ex->pid_states, n, NULL, sec_ex_compar))
    {
      errMsg ("insertion failed\n");
      utlFAN (n);
      return NULL;
    }
  }
  return btreeNodePayload (n);
}

/**
 *\brief returns the indices to payload sections
 *
 * a..b are the first part
 * b..c the second one (if another payload packet starts)
 *
 * c is -1 if no payload packet starts in this ts packet
 *
 * BUG: there may be more sections in here... more ranges/pointer fields.. etc...
 * TS 102 809 B.2.1.1
 */
static int
sec_ex_get_ranges (int *a, int *b, int *c, void *buf)
{
  int ra, rb, rc;
  uint8_t *data;
  uint8_t afc, pusi;
  debugMsg ("sec_ex_get_ranges\n");
  data = buf;
  afc = tsGetAFC (data);
  pusi = tsGetPUSI (data);
  switch (afc)
  {
  case 0:
  default:
    debugMsg ("invalid\n");
    return 1;
    break;
  case 1:                      //payload only
    ra = 4;
    break;
  case 2:                      //adapt field only
    debugMsg ("adapt only\n");
    return 2;                   //no payload
    break;
  case 3:                      //adapt+payload
    ra = data[4] + 5;
    if (ra > TS_LEN)            //error
      return 3;
    break;
  }
  rb = TS_LEN;
  rc = -1;
  debugMsg ("PUSI: 0x%" PRIx8 "\n", pusi);
  if (pusi)
  {
    rb = ra + 1 + data[ra];
    ra++;
    rc = TS_LEN;
    if ((rb > TS_LEN) || (ra > TS_LEN)) //error
      return 4;
  }
  if (ra > rb)
    return 5;
  if ((rc != -1) && (rc < rb))
    return 6;
  *a = ra, *b = rb, *c = rc;
  debugMsg ("ranges a: %d, b: %d, c: %d \n", ra, rb, rc);
  return 0;
}

static void
dump_sec_state (SecExState * st)
{
  debugMsg ("\n\tPID: %" PRIu16 "\n\tidx: %d\n\taux: %p\n\tstate: %" PRIu8
            "\n", st->buf.pid, st->buf_idx, st->aux, st->state);
}

SecBuf *
sec_ex_process_buf (SecEx * ex, void *buf)
{
  SecExState *st;
  uint8_t *data = buf;
  int a, b, c;
  debugMsg ("processing buffer\n");

  st = sec_ex_get_or_ins_state (ex, tsGetPid (data));
  if ((NULL == st) || (sec_ex_get_ranges (&a, &b, &c, buf)))
  {                             //something went wrong
    ex->release_element (ex->d, buf);
    return NULL;
  }
  debugMsg ("got sec state\n");
  dump_sec_state (st);
  switch (st->state)
  {
  case 0:                      //new
    debugMsg ("new\n");
    if (c == -1)                //no payload unit start indicator
    {
      ex->release_element (ex->d, buf); //discard, no action
      return NULL;
    }
    if ((c - b + st->buf_idx) > MAX_SEC_SZ)
    {
      memmove (st->buf.data + st->buf_idx, data + b,
               MAX_SEC_SZ - st->buf_idx);
      st->buf_idx = MAX_SEC_SZ;
    }
    else
    {
      memmove (st->buf.data + st->buf_idx, data + b, c - b);
      st->buf_idx += c - b;
    }
    st->state = 1;
    ex->release_element (ex->d, buf);   //we're done with it
    break;
  case 2:                      //a partial buffer is waiting
    debugMsg ("partial\n");
    if (sec_ex_get_ranges (&a, &b, &c, st->aux))
    {                           //no payload(shouldn't happen here)
      debugMsg ("failed\n");
      st->state = 0;
      st->buf_idx = 0;
      ex->release_element (ex->d, st->aux);
      st->aux = NULL;
      ex->release_element (ex->d, buf);
      return NULL;
    }
    if ((c - b + st->buf_idx) > MAX_SEC_SZ)
    {
      memmove (st->buf.data + st->buf_idx, st->aux + b,
               MAX_SEC_SZ - st->buf_idx);
      st->buf_idx = MAX_SEC_SZ;
    }
    else
    {
      memmove (st->buf.data + st->buf_idx, st->aux + b, c - b);
      st->buf_idx += c - b;
    }
    ex->release_element (ex->d, st->aux);
    st->aux = NULL;
    st->state = 1;
    if (sec_ex_get_ranges (&a, &b, &c, buf))
    {                           //no payload (is this ok??)
      ex->release_element (ex->d, buf);
      return NULL;
    }
    //fallthrough
  case 1:                      //first packet seen
    debugMsg ("first packet seen\n");
    if ((b - a + st->buf_idx) > MAX_SEC_SZ)
    {
      memmove (st->buf.data + st->buf_idx, data + a,
               MAX_SEC_SZ - st->buf_idx);
      st->buf_idx = MAX_SEC_SZ;
    }
    else
    {
      memmove (st->buf.data + st->buf_idx, data + a, b - a);
      st->buf_idx += b - a;
    }
    if (c != -1)                //another section starts. this may delay packet arrival at low bitrates. alternatively we could read out the sections' length field
    {
      st->aux = buf;
      st->state = 2;
      st->buf_idx = 0;
//                      ex->ready=st;//this one is complete
      return &st->buf;
    }
    else
      ex->release_element (ex->d, buf);
    break;
  default:
    errMsg ("Ooops\n");
    break;
  }
  return NULL;
}

/**
 *\brief put a ts buffer (from a ts selector) into the extractor.
 *
 * Extractor will release buffer when done
 *
 * Releases tsbuffer when done with it.
 * secExClear() and secExFlush() will implicitly release all pending buffers and flush our extractor states.
 */
SecBuf const *
secExPut (SecEx * ex, void *tsbuf)
{
  return sec_ex_process_buf (ex, tsbuf);
}

int
secExFlush (SecEx * ex)
{
  btreeWalk (ex->pid_states, ex, sec_ex_st_destroy);
  ex->pid_states = NULL;
  return 0;
}
