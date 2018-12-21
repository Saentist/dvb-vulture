#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include "dmalloc.h"
#include "tsv.h"
#include "btre.h"
#include "bits.h"

int DF_TSV;

#define TS_SYNC 0x47
#define TS_LEN 188
static int
tsv_find_start (FILE * in_f, FILE * dump_f)
{
  int c;
  off_t pos;
  int crap = 0;
  do
  {
    if (EOF == (c = fgetc (in_f)))
    {
      return -1;
    }
    if (c != TS_SYNC)
    {
      if (crap == 0)
      {
        pos = ftello (in_f);
        pos -= 1;
        fprintf (stderr, "lost sync. Offs: 0x%llx\n", pos);
        if (dump_f)
          fprintf (dump_f, "tfs@0x%llx:", pos);
        crap = 1;
      }
      if (dump_f)
        fputc (c, dump_f);
    }
  }
  while (c != TS_SYNC);

  if (crap == 1)                //lost sync before
    fprintf (stderr, "found sync\n");
  return 0;
}

static int
tsv_get_pack (FILE * in_f, FILE * dump_f, uint8_t * buf)
{
  int rv;
  buf[0] = TS_SYNC;
  rv = fread (buf + 1, sizeof (uint8_t), TS_LEN - 1, in_f);
  if ((TS_LEN - 1) != rv)
  {
    off_t pos;
    fprintf (stderr, "short ts packet byte count: %d want: %d\n", rv + 1,
             TS_LEN);
    if (dump_f)
    {
      pos = ftello (in_f);
      pos -= rv + 1;
      fprintf (dump_f, "tgp@0x%llx:", pos);
      fwrite (buf, sizeof (uint8_t), rv + 1, dump_f);
    }
    return -1;
  }
  return 0;
}

static int
tsv_get_cc (uint8_t * buf)
{
  return bitsGet (buf, 3, 4, 4);
}

static int
tsv_get_afc (uint8_t * buf)
{
  return bitsGet (buf, 3, 2, 2);
}

static int
tsv_get_pid (uint8_t * buf)
{
  return bitsGet (buf, 1, 3, 13);
}

typedef struct
{
  uint16_t pid;
  int cc;
  int same_ct;
  uint64_t count;
} TsvPid;

static int
tsv_cmp (void *x, const void *a, const void *b)
{
  TsvPid const *ai = a, *bi = b;
  return (ai->pid > bi->pid) ? -1 : (ai->pid != bi->pid);
}

struct
{
  uint64_t div;
  char const *unit;
} utbl[] =
{
  {
  1, ""},
  {
  1024, "Ki"},
  {
  1024 *1024, "Mi"},
  {
  1024 *1024 * 1024, "Gi"}
};

static uint64_t
cnv_num (uint64_t n)
{
  int i;
  i = (sizeof (utbl) / sizeof (utbl[0])) - 1;
  while ((n < utbl[i].div) && (i > 0))
    i--;

  return n / utbl[i].div;
}

static char const *
cnv_unit (uint64_t n)
{
  int i;
  i = (sizeof (utbl) / sizeof (utbl[0])) - 1;
  while ((n < utbl[i].div) && (i > 0))
    i--;

  return utbl[i].unit;
}

static void
tsv_btree_destroy (void *x, BTreeNode * this, int which, int depth)
{
  TsvPid *tp;
  if ((which == 3) || (which == 2))
  {
    tp = btreeNodePayload (this);
    fprintf (stderr, "destroying PID: 0x%" PRIx16 "\n", tp->pid);
    fprintf (stderr, "\twith %" PRIu64 " packets,\n", tp->count);
    fprintf (stderr, "\t(%" PRIu64 ") %sB\n", cnv_num (tp->count * 188),
             cnv_unit (tp->count * 188));
    free (this);
  }
}

static int
tsv_get_dci (uint8_t * buf)
{
  return bitsGet (buf, 5, 1, 1);
}

int
tsVerify (FILE * in_f, FILE * out_f, FILE * dump_f)
{
  int cc;
//      int lastpid=-1;
  off_t pos;
  int afc;
  int dci;
  uint64_t total_count = 0;
  uint16_t pid;
  BTreeNode *root = NULL;
  BTreeNode *np;
  TsvPid tmp;
  TsvPid *tp = &tmp;
  uint8_t buf[TS_LEN];
//  if (tsv_find_start (in_f, dump_f) || tsv_get_pack (in_f, dump_f, buf))
//  {
//    return 0;                   //eof
//  }
//  if (out_f)
//    fwrite (buf, sizeof (uint8_t), TS_LEN, out_f);
//  cc = tsv_get_cc (buf);
//  pid = tsv_get_pid (buf);
//  tmp.cc = cc;
//  tmp.pid = pid;
//  tmp.same_ct = 0;
//  tmp.count = 1;
//      lastpid=tmp.pid;
//  np = btreeNodeGetOrIns (&root, &tmp, NULL, tsv_cmp, sizeof (TsvPid));
  while (!feof (in_f))
  {
    int wantcc;

    if (tsv_find_start (in_f, dump_f) || tsv_get_pack (in_f, dump_f, buf))
    {
      break;                    //eof, too
    }
    total_count++;

    cc = tsv_get_cc (buf);
    pid = tsv_get_pid (buf);
    afc = tsv_get_afc (buf);
    dci = tsv_get_dci (buf);
    if (pid != 0x1fff)          //NULL pid?
    {
      tmp.pid = pid;
      np = btreeNodeFind (root, &tmp, NULL, tsv_cmp);
      if (!np)
      {                         //new pid, initialize
        tmp.cc = cc;
        tmp.pid = pid;
        tmp.same_ct = 0;
        tmp.count = 1;
        np = btreeNodeGetOrIns (&root, &tmp, NULL, tsv_cmp, sizeof (TsvPid));
        tp = btreeNodePayload (np);
        if (out_f)
          fwrite (buf, sizeof (uint8_t), TS_LEN, out_f);
//                                      fprintf(stderr,"PID: %d, cc=%d, tp->cc=%d, tp->same_ct=%d, afc=%d, dci=%d\n",tp->pid,cc,tp->cc,tp->same_ct,afc,dci);
      }
      else
      {                         //known pid, compare
        tp = btreeNodePayload (np);
//                      fprintf(stderr,"PID: %d, cc=%d, tp->cc=%d, tp->same_ct=%d, afc=%d, dci=%d\n",tp->pid,cc,tp->cc,tp->same_ct,afc,dci);
        tp->count++;
        switch (afc & 3)
        {
        case 0:
          wantcc = tp->cc;
          tp->same_ct = 0;
          break;
        case 1:
          if ((cc == tp->cc) && (tp->same_ct == 0))
          {
            tp->same_ct++;
            wantcc = tp->cc;
          }
          else
          {
            tp->same_ct = 0;
            wantcc = (tp->cc + 1) & 0xf;
          }
          break;
        case 2:
          tp->same_ct = 0;
          if (dci == 1)
            wantcc = cc;
          else
            wantcc = tp->cc;
          break;
        case 3:
          if (dci == 1)
          {
            tp->same_ct = 0;
            wantcc = cc;
          }
          else if ((cc == tp->cc) && (tp->same_ct == 0))
          {
            tp->same_ct++;
            wantcc = tp->cc;
          }
          else
          {
            tp->same_ct = 0;
            wantcc = (tp->cc + 1) & 0xf;
          }
          break;
        }
        if (wantcc != cc)
        {
          pos = ftello (in_f);
          pos -= TS_LEN;
          fprintf (stderr,
                   "Wrong cc on pid 0x%" PRIx16
                   ". Offs: 0x%llx. AFC: 0x%x. got: %d want: %d\n", pid, pos,
                   afc, cc, wantcc);
          if (dump_f)
          {
            fprintf (dump_f, "tcc@0x%llx:", pos);
            fwrite (buf, sizeof (uint8_t), TS_LEN, dump_f);
          }
        }
        if (out_f)
          fwrite (buf, sizeof (uint8_t), TS_LEN, out_f);
        tp->cc = cc;
      }
    }
  }
  fprintf (stderr, "Done. \n");
  btreeWalk (root, NULL, tsv_btree_destroy);
  fprintf (stderr, "Packet count (including NULL packets): %" PRIu64 "\n",
           total_count);
  return 0;
}
