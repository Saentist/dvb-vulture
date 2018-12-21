#include "pgmstate.h"
#include "utl.h"
#include "server.h"
#include "debug.h"

int DF_PGM_STATE;
#define FILE_DBG DF_PGM_STATE

///unsets streaming flag
static void
pgm_cn_stop (void *v NOTUSED, DListE * e)
{
  Connection *c;
  debugPri (1, "pgm_cn_stop\n");
  c = (Connection *) e;
//      c->streaming=0;
  c->pnr = -1;
  if (recorderActive (&c->quickrec))
    recorderStop (&c->quickrec);
  debugPri (1, "done\n");
}

int
pgmStreaming (PgmState * p, uint16_t pnr)
{
  DListE *e;
  e = dlistFirst (&p->conn_active);
  while (e)
  {
    Connection *c;
    c = (Connection *) e;
    if ((c->pnr != -1) && (c->pnr == pnr))
      return 1;
    e = dlistENext (e);
  }
  return 0;
}

int
pgmRmvAllPnrs (PgmState * p)
{
  debugPri (1, "pgmRmvAllPnrs %p\n", p);
  dlistForEach (&p->conn_active, p, pgm_cn_stop);
  return 0;
}
