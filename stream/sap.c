#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "in_out.h"
#include "sap.h"
#include "config.h"
#include "utl.h"
#include "debug.h"
#include "pidbuf.h"
#include "dmalloc.h"
int DF_SAP;
#define FILE_DBG DF_SAP

#define SAP_PORT 9875
///bits
#define DEFAULT_SAP_LIMIT 4000
#define DEFAULT_USERNAME "user"



static int sap_announce_pending (DvbSap * sap);
static void *sap_loop (void *p);
static void sap_delete (DvbSap * sap);
static void sap_new (DvbSap * sap);
static void sap_proc_evt (DvbSap * sap, SelBuf * evt);
static void sap_announce (DvbSap * sap);
static int sap_sdp_delete (DvbSap * sap, char *d, int len);
static int sap_sdp_announce (DvbSap * sap, char *d, int len);
static int sap_header (DvbSap * sap, uint8_t type);
//static void sap_to_ascii(char * p);
static char *sap_get_desc (DvbSap * sap);
static void CUsapClear (void *s);


int
sapInit (DvbSap * sap, RcFile * cfg, dvb_s * d, PgmDb * pgmdb, EpgDb * epgdb)
{
  Section *s;
//      char sbuf[64];
//      int adapter,dmx;
  char *v;
  long tmp;
  char *bcast_addr;
  char *sap_addr;
  char *my_addr;
  int sockfd;
  int ttl;
  int loop;
  int af;
  uint16_t sap_port;
  memset (sap, 0, sizeof (DvbSap));
  debugMsg ("sapInit start\n");
  s = rcfileFindSec (cfg, "SAP");

  sap->username = DEFAULT_USERNAME;
  if (s && (!rcfileFindSecVal (s, "username", &v)))
  {
    sap->username = v;
  }
  debugMsg ("using username: %s\n", sap->username);

  af = DEFAULT_AF;
  if (s && !rcfileFindSecValInt (s, "af", &tmp))
  {
    if (tmp == 6)
      af = AF_INET6;
    if (tmp == 4)
      af = AF_INET;
  }
  debugMsg ("using af: %s\n", (af == AF_INET) ? "AF_INET" : "AF_INET6");

  my_addr = DEFAULT_SAP_SRC (af);
  if (s && (!rcfileFindSecVal (s, "my_addr", &v)))
  {
    my_addr = v;
  }
  debugMsg ("using my_addr: %s\n", my_addr);
  sap->my_addr = my_addr;

  sap_addr = DEFAULT_SAP_ADDR (af);
  if (s && (!rcfileFindSecVal (s, "sap_addr", &v)))
  {
    sap_addr = v;
  }
  debugMsg ("using sap_addr: %s\n", sap_addr);

  sap_port = SAP_PORT;
  if (s && !rcfileFindSecValInt (s, "sap_port", &tmp))
  {
    sap_port = tmp;
  }
  debugMsg ("using sap_port: %" PRIu16 "\n", sap_port);


  sap->bcast_port = BCAST_PORT;
  if (!rcfileFindValInt (cfg, "SERVER", "bcast_port", &tmp))
  {
    sap->bcast_port = tmp;
  }
  debugMsg ("using port: %" PRIu16 "\n", sap->bcast_port);

  ttl = BCAST_TTL;
  if (!rcfileFindValInt (cfg, "SERVER", "bcast_ttl", &tmp))
  {
    ttl = tmp;
  }
  sap->ttl = ttl;
  debugMsg ("using ttl: %u\n", sap->ttl);

  sap->bcast_af = DEFAULT_AF;
  if (!rcfileFindValInt (cfg, "SERVER", "af", &tmp))
  {
    if (tmp == 6)
      sap->bcast_af = AF_INET6;
    if (tmp == 4)
      sap->bcast_af = AF_INET;
  }
  debugMsg ("using broadcast af: %s\n",
            (sap->bcast_af == AF_INET) ? "AF_INET" : "AF_INET6");

  bcast_addr = BCAST_ADDR (sap->bcast_af);
  if (!rcfileFindVal (cfg, "SERVER", "bcast_addr", &v))
  {
    bcast_addr = v;
  }
  debugMsg ("using bcast_addr: %s\n", bcast_addr);
  sap->bcast_addr = bcast_addr;

  loop = 0;

  sockfd = ioUdpSocket (af, sap_addr, sap_port, ttl, loop, &sap->sap_addr);
  if (-1 == sockfd)
    return 1;

  sap->bcast_sock = sockfd;
  sap->dvb = d;
  sap->epg_db = epgdb;
  sap->pgm_db = pgmdb;
  taskInit (&sap->t, sap, sap_loop);
  sap->o = dvbDmxOutAdd (sap->dvb);
  sap->input = svtOutSelPort (sap->o);  //dvbGetTsOutput (sap->dvb, NULL, 0);
  sap->s_id = sap->s_ver = time (NULL);
  sap->tuned = 0;
  sap->idle = 1;
  return 0;
}

void
sapClear (DvbSap * s)
{
  taskClear (&s->t);
  if (s->pnrs)
  {
    utlFAN (s->pnrs);
  }
  s->pnrs = NULL;
  s->num_pnrs = 0;
  if (s->tuned)
  {
    sap_delete (s);
    s->tuned = 0;
  }
  dvbDmxOutRmv (s->o);
  s->o = NULL;
  s->input = NULL;
//  dvbTsOutputClear (s->dvb, s->input);
  shutdown (s->bcast_sock, SHUT_RDWR);
  close (s->bcast_sock);
}

int
CUsapInit (DvbSap * sap, RcFile * cfg, dvb_s * d, PgmDb * pgmdb,
           EpgDb * epgdb, CUStack * s)
{
  int rv;
  rv = sapInit (sap, cfg, d, pgmdb, epgdb);
  cuStackFail (rv, sap, CUsapClear, s);
  return rv;
}

int
sapStart (DvbSap * s)
{
  return taskStart (&s->t);
}

int
sapStop (DvbSap * s)
{
  return taskStop (&s->t);
}

static void
CUsapClear (void *s)
{
  sapClear (s);
}

static int
sap_announce_pending (DvbSap * sap)
{
  time_t current;
  current = time (NULL);
  return ((current > (sap->last_announce + 300)) && sap->tuned);
}

static void *
sap_loop (void *p)
{
  DvbSap *sap = p;
  int s_rv;
  taskLock (&sap->t);
  while (taskRunning (&sap->t))
  {
    taskUnlock (&sap->t);
    s_rv = selectorWaitData (sap->input, 1000);
    taskLock (&sap->t);
    if (!s_rv)
    {
      uint8_t *d;
      while ((d = selectorReadElement (sap->input)))
      {
        if (selectorEIsEvt (d))
        {
          sap_proc_evt (sap, (SelBuf *) d);
        }
        selectorReleaseElement (svtOutSelector (sap->o), d);
      }
    }
    if (sap_announce_pending (sap))     //&&(sap->num_pnrs!=0))
    {
      sap_announce (sap);
    }
  }
  taskUnlock (&sap->t);
  return NULL;
}

static void
sap_delete (DvbSap * sap)
{
  int len;
  len = sap_header (sap, 1);
  len += sap_sdp_delete (sap, (char *) sap->sap_packet + len, 1024 - len);
  sap->sap_len = len;
  sap_announce (sap);
}

static void
sap_new (DvbSap * sap)
{
  int len;
  sap->s_ver++;
  len = sap_header (sap, 0);
  len += sap_sdp_announce (sap, (char *) sap->sap_packet + len, 1024 - len);
  sap->sap_len = len;
  sap_announce (sap);
}

static void
sap_proc_evt (DvbSap * sap, SelBuf * d)
{
  switch (selectorEvtId (d))
  {
  case E_TUNING:
    debugMsg ("E_TUNING\n");
    sap->idle = 1;
    if (sap->pnrs)
    {
      utlFAN (sap->pnrs);
    }
    sap->pnrs = NULL;
    sap->num_pnrs = 0;
    if (sap->tuned)
    {
      sap_delete (sap);
      sap->tuned = 0;
    }
    break;
  case E_IDLE:
    debugMsg ("E_IDLE\n");
    sap->idle = 1;
    if (sap->pnrs)
    {
      utlFAN (sap->pnrs);
    }
    sap->pnrs = NULL;
    sap->num_pnrs = 0;
    if (sap->tuned)
    {
      sap_delete (sap);
      sap->tuned = 0;
    }
    break;
  case E_TUNED:
    debugMsg ("E_TUNED\n");
    sap->idle = 0;
//              tda=(TuneDta *)selectorEvtDta(d);
    sap->pos = d->event.d.tune.pos;
    sap->freq = d->event.d.tune.freq;
    sap->pol = d->event.d.tune.pol;
    break;
  case E_PNR_ADD:
    debugMsg ("E_PNR_ADD\n");
//              pnr_dta=(PnrDta *)selectorEvtDta(d);
    if (sap->tuned)
    {
      sap_delete (sap);
    }
    pidbufAddPid (d->event.d.pnr, &sap->pnrs, &sap->num_pnrs);
    sap_new (sap);
    sap->tuned = 1;
    break;
  case E_PNR_RMV:
    debugMsg ("E_PNR_RMV\n");
//              pnr_dta=(PnrDta *)selectorEvtDta(d);
    if (sap->tuned)
    {
      sap_delete (sap);
    }
    pidbufRmvPid (d->event.d.pnr, &sap->pnrs, &sap->num_pnrs);
    sap_new (sap);
    sap->tuned = 1;
    break;
  default:
    break;
  }
}

static void
sap_announce (DvbSap * sap)
{
  debugMsg ("sending packet. size:%" PRIu16 "\n", sap->sap_len);
  if (sendto
      (sap->bcast_sock, sap->sap_packet, sap->sap_len, 0,
       (struct sockaddr *) (&sap->sap_addr),
       (sap->sap_addr.ss_family ==
        AF_INET) ? sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6))
      < 0)
  {
    errMsg ("sendto error %s\n", strerror (errno));
  }
  sap->last_announce = time (NULL);
}

static int
sap_sdp_delete (DvbSap * sap, char *d, int len)
{
  int rv;
  rv =
    snprintf (d, len, "v=0\no=%s %" PRIu32 " %" PRIu32 " IN %s %s\n",
              sap->username, sap->s_id,
              sap->s_ver,
              (sap->sap_addr.ss_family == AF_INET) ? "IP4" : "IP6",
              sap->my_addr);
  if (rv >= len)
  {
    d[len - 1] = '\0';
    rv = len;
  }
  else
    rv++;
  debugMsg ("sdp_delete: %s\n", d);
  return rv;
}


static char *
sap_get_desc (DvbSap * sap)
{
  ProgramInfo *pi;
  int i;
  unsigned size = 0;
  char *str = NULL;
  for (i = 0; i < sap->num_pnrs; i++)
  {
    int num_res;
    pi =
      pgmdbFindPgmPnr (sap->pgm_db, sap->pos, sap->freq, sap->pol,
                       sap->pnrs[i], &num_res);
    if (pi)
    {
      char *buf = NULL;
      if (i != 0)
        utlSmartCat (&str, &size, "/");
      buf = dvbStringToAscii (&pi->service_name);
      utlSmartCat (&str, &size, buf);
      utlFAN (buf);

      programInfoClear (pi);
      utlFAN (pi);
    }
  }
  utlSmartCatDone (&str);
  return str;
}

static int
sap_sdp_announce (DvbSap * sap, char *d, int len)
{
  char *desc;
  int rv;
  desc = sap_get_desc (sap);
  if (sap->bcast_af == AF_INET)
  {
    rv = snprintf (d, len, "\
v=0\n\
o=%s %" PRIu32 " %" PRIu32 " IN %s %s\n\
s=DVB Stream\n\
i=%s\n\
c=IN IP4 %s/%u\n\
t=0 0\n\
m=video %" PRIu16 "/1 RTP/AVP 33\n\
i=%s\n", sap->username, sap->s_id, sap->s_ver, (sap->sap_addr.ss_family == AF_INET) ? "IP4" : "IP6", sap->my_addr, desc, sap->bcast_addr, sap->ttl, sap->bcast_port, desc);
  }
  else
  {
    rv = snprintf (d, len, "\
v=0\n\
o=%s %" PRIu32 " %" PRIu32 " IN %s %s\n\
s=DVB Stream\n\
i=%s\n\
c=IN IP6 %s\n\
t=0 0\n\
m=video %" PRIu16 "/1 RTP/AVP 33\n\
i=%s\n", sap->username, sap->s_id, sap->s_ver, (sap->sap_addr.ss_family == AF_INET) ? "IP4" : "IP6", sap->my_addr, desc, sap->bcast_addr, sap->bcast_port, desc);
  }
  utlFAN (desc);
  if (rv >= len)
  {
    d[len - 1] = '\0';
    rv = len;
  }
  else
    rv++;
  debugMsg ("sdp_announce: %s\n", d);
  return rv;
}

static int
sap_header (DvbSap * sap, uint8_t type)
{
  uint8_t v = 1, a = (sap->sap_addr.ss_family == AF_INET) ? 0 : 1, r = 0, e =
    0, c = 0, a_len = 0, tmp;
  uint16_t tmp16;
  static char const *mime = "application/sdp";
  int offs;
  tmp =
    ((v & 7) << 5) | ((a & 1) << 4) | ((r & 1) << 3) | ((type & 1) << 2) |
    ((e & 1) << 1) | ((c & 1) << 0);
  sap->sap_packet[0] = tmp;
  sap->sap_packet[1] = a_len;
  tmp16 = htons (sap->s_ver & 0xffff);
  memmove (sap->sap_packet + 2, &tmp16, sizeof tmp16);
  if (sap->sap_addr.ss_family == AF_INET)
  {
    inet_pton (AF_INET, sap->my_addr, (void *) (sap->sap_packet + 4));
    offs = 4 + 4;
  }
  else
  {
    inet_pton (AF_INET6, sap->my_addr, (void *) (sap->sap_packet + 4));
    offs = 16 + 4;
  }
  strncpy ((char *) sap->sap_packet + offs, mime, 16);
  offs += 16;
  return offs;
//      offs+=sap_sdp(sap,sap->sap_packet+offs,1024-offs);
//      sap->sap_len=offs;
}

int
sapIdle (DvbSap * db)
{
  int rv;
  taskLock (&db->t);
  rv = (1 == db->idle);
  taskUnlock (&db->t);
  return rv;
}
