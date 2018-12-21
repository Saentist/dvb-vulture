#ifdef __WIN32__
#include <winsock.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include "client.h"
#include "in_out.h"
#include "ipc.h"
#include "utl.h"
#include "srv_codes.h"
#include "timestamp.h"
#include "custck.h"
#include "dmalloc_loc.h"
#include "debug.h"

int DF_CLIENT;
#define FILE_DBG DF_CLIENT

static const uint8_t SRV_ERR = SRV_E;
static const uint8_t SRV_NOERR = SRV_OK;

static const char *client_error_str[] = {
  "No Error",
  "Generic Error",
  "undefined",
  "undefined",
  "Not permitted"
};


static char *
client_strerror (uint8_t e)
{
  return (char *) struGetStr (client_error_str, e);
}


/*
static void client_rcv_transponder_info(int sock,TransponderInfo * tp)
{
	read_sz(sock,tp->frequency);
	ipcRcvS(sock,tp->pol);
	ipcRcvS(sock,tp->ftune);
	ipcRcvS(sock,tp->srate);
	ipcRcvS(sock,tp->fec);
	ipcRcvS(sock,tp->orbi_pos);
	ipcRcvS(sock,tp->east);
	ipcRcvS(sock,tp->msys);
	ipcRcvS(sock,tp->mtype);
	ipcRcvS(sock,tp->deletable);
}
*/
//clientVtGetPids

uint16_t *
clientVtGetPids (int sock, uint32_t pos, uint32_t frequency, uint8_t pol,
                 uint16_t pnr, uint16_t * num_pids)
{
  uint8_t cmd = VT_GET_PIDS;
  uint8_t res;
  uint16_t num_pk_ret;
  uint16_t *rv;
  size_t i;
  debugMsg ("vt_get_pids\n");
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  ipcSndS (sock, pol);
  ipcSndS (sock, pnr);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    *num_pids = 0;
    return NULL;
  }
  ipcRcvS (sock, num_pk_ret);
  debugMsg ("getting %" PRIu16 " pids\n", num_pk_ret);
  if (!num_pk_ret)
  {
    errMsg ("Error: no txt services\n");
    *num_pids = 0;
    return NULL;
  }
  rv = utlCalloc (num_pk_ret, sizeof (uint16_t));
  if (!rv)
  {
    errMsg ("Error: out of mem\n");
    *num_pids = 0;
    uint8_t bb;
    for (i = 0; i < num_pk_ret * sizeof (uint16_t); i++)
    {
      ipcRcvS (sock, bb);
    }
    return NULL;
  }
  debugMsg ("reading %" PRIu16 " bytes\n", num_pk_ret * sizeof (uint16_t));
  for (i = 0; i < num_pk_ret; i++)
  {
    ipcRcvS (sock, rv[i]);
  }
//      ioBlkRd(sock,rv,num_pk_ret*sizeof(uint16_t));
  *num_pids = num_pk_ret;
  //debugMsg
  return rv;
}

uint8_t *
clientVtGetSvc (int sock, uint32_t pos, uint32_t frequency, uint8_t pol,
                uint16_t pid, uint16_t * num_svc)
{
  uint8_t cmd = VT_GET_SVC;
  uint8_t res;
  uint16_t num_pk_ret;
  uint8_t *rv;
  size_t i;
  debugMsg ("vt_get_pids\n");
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  ipcSndS (sock, pol);
  ipcSndS (sock, pid);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    *num_svc = 0;
    return NULL;
  }
  ipcRcvS (sock, num_pk_ret);
  debugMsg ("getting %" PRIu16 " services\n", num_pk_ret);
  if (!num_pk_ret)
  {
    errMsg ("Error: no txt services\n");
    *num_svc = 0;
    return NULL;
  }
  rv = utlCalloc (num_pk_ret, sizeof (uint8_t));
  if (!rv)
  {
    errMsg ("Error: out of mem\n");
    *num_svc = 0;
    uint8_t bb;
    for (i = 0; i < num_pk_ret * sizeof (uint8_t); i++)
    {
      ipcRcvS (sock, bb);
    }
    return NULL;
  }
  debugMsg ("reading %" PRIu16 " bytes\n", num_pk_ret * sizeof (uint8_t));
  for (i = 0; i < num_pk_ret; i++)
  {
    ipcRcvS (sock, rv[i]);
  }
  *num_svc = num_pk_ret;
  //debugMsg
  return rv;
}

VtPk *
clientVtGetSvcPk (int sock, uint32_t pos, uint32_t frequency, uint8_t pol,
                  uint16_t pid, uint8_t data_id, uint16_t * num_pk)
{
  uint8_t err;
  uint16_t size;
  VtPk *buf;
  uint8_t cmd = VT_GET_SVC_PK;
  size_t i;
  debugMsg ("get_svc_pk\n");
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  debugMsg ("a\n", size);
  ipcSndS (sock, pol);
  debugMsg ("b\n", size);
//      ipcSndS(sock,pnr);
  ipcSndS (sock, pid);
  debugMsg ("c\n", size);
  ipcSndS (sock, data_id);
  debugMsg ("d\n", size);
  ipcRcvS (sock, err);
  debugMsg ("e\n", size);
  if (err)
  {
    errMsg ("Error: %s\n", client_strerror (err));
    *num_pk = 0;
    return NULL;
  }
  ipcRcvS (sock, size);
  debugMsg ("getting %" PRIu16 " packets\n", size);
  buf = utlCalloc (size, sizeof (VtPk));
  if (!buf)
  {
    uint8_t bb;
    errMsg ("Error: out of mem\n");
    *num_pk = 0;
    for (i = 0; i < (size * sizeof (VtPk)); i++)
    {
      ipcRcvS (sock, bb);
    }
    return NULL;
  }
  debugMsg ("reading %" PRIu16 " bytes\n", size * sizeof (VtPk));
  for (i = 0; i < size; i++)
    vtpkRcv (sock, &buf[i]);
//      ioBlkRd(sock,buf,size*sizeof(VtPk));
  *num_pk = size;
  return buf;
}

VtPk *
clientVtGetMagPk (int sock, uint32_t pos, uint32_t frequency, uint8_t pol,
                  uint16_t pid, uint8_t data_id, uint8_t magno,
                  uint16_t * num_pk)
{
  uint8_t err;
  uint16_t size;
  VtPk *buf;
  size_t i;
  uint8_t cmd = VT_GET_MAG_PK;
  debugMsg ("get_mag_pk magno: %" PRIu8 "\n", magno);
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  ipcSndS (sock, pol);
  ipcSndS (sock, pid);
  ipcSndS (sock, data_id);
  ipcSndS (sock, magno);
  ipcRcvS (sock, err);
  if (err)
  {
    errMsg ("Error: %s\n", client_strerror (err));
    *num_pk = 0;
    return NULL;
  }
  ipcRcvS (sock, size);
  buf = utlCalloc (size, sizeof (VtPk));
  if (!buf)
  {
    uint8_t bb;
    errMsg ("Error: out of mem\n");
    *num_pk = 0;
    for (i = 0; i < (size * sizeof (VtPk)); i++)
    {
      ipcRcvS (sock, bb);
    }
    return NULL;
  }
  for (i = 0; i < size; i++)
    vtpkRcv (sock, &buf[i]);
  *num_pk = size;
  return buf;
}

VtPk *
clientVtGetPagPk (int sock, uint32_t pos, uint32_t frequency, uint8_t pol,
                  uint16_t pid, uint8_t data_id, uint8_t magno, uint8_t pgno,
                  uint16_t subno, uint16_t * num_pk)
{
  uint8_t err;
  uint16_t size;
  VtPk *buf;
  uint8_t cmd = VT_GET_PAG_PK;
  size_t i;
  debugMsg ("get_pag_pk mag: %" PRIu8 " pg: 0x%" PRIx8 " sub: 0x%" PRIx16
            "\n", magno, pgno, subno);
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  ipcSndS (sock, pol);
  ipcSndS (sock, pid);
  ipcSndS (sock, data_id);
  ipcSndS (sock, magno);
  ipcSndS (sock, pgno);
  ipcSndS (sock, subno);
  ipcRcvS (sock, err);
  if (err)
  {
    errMsg ("Error: %s\n", client_strerror (err));
    *num_pk = 0;
    return NULL;
  }
  ipcRcvS (sock, size);
  buf = utlCalloc (size, sizeof (VtPk));
  if (!buf)
  {
    uint8_t bb;
    errMsg ("Error: out of mem\n");
    *num_pk = 0;
    for (i = 0; i < (size * sizeof (VtPk)); i++)
    {
      ipcRcvS (sock, bb);
    }
    return NULL;
  }
  for (i = 0; i < size; i++)
    vtpkRcv (sock, &buf[i]);
  *num_pk = size;
  return buf;
}

#ifdef __WIN32__
static int
net_start (void)
{
  WORD want_ver = MAKEWORD (1, 0);
  WSADATA d;
  int err;
  //BUG: call this in other socket funcs too, run WSACleanup() at exit
  err = WSAStartup (want_ver, &d);      //WAHAHAHAHAH!!!! the returned socket is 10093, which is WSANOTINITIALISED (but also a valid socket)
  //this differs from Ms docs...
  if (err != 0)
  {
    errMsg ("WSAStartup Failed: %s\n", strerror (err));
  }
  return err;
}
#endif

int
clientInit (int af, char *ip_addr, uint16_t prt)
{
  int sock;
  uint32_t id;
#ifdef __WIN32__
  if (net_start ())
    return -1;
#endif
  sock = ioTcpSocket (af, 0);
  if (sock == -1)
  {
    return -1;
  }
#ifdef __WIN32__
  if (af == AF_INET)
  {
    struct sockaddr_in addr;
    memset ((void *) &addr, 0, sizeof (addr));
    addr.sin_family = af;
    addr.sin_port = htons (prt);
    addr.sin_addr.S_un.S_addr = inet_addr (ip_addr);
    debugMsg ("Opening addr.sin_addr.S_un.S_addr=0x%x\n",
              addr.sin_addr.S_un.S_addr);
    if (connect (sock, (struct sockaddr *) &addr, sizeof (addr)))
    {
      errMsg ("Error on connect: %s, socket: %d\n", strerror (errno), sock /*WSAGetLastError() */ );    //what's 10009? winerror.h:WSAEBADF 'No such file or directory'
      close (sock);
      return -1;
    }
  }
  else
  {
    errMsg ("weird af:%d\n", af);
    close (sock);
    return -1;
  }
#else

  if (af == AF_INET)
  {
    struct sockaddr_in addr;
    memset ((void *) &addr, 0, sizeof (addr));
    addr.sin_family = af;
    addr.sin_port = htons (prt);
    inet_pton (af, ip_addr, &addr.sin_addr);
    if (connect (sock, (struct sockaddr *) &addr, sizeof (addr)))
    {
      errMsg ("Error on connect: %s\n", strerror (errno));
      close (sock);
      return -1;
    }
  }
  else if (af == AF_INET6)
  {
    struct sockaddr_in6 addr;
    memset ((void *) &addr, 0, sizeof (addr));
    addr.sin6_family = af;
    addr.sin6_port = htons (prt);
    inet_pton (af, ip_addr, &addr.sin6_addr);
    if (connect (sock, (struct sockaddr *) &addr, sizeof (addr)))
    {
      errMsg ("Error on connect: %s\n", strerror (errno));
      close (sock);
      return -1;
    }
  }
  else
  {
    errMsg ("weird af:%d\n", af);
    close (sock);
    return -1;
  }
#endif
  if (ipcRcvS (sock, id))
  {
    close (sock);
    return -1;
  }

  if (id != CONN_ID)
  {
    errMsg ("id mismatch: %" PRIx32 "\n", id);
    close (sock);
    return -1;
  }

  debugMsg ("Id seems ok, proceeding\n");
  return sock;
}

void
clientClear (int sock)
{
#ifdef __WIN32__
  closesocket (sock);
#else
  shutdown (sock, SHUT_RDWR);
  close (sock);
#endif
}

uint32_t
clientListPos (int sock)
{
  uint8_t err;
  uint32_t rv = 0;
  uint8_t cmd = NUM_POS;
  debugMsg ("list_pos\n");
  ipcSndS (sock, cmd);
  ipcRcvS (sock, err);
  if (err != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (err));
    return 0;
  }
  ipcRcvS (sock, rv);
  return rv;
}

TransponderInfo *
clientGetTransponders (int sock, uint32_t pos, uint32_t * sz_ret)
{
  uint32_t i;
  uint32_t num_tp;
  uint8_t cmd = LIST_TP;
  uint8_t res;
  TransponderInfo *tp;

  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    *sz_ret = 0;
    return NULL;
  }
  ipcRcvS (sock, num_tp);
  if (num_tp)
  {
    tp = utlCalloc (num_tp, sizeof (TransponderInfo));
    for (i = 0; i < num_tp; i++)
    {
      tpiRcv (sock, tp + i);
    }
    *sz_ret = num_tp;
    return tp;
  }
  errMsg ("Sorry, no Transponders\n", res);
  *sz_ret = 0;
  return NULL;
}

ProgramInfo *
clientGetPgms (int sock, uint32_t pos, uint32_t frequency, uint8_t pol,
               uint32_t * sz_ret)
{
  uint8_t cmd = LIST_PG;
  uint8_t res;
  uint32_t num_pg;
  uint32_t final_num_pg = 0;
  ProgramInfo *rv;
  uint32_t j;
  debugMsg ("get pgm\n");
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  ipcSndS (sock, pol);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    *sz_ret = 0;
    return NULL;
  }
  ipcRcvS (sock, num_pg);
  if (!num_pg)
  {
    *sz_ret = 0;
    errMsg ("Sorry. No Programs at all.\n");
    return NULL;
  }
  debugMsg ("allocating buffer\n");
  rv = utlCalloc (num_pg, sizeof (ProgramInfo));

  for (j = 0; j < num_pg; j++)
  {
    uint8_t use_it = 0;
    ProgramInfo tmp;
    debugMsg ("get pgm info\n");
    programInfoRcv (sock, &tmp);
    switch (tmp.svc_type)
    {
    case 1:
    case 2:
    case 10:
    case 17:
    case 22:
    case 25:
      use_it = 1;
      break;
    default:
      use_it = 1;
      break;
    }
    if (use_it)
    {
      memmove (rv + final_num_pg, &tmp, sizeof (ProgramInfo));
      final_num_pg++;
    }
    else
    {
      programInfoClear (&tmp);  //to get rid of those strings
    }
  }
  if (!final_num_pg)
  {
    errMsg ("Sorry, no TV or Radio programmes found\n");
    utlFAN (rv);
    *sz_ret = 0;
    return NULL;
  }
  rv = utlRealloc (rv, final_num_pg * sizeof (ProgramInfo));
  *sz_ret = final_num_pg;
  return rv;
}

static EpgArray *
rcv_epg_array2 (int sock, CUStack * st)
{
  uint32_t i;
  int rr;
  EpgArray *rv;
  rv = CUutlCalloc (1, sizeof (EpgArray), st);
  if (!rv)
    return NULL;
  ipcRcvS (sock, rv->num_pgm);
  debugMsg ("num_pgm: %d\n", rv->num_pgm);
  rv->pgm_nrs = CUutlCalloc (rv->num_pgm, sizeof (uint16_t), st);
  if (!rv->pgm_nrs)
    return NULL;
  rv->sched = CUutlCalloc (rv->num_pgm, sizeof (EpgSchedule), st);
  if (!rv->sched)
    return NULL;
  for (i = 0; i < rv->num_pgm; i++)
  {
    ipcRcvS (sock, rv->pgm_nrs[i]);
  }
  for (i = 0; i < rv->num_pgm; i++)
  {
    uint32_t j;
    ipcRcvS (sock, rv->sched[i].num_events);
    debugMsg ("num_events: %d\n", rv->sched[i].num_events);
    if (rv->sched[i].num_events)
    {
      rv->sched[i].events =
        CUutlCalloc (rv->sched[i].num_events, sizeof (uint8_t *), st);
      if (!rv->sched[i].events)
        return NULL;
    }
    for (j = 0; j < rv->sched[i].num_events; j++)
    {
      uint16_t sz;
      ipcRcvS (sock, sz);
      debugMsg ("evt_size: %" PRIu16 "\n", sz);
      rv->sched[i].events[j] = CUutlCalloc (sz, sizeof (uint8_t), st);
      if (!rv->sched[i].events[j])
        return NULL;
      rr = ioBlkRcv (sock, rv->sched[i].events[j], sz);
      debugMsg ("read returned: %d\n", rr);

    }
  }
  return rv;
}

EpgArray *
clientRcvEpgArray (int sock)
{
  EpgArray *rv;
  CUStack st;
  if (!cuStackInit (&st))
    return NULL;
  rv = rcv_epg_array2 (sock, &st);
  if (NULL != rv)
    cuStackUninit (&st);
  return rv;
}

EpgArray *
clientGetEpg (int sock, uint32_t pos,
              uint32_t frequency, uint8_t pol,
              uint16_t * pgms, uint16_t num_pgm,
              uint32_t start, uint32_t end, uint32_t num_entries)
{
  uint8_t cmd = EPG_GET_DATA;
  uint8_t res;

  int j;
  debugMsg ("get epg\n");
//      start=epg_get_buf_time();
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, frequency);
  ipcSndS (sock, pol);
  ipcSndS (sock, start);
  ipcSndS (sock, end);
  ipcSndS (sock, num_pgm);
  ipcSndS (sock, num_entries);
  for (j = 0; j < num_pgm; j++)
  {
    ipcSndS (sock, pgms[j]);
  }
  //this seems to fail for some reason.. 
  //perhaps a tcp timeout as the epg assembly takes some time. 
  //RcvSz does ioBlkRd which fails on zero reads... check for those
  //getting Interrupted system call (due to terminal resizes.. ncurses sigwinch handling)
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return NULL;
  }
  return clientRcvEpgArray (sock);
}



uint8_t
clientGoActive (int sock)
{
  uint8_t cmd = GO_ACTIVE;
  uint8_t res;

  ipcSndS (sock, cmd);

  ipcRcvS (sock, res);
  if (res)
    errMsg ("Error: %s\n", client_strerror (res));
  return res;
}

uint8_t
clientGoInactive (int sock)
{
  uint8_t cmd = GO_INACTIVE;
  uint8_t res;

  ipcSndS (sock, cmd);

  ipcRcvS (sock, res);
  if (res)
    errMsg ("Error: %s\n", client_strerror (res));
  return res;
}

int
clientRecordStart (int sock)
{
  uint8_t cmd = REC_PGM, res;
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res)
    errMsg ("Error: %s\n", client_strerror (res));
  return res;
}

int
clientRecordStop (int sock)
{
  uint8_t cmd = REC_PGM_STOP, res;
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res)
    errMsg ("Error: %s\n", client_strerror (res));
  return res;
}

int
clientDelTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol)
{
  uint8_t cmd = DEL_TP, res;
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, freq);
  ipcSndS (sock, pol);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }
  return 0;
}

int
clientFtTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol, int32_t ft)
{
  uint8_t cmd = TP_FT, res;
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, freq);
  ipcSndS (sock, pol);
  ipcSndS (sock, ft);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }
  return 0;
}

int
clientScanTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol)
{
  uint8_t cmd = SCAN_TP, res;
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, freq);
  ipcSndS (sock, pol);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }
  return 0;
}

int
clientDoTune (int sock, uint32_t pos, uint32_t freq, uint8_t pol,
              uint32_t pnr)
{
  uint8_t res;
  uint8_t cmd = TUNE_PG;
  ipcSndS (sock, cmd);
  ipcSndS (sock, pos);
  ipcSndS (sock, freq);
  ipcSndS (sock, pol);
  ipcSndS (sock, pnr);
  ipcRcvS (sock, res);
  if (res)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }
  return 0;
}

int
clientRecSAbort (int sock)
{
  uint8_t cmd;
  uint8_t res;
  cmd = ABORT_REC;
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }
  return 0;
}

int
clientRecSchedAdd (int sock, RsEntry * e)
{
  uint8_t cmd;
  uint8_t res;
  cmd = ADD_SCHED;
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
    return 1;
  rsSndEntry (e, sock);
  return 0;
}

int
clientRecSched (int sock, RsEntry * new_sch, uint32_t new_sz,
                RsEntry * prev_sch, uint32_t prev_sz)
{
  uint8_t cmd;
  uint8_t res;
  uint8_t flg;
  uint32_t sch_sz, i;
  RsEntry *r_sch;
  cmd = REC_SCHED;
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
    return 1;

  ipcRcvS (sock, sch_sz);
  r_sch = utlCalloc (sch_sz, sizeof (RsEntry));
  if (!r_sch)
  {
    for (i = 0; i < sch_sz; i++)
    {
      RsEntry tmp;
      rsRcvEntry (&tmp, sock);
      rsClearEntry (&tmp);
    }
    flg = 1;
    ipcSndS (sock, flg);
    return 2;
  }
  else
    for (i = 0; i < sch_sz; i++)
    {
      rsRcvEntry (r_sch + i, sock);
    }
  if (rsNeqSched (r_sch, sch_sz, prev_sch, prev_sz))
  {
    flg = 1;
    ipcSndS (sock, flg);
    return 3;
  }

  flg = 0;
  ipcSndS (sock, flg);
  ipcSndS (sock, new_sz);
  for (i = 0; i < new_sz; i++)
  {
    rsSndEntry (new_sch + i, sock);
  }
  return 0;
}

int
clientGetSched (int sock, RsEntry ** rs_ret, uint32_t * sz_ret)
{
  uint32_t i;
  uint8_t cmd = GET_SCHED;
  uint8_t res;
  uint32_t sch_sz;
  RsEntry *r_sch;

  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    *rs_ret = NULL;
    *sz_ret = 0;
    return res;
  }

  ipcRcvS (sock, sch_sz);
  r_sch = utlCalloc (sch_sz, sizeof (RsEntry));
  if (!r_sch)
    for (i = 0; i < sch_sz; i++)
    {
      RsEntry tmp;
      rsRcvEntry (&tmp, sock);
      rsClearEntry (&tmp);
    }
  else
    for (i = 0; i < sch_sz; i++)
    {
      rsRcvEntry (r_sch + i, sock);
    }
  *sz_ret = sch_sz;
  *rs_ret = r_sch;
  return res;
}

int
clientGetSwitch (int sock, SwitchPos ** sp_ret, uint32_t * sz_ret)
{
  uint32_t i;
  SwitchPos *sp = NULL;
  uint32_t num = 0;
  uint8_t cmd = GET_SWITCH;
  uint8_t res;

  ipcSndS (sock, cmd);

  ipcRcvS (sock, res);

  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    *sp_ret = NULL;
    *sz_ret = 0;
    return res;
  }

  ipcRcvS (sock, num);
  debugMsg ("%" PRIu32 " positions\n", num);
  if (num)
  {
    sp = utlCalloc (num, sizeof (SwitchPos));
    if (sp)
      for (i = 0; i < num; i++)
      {
        spRcv (sp + i, sock);
      }
    else
    {
      SwitchPos dummy;
      errMsg ("allocation failed\n");
      for (i = 0; i < num; i++)
      {
        spRcv (&dummy, sock);
        spClear (&dummy);
      }
      *sp_ret = NULL;
      *sz_ret = 0;
      return SRV_ERR;
    }
  }
  *sp_ret = sp;
  *sz_ret = num;
  return res;
}

int
clientSetSwitch (int sock, SwitchPos * sp, uint32_t num)
{
  uint32_t i;
  uint8_t cmd = SET_SWITCH;
  uint8_t res;
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }

  ipcSndS (sock, num);
  for (i = 0; i < num; i++)
  {
    spSnd (sp + i, sock);
  }
  return 0;
}

int
clientSetFcorr (int sock, int32_t fc)
{
  uint8_t cmd = SET_FCORR;
  uint8_t res;

  ipcSndS (sock, cmd);
  ipcSndS (sock, fc);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }

  return 0;
}

int
clientGetFcorr (int sock, int32_t * fc_ret)
{
  int32_t fc;
  uint8_t cmd = GET_FCORR;
  uint8_t res;

  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }

  ipcRcvS (sock, fc);
  *fc_ret = fc;
  return 0;
}

int
clientGetStatus (int sock, uint8_t * status_ret,
                 uint32_t * ber_ret, uint32_t * strength_ret,
                 uint32_t * snr_ret, uint32_t * ucblk_ret)
{
  uint8_t cmd = GET_STATS;
  uint8_t res;
  uint32_t ber = 0, strength = 0, snr = 0, ucblk = 0;
  uint8_t status = 0;
  debugMsg ("get_dvb_status\n");
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }

  ipcRcvS (sock, status);
  ipcRcvS (sock, ber);
  ipcRcvS (sock, strength);
  ipcRcvS (sock, snr);
  ipcRcvS (sock, ucblk);

  *status_ret = status;
  *ber_ret = ber;
  *strength_ret = strength;
  *snr_ret = snr;
  *ucblk_ret = ucblk;
  return 0;
}

int
clientGetTstat (int sock, uint8_t * valid, uint8_t * tuned, uint32_t * pos,
                uint32_t * freq, uint8_t * pol, int32_t * ft, int32_t * fc,
                uint32_t * ifreq)
{
  uint8_t cmd = SRV_TSTAT;
  uint8_t res;
  debugMsg ("GetTstat\n");
  ipcSndS (sock, cmd);
  ipcRcvS (sock, res);
  if (res != SRV_NOERR)
  {
    errMsg ("Error: %s\n", client_strerror (res));
    return 1;
  }
  ipcRcvS (sock, *valid);
  ipcRcvS (sock, *tuned);
  ipcRcvS (sock, *pos);
  ipcRcvS (sock, *freq);
  ipcRcvS (sock, *pol);
  ipcRcvS (sock, *ft);
  ipcRcvS (sock, *fc);
  ipcRcvS (sock, *ifreq);
  return 0;
}
