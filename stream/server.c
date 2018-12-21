#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include "server.h"
#include "tp_info.h"
#include "utl.h"
#include "pgm_info.h"
#include "in_out.h"
#include "ipc.h"
#include "debug.h"
//#include "sec_reader.h"
#include "sit_com.h"
#include "epgdb.h"
#include "dmalloc.h"

int DF_SERVER;
#define FILE_DBG DF_SERVER

static const uint8_t SRV_NOACTIVE = SRV_E_AUTH;
static const uint8_t SRV_ERR = SRV_E;
static const uint8_t SRV_NOERR = SRV_OK;

int
connectionInit (Connection * c, int fd, PgmState * p,
                struct sockaddr_storage *peer_addr, int niceness)
{
#define BSZ 256
  char buf[BSZ + 1];

  memset (c, 0, sizeof (Connection));
  c->super = 0;
  if ((NULL != ioNtoP (peer_addr, buf, BSZ))
      && (0 == strncmp (p->super_addr, buf, BSZ)))
    c->super = 1;
  c->sockfd = fd;
  c->pnr = -1;
  c->dead = 0;
  c->p = p;
  c->niceness = niceness;
  return 0;
}

void
connectionClear (Connection * c)
{
  if (c->pnr != -1)
  {
    dvbPnrRmvEvt (&c->p->dvb, c->pnr);
    c->pnr = -1;
  }
  if (recorderActive (&c->quickrec))
  {
    recorderStop (&c->quickrec);
  }
  if (c->p->master == c)
    c->p->master = NULL;
}

int
cnActive (Connection * c)
{
  return c->active;
}

int
srvGetDvbStats (Connection * c)
{
  uint32_t ber = 0, strength = 0, snr = 0, ucblk = 0;
  uint8_t status = 0;
  debugMsg ("get_dvb_stats\n");
//      if(dvbTuned(&c->p->dvb))
  ipcSndS (c->sockfd, SRV_NOERR);
//      else
//      {
//              ipcSndS(c->sockfd,SRV_ERR);
//              return 0;
//      }
  dvbFeStatus (&c->p->dvb, (fe_status_t *) & status, &ber, &strength, &snr,
               &ucblk);
  ipcSndS (c->sockfd, status);
  ipcSndS (c->sockfd, ber);
  ipcSndS (c->sockfd, strength);
  ipcSndS (c->sockfd, snr);
  ipcSndS (c->sockfd, ucblk);
  return 0;
}

int
srvSetFcorr (Connection * c)
{
  int32_t fcorr;
  TransponderInfo t;
  debugMsg ("set_fcorr start\n");

  ipcRcvS (c->sockfd, fcorr);

  if (!(c->super && c->active))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }

  if (dvbSetFCorr (&c->p->dvb, fcorr))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  else
  {
    //this works because I was lazy and didn't clear the frontend data on tuning failure
    //TODO: find a sane way of getting at some valid tuning parms
    if (pgmdbFindTransp
        (&c->p->program_database, &t, c->p->dvb.pos, c->p->dvb.freq,
         c->p->dvb.pol))
    {
      ipcSndS (c->sockfd, SRV_NOERR);
      return 0;
    }
    if (!recTaskActive (&c->p->recorder_task))
    {
      debugMsg ("pgmRmvAllPnrs\n");
      pgmRmvAllPnrs ((PgmState *) c->p);        //iterate over all connections stopping their pnrs/recordings.
      if (!srvTuneTpi (c->p, c->p->dvb.pos, &t))
      {
        debugMsg ("done\n");
      }
      else
      {
        errMsg ("tuning error\n");
        ipcSndS (c->sockfd, SRV_ERR);
        return 1;
      }
    }
    else
    {
      errMsg ("error\n");
      ipcSndS (c->sockfd, SRV_NOACTIVE);
      return 1;
    }
    ipcSndS (c->sockfd, SRV_NOERR);
  }
  return 0;
}

int
srvGetFcorr (Connection * c)
{
  int32_t fcorr;
  debugMsg ("get_fcorr\n");
  if (c->super && c->active)
  {
    ipcSndS (c->sockfd, SRV_NOERR);
    fcorr = dvbGetFCorr (&c->p->dvb);
    ipcSndS (c->sockfd, fcorr);
  }
  else
  {
    ipcSndS (c->sockfd, SRV_ERR);
  }
  return 0;
}

int
srvNumPos (Connection * c)
{
  uint32_t num = 0;
//      SwitchPos *sp=NULL;
  debugMsg ("num_pos start\n");
  ipcSndS (c->sockfd, SRV_NOERR);
  dvbGetSwitch (&c->p->dvb, &num);
  ipcSndS (c->sockfd, num);
  return 0;
}

int
srvListTp (Connection * c)
{
  int num_tp, i;
  TransponderInfo *t;
  uint32_t pos;
  debugMsg ("list_tp start\n");
  ipcRcvS (c->sockfd, pos);
  t = pgmdbListTransp (&c->p->program_database, pos, &num_tp);
  if (!t)
  {
    debugMsg ("pgmdb_list_tp error\n");
    ipcSndS (c->sockfd, SRV_ERR);
    return 1;
  }

  ipcSndS (c->sockfd, SRV_NOERR);
  debugMsg ("success: got %d transponders\n", num_tp);
  ipcSndS (c->sockfd, num_tp);
  for (i = 0; i < num_tp; i++)
  {
    tpiSnd (c->sockfd, &t[i]);
  }
  debugMsg ("freeing transponder array\n");
  utlFAN (t);
  return 0;
}

int
srvScanTp (Connection * c)
{
  TransponderInfo t;
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;
  debugMsg ("scan_tp\n");
  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);

  if (!(c->super && c->active))
  {
    ipcSndS (c->sockfd, SRV_NOACTIVE);
    return 0;
  }

  if (pgmdbFindTransp (&c->p->program_database, &t, pos, freq, pol))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 1;
  }

  if (pgmRmvAllPnrs (c->p) ||
      srvTuneTpi (c->p, pos, &t) || pgmdbScanTp (&c->p->program_database))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 1;
  }
  else
  {
    ipcSndS (c->sockfd, SRV_NOERR);
    return 0;
  }
}

int
srvListPg (Connection * c)
{
  int num_pg, i;
  ProgramInfo *pi;
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  debugMsg ("pgmdbListPgm\n");
  pi = pgmdbListPgm (&c->p->program_database, pos, freq, pol, &num_pg);
  if (!pi)
  {
    debugMsg ("pgmdbListPgm error\n");
    ipcSndS (c->sockfd, SRV_ERR);
    return 1;
  }

  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_pg);
  if (num_pg > 0)
  {
    for (i = 0; i < num_pg; i++)
    {
      programInfoSnd (c->sockfd, &pi[i]);
      programInfoClear (&pi[i]);
    }
    utlFAN (pi);
  }
  return 0;
}

uint8_t
server_hier_t (uint8_t hier_in)
{
  switch (hier_in & 3)
  {
  case 0:
    return HIERARCHY_NONE;
  case 1:
    return HIERARCHY_1;
  case 2:
    return HIERARCHY_2;
  case 3:
    return HIERARCHY_4;
  }
  return HIERARCHY_AUTO;
}

uint8_t
server_guard_t (uint8_t guard_in)
{
  switch (guard_in)
  {
  case 0:
    return GUARD_INTERVAL_1_32;
    break;
  case 1:
    return GUARD_INTERVAL_1_16;
    break;
  case 2:
    return GUARD_INTERVAL_1_8;
    break;
  case 3:
    return GUARD_INTERVAL_1_4;
    break;
  default:
    break;
  }
  return GUARD_INTERVAL_AUTO;
}

uint8_t
server_mode_t (uint8_t mode_in)
{
  switch (mode_in)
  {
  case 0:
    return TRANSMISSION_MODE_2K;
    break;
  case 1:
    return TRANSMISSION_MODE_8K;
    break;
  case 2:
    return TRANSMISSION_MODE_4K;
    break;
  default:
    break;
  }
  return TRANSMISSION_MODE_AUTO;
}

uint8_t
server_constellation_t (uint8_t con_in)
{
  switch (con_in)
  {
  case 0:
    return QPSK;
    break;
  case 1:
    return QAM_16;
    break;
  case 2:
    return QAM_64;
    break;
  default:
    break;
  }
  return QAM_AUTO;
}

uint8_t
server_coderate_t (uint8_t cr_in)
{
  switch (cr_in)
  {
  case 0:
    return FEC_1_2;
    break;
  case 1:
    return FEC_2_3;
    break;
  case 2:
    return FEC_3_4;
    break;
  case 3:
    return FEC_5_6;
    break;
  case 4:
    return FEC_7_8;
    break;
  default:
    break;
  }
  return FEC_AUTO;
}

uint8_t
server_bandwidth_t (uint8_t bw_in)
{
  switch (bw_in)
  {
  case 0:
    return BANDWIDTH_8_MHZ;
    break;
  case 1:
    return BANDWIDTH_7_MHZ;
    break;
  case 2:
    return BANDWIDTH_6_MHZ;
    break;
  case 3:
    return BANDWIDTH_AUTO;
    break;
  default:
    break;
  }
  return BANDWIDTH_AUTO;
}

fe_rolloff_t
server_roff_t (uint8_t roff_in)
{
  switch (roff_in & 3)
  {
  case 0:
    return ROLLOFF_35;
  case 1:
    return ROLLOFF_20;
  case 2:
    return ROLLOFF_25;
  case 3:
  default:
    return ROLLOFF_AUTO;
  }
}

fe_modulation_t
server_mod_t (uint8_t mod_in)
{
  switch (mod_in & 3)
  {
  case 0:
  case 1:
  default:
    return QPSK;
  case 2:
    return PSK_8;
  case 3:
    return QAM_16;
  }
}

int
srvTuneTpi (PgmState * p, uint32_t pos, TransponderInfo * t)
{
  dvb_s *dvb = &p->dvb;
  int rv;
  if (dvbTuned (dvb))
  {
    dvbUnTune (dvb);
  }

  //wait for state change to propagate
  while ((!pgmdbUntuned (&p->program_database)) ||
#ifdef WITH_VT
         (!vtdbIdle (&p->videotext_database)) ||
#endif
#ifdef WITH_MHEG
         (!mhegIdle (&p->mheg)) ||
#endif
         (!epgdbIdle (&p->epg_database)) ||
         (!netWriterIdle (&p->net_writer)) || (!sapIdle (&p->sap_announcer)))
    usleep (300000);

  if (dvbIsSat (dvb))
  {
    rv = dvbTuneS (dvb, pos, t->frequency, t->srate, t->pol, t->ftune,
                   server_roff_t (t->roff), server_mod_t (t->mtype));
  }
  else if (dvbIsTerr (dvb))
  {
    rv = dvbTuneT (dvb, pos, t->frequency, t->inv,      //INVERSION_AUTO,
                   server_bandwidth_t (t->bw),
                   server_coderate_t (t->code_rate_h),
                   (t->hier & 3) ? server_coderate_t (t->code_rate_l) :
                   FEC_NONE, server_constellation_t (t->constellation),
                   server_mode_t (t->mode), server_guard_t (t->guard),
                   server_hier_t (t->hier), t->ftune);
  }
  else
  {
    errMsg ("unsupported delivery system (tuner)\n");
    rv = 1;
  }

  //again..
  if (rv == 0)
    while ((pgmdbUntuned (&p->program_database)) ||
#ifdef WITH_MHEG
           (mhegIdle (&p->mheg)) ||
#endif
#ifdef WITH_VT
           (vtdbIdle (&p->videotext_database)) ||
#endif
           (epgdbIdle (&p->epg_database)) ||
           (netWriterIdle (&p->net_writer)) || (sapIdle (&p->sap_announcer)))
      usleep (300000);
  return rv;
}



int
srvTunePg (Connection * c)
{
  TransponderInfo t;
  uint32_t freq;
  uint32_t pnr;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, pnr);
  debugMsg ("tune_pg: %" PRIu32 " %" PRIu32 " %" PRIu8 " %" PRIu32 "\n", pos,
            freq, pol, pnr);
  /*
     If we want another transponder, or initial tuning,
     see that we are currently active.
     If we are on same transponder, just add stream.
     If it's already running, do nothing..
   */
  if (pgmStreaming (c->p, pnr))
  {
    debugMsg ("Already running.\n");    //may be false information there under some circumstances...
    ipcSndS (c->sockfd, SRV_ERR);
    return 1;
  }

  if (pgmdbFindTransp (&c->p->program_database, &t, pos, freq, pol))
  {
    debugMsg ("Transponder not found\n");
    ipcSndS (c->sockfd, SRV_ERR);
    return 1;
  }
  if (dvbWouldTune (&c->p->dvb, pos, t.frequency, t.srate, t.pol, t.ftune))
  {
    if (cnActive (c) && (!recTaskActive (&c->p->recorder_task)))
    {
      debugMsg ("pgmRmvAllPnrs\n");
      pgmRmvAllPnrs ((PgmState *) c->p);        //iterate over all connections stopping their recordings and removing pnrs.
      if (!srvTuneTpi (c->p, pos, &t))
      {
        debugMsg ("dvbPnrAddEvt\n");
        dvbPnrAddEvt (&c->p->dvb, pnr);
        c->pnr = pnr;
        //                      c->streaming=1;
        debugMsg ("done\n");
      }
      else
      {
        errMsg ("tuning error\n");
        ipcSndS (c->sockfd, SRV_ERR);
        return 1;
      }
    }
    else
    {
      debugMsg ("error, not active\n");
      ipcSndS (c->sockfd, SRV_NOACTIVE);
      return 1;
    }
  }
  else
  {
    if (c->pnr != -1)
    {
      dvbPnrRmvEvt (&c->p->dvb, c->pnr);
      c->pnr = -1;
    }

    dvbPnrAddEvt (&c->p->dvb, pnr);
    c->pnr = pnr;
//              c->streaming=1;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  return 0;
}

/*
int tune_name_pg(Connection * c)
{
	ipcSndS(c->sockfd,SRV_NOERR);
	return 0;
}

int scan_tune_func(void * x,uint32_t * pos,TransponderInfo * i)
{
	Connection * c=x;
	return dvb_tune(&c->p->dvb,pos,i->frequency,i->srate*100,i->pol);
}

int scan_full(Connection * c)
{
	debugMsg("starting full scan\n");
	if(pgmdbScanFull(&c->p->program_database,c,scan_tune_func))
	{
//		pgmdbStop(&c->p->program_database);
		ipcSndS(c->sockfd,SRV_ERR);
		return 1;
	}
//	pgmdbStop(&c->p->program_database);
	ipcSndS(c->sockfd,SRV_NOERR);
	return 0;
}
*/
int
srvAddTp (Connection * c)
{
  TransponderInfo tpi;
  debugMsg ("scan_tp\n");
  uint32_t pos;
  ipcRcvS (c->sockfd, pos);
  tpiRcv (c->sockfd, &tpi);
  if (!(c->super && c->active))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  if (pgmdbAddTransp (&c->p->program_database, pos, &tpi))
    ipcSndS (c->sockfd, SRV_ERR);
  else
    ipcSndS (c->sockfd, SRV_NOERR);
  return 0;
}

int
srvDelTp (Connection * c)
{
  debugMsg ("del_tp\n");
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  if (!(c->super && c->active))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  if (pgmdbDelTransp (&c->p->program_database, pos, freq, pol))
    ipcSndS (c->sockfd, SRV_ERR);
  else
    ipcSndS (c->sockfd, SRV_NOERR);
  return 0;
}

int
srvTpFt (Connection * c)
{
  debugMsg ("ft_tp\n");
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;
  int32_t ft;
  TransponderInfo t;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, ft);
  if (!(c->super && c->active))
  {
    ipcSndS (c->sockfd, SRV_NOACTIVE);
    return 0;
  }
  if (pgmdbFt (&c->p->program_database, pos, freq, pol, ft))
    ipcSndS (c->sockfd, SRV_ERR);
  else
  {
    if (pgmdbFindTransp (&c->p->program_database, &t, pos, freq, pol))
    {
      ipcSndS (c->sockfd, SRV_NOERR);
      return 0;
    }
    if (!recTaskActive (&c->p->recorder_task))
    {
      debugMsg ("pgmRmvAllPnrs\n");
      pgmRmvAllPnrs ((PgmState *) c->p);        //iterate over all connections stopping their pnrs/recordings.
      if (!srvTuneTpi (c->p, pos, &t))
      {
        debugMsg ("done\n");
      }
      else
      {
        errMsg ("tuning error\n");
        ipcSndS (c->sockfd, SRV_ERR);
        return 1;
      }
    }
    else
    {
      errMsg ("error\n");
      ipcSndS (c->sockfd, SRV_NOACTIVE);
      return 1;
    }
    ipcSndS (c->sockfd, SRV_NOERR);
  }
  return 0;
}

static void
snd_epg_array (int sock, EpgArray * rv)
{
  uint32_t i;
  ipcSndS (sock, rv->num_pgm);
  debugMsg ("num_pgm: %d\n", rv->num_pgm);
  for (i = 0; i < rv->num_pgm; i++)
  {
    ipcSndS (sock, rv->pgm_nrs[i]);
  }
  for (i = 0; i < rv->num_pgm; i++)
  {
    uint32_t j;
    debugMsg ("num_events: %d\n", rv->sched[i].num_events);
    ipcSndS (sock, rv->sched[i].num_events);
    for (j = 0; j < rv->sched[i].num_events; j++)
    {
      uint16_t sz;
      sz = evtGetSize (rv->sched[i].events[j]);
      debugMsg ("event size: %" PRIu16 "\n", sz);
      ipcSndS (sock, sz);
      ioBlkWr (sock, rv->sched[i].events[j], sz);
    }
  }
}

int
srvEpgGetData (Connection * c)
{
  EpgArray *rv;
  int i;
  uint16_t num_pnr;
  uint16_t *pnrs;
  uint16_t tmp;
  uint32_t pos;
  uint32_t freq;
  uint32_t start, end;
  uint32_t num_max;
  int32_t tsid;
  uint8_t pol;
  debugMsg ("epg_get_data\n");
  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, start);
  ipcRcvS (c->sockfd, end);
  ipcRcvS (c->sockfd, num_pnr);
  ipcRcvS (c->sockfd, num_max);
  pnrs = utlCalloc (num_pnr, sizeof (uint16_t));
  if (pnrs)
  {
    for (i = 0; i < num_pnr; i++)
    {
      ipcRcvS (c->sockfd, pnrs[i]);
    }
  }
  else
  {
    for (i = 0; i < num_pnr; i++)
    {
      ipcRcvS (c->sockfd, tmp); //so client doesn't stall
    }
    ipcSndS (c->sockfd, SRV_ERR);
    debugMsg ("no pnrs\n");
    return 1;
  }

  tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol);
  if (tsid < 0)
  {
    utlFAN (pnrs);
    ipcSndS (c->sockfd, SRV_ERR);
    debugMsg ("no tsid\n");
    return 1;
  }

  rv =
    epgdbGetEvents (&c->p->epg_database, pos, tsid, pnrs, num_pnr, start, end,
                    num_max);
  if (!rv)
  {
    utlFAN (pnrs);
    ipcSndS (c->sockfd, SRV_ERR);
    debugMsg ("no events\n");
    return 1;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  snd_epg_array (c->sockfd, rv);
  utlFAN (pnrs);
  epgArrayDestroy (rv);
  return 0;
}

static int
can_go_active (Connection * c)
{
  PgmState *p = c->p;
  if (recTaskActive (&c->p->recorder_task))
    return 0;

  if (NULL == p->master)
  {
    return 1;
  }
  else
  {
    if (c->super && (!p->master->super))
    {
      return 1;
    }
  }
  return 0;
}

static int
activate_connection (Connection * c)
{
  PgmState *p = c->p;
  if (NULL != p->master)
  {
    p->master->active = 0;
  }
  c->active = 1;
  p->master = c;
  return 0;
}

static int
deactivate_connection (Connection * c)
{
  PgmState *p = c->p;
  if (c != p->master)
    return 1;
  c->active = 0;
  p->master = NULL;
  return 0;
}

int
srvGoInactive (Connection * c)
{
  if (deactivate_connection (c))
    ipcSndS (c->sockfd, SRV_ERR);
  else
  {
    debugMsg ("gone inactive %p\n", c);
    ipcSndS (c->sockfd, SRV_NOERR);
  }
  return 0;
}

int
srvGoActive (Connection * c)
{
  if (can_go_active (c))
  {
    if (activate_connection (c))
      ipcSndS (c->sockfd, SRV_ERR);
    else
    {
      debugMsg ("gone active %p\n", c);
      ipcSndS (c->sockfd, SRV_NOERR);
    }
  }
  else
    ipcSndS (c->sockfd, SRV_ERR);
  return 0;
}

static void
send_txt_info (int sock, VTI * txt)
{
  ioBlkWr (sock, txt->lang, 4);
  ipcSndS (sock, txt->txt_type);
  ipcSndS (sock, txt->magazine);
  ipcSndS (sock, txt->page);
}

static void
send_sub_info (int sock, SUBI * sub)
{
  ioBlkWr (sock, sub->lang, 4);
  ipcSndS (sock, sub->type);
  ipcSndS (sock, sub->composition);
  ipcSndS (sock, sub->ancillary);
}

static void
send_stream_info (int sock, ES * stream_info)
{
  int i;
  ipcSndS (sock, stream_info->stream_type);
  ipcSndS (sock, stream_info->pid);
  ipcSndS (sock, stream_info->cmp_tag);
  ipcSndS (sock, stream_info->num_txti);
  ipcSndS (sock, stream_info->num_subi);
  for (i = 0; i < stream_info->num_txti; i++)
  {
    send_txt_info (sock, &stream_info->txt[i]);
  }
  for (i = 0; i < stream_info->num_subi; i++)
  {
    send_sub_info (sock, &stream_info->sub[i]);
  }
}

static void
send_stream_infos (int sock, ES * stream_info, int num)
{
  int i;
  for (i = 0; i < num; i++)
  {
    send_stream_info (sock, &stream_info[i]);
  }
}

/*
int list_pids(Connection * c)
{
	ES * stream_info;
	uint32_t freq;
	uint32_t pnr;
	uint8_t pol;
	int num;
	ipcRcvS(c->sockfd,freq);
	ipcRcvS(c->sockfd,pol);
	ipcRcvS(c->sockfd,pnr);
	debugMsg("listing pids for %u %hhu %u\n",freq,pol,pnr);
	stream_info=pgmdbListStreams(&c->p->program_database,freq,pol,pnr,&num);
	if(!stream_info)
	{
		ipcSndS(c->sockfd,SRV_ERR);
		return 0;
	}
	ipcSndS(c->sockfd,SRV_NOERR);
	ipcSndS(c->sockfd,num);
	send_stream_infos(c->sockfd,stream_info,num);
	clear_es_infos(stream_info,num);
	utlFAN(stream_info);
	return 0;
}
*/
static void
send_vt_packets (int sockfd, VtPk * pk, uint16_t num_pk)
{
  uint16_t i;
  for (i = 0; i < num_pk; i++)
  {
    vtpkSnd (sockfd, &pk[i]);
  }
}

int
srvVtGetSvcPk (Connection * c)
{
#ifdef WITH_VT
  int tsid;
  VtPk *pk;
  uint16_t num_pk_ret;
#endif
  uint16_t pid;
  uint8_t data_id;
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, pid);
  ipcRcvS (c->sockfd, data_id);
#ifndef WITH_VT
  ipcSndS (c->sockfd, SRV_ERR);
#else
  if (-1 == (tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  pk =
    vtdbGetSvcPk (&c->p->videotext_database, pos, tsid, pid, data_id,
                  &num_pk_ret);
  if (!pk)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_pk_ret);
  send_vt_packets (c->sockfd, pk, num_pk_ret);
  utlFAN (pk);
#endif
  return 0;
}

int
srvVtGetMagPk (Connection * c)
{
#ifdef WITH_VT
  int tsid;
  VtPk *pk;
  uint16_t num_pk_ret;
#endif
  uint16_t pid;
  uint8_t data_id;
  uint32_t freq;
  uint8_t pol;
  uint8_t magno;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, pid);
  ipcRcvS (c->sockfd, data_id);
  ipcRcvS (c->sockfd, magno);
#ifndef WITH_VT
  ipcSndS (c->sockfd, SRV_ERR);
#else

  if (-1 == (tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  pk =
    vtdbGetMagPk (&c->p->videotext_database, pos, tsid, pid, data_id, magno,
                  &num_pk_ret);
  if (!pk)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_pk_ret);
  send_vt_packets (c->sockfd, pk, num_pk_ret);
  utlFAN (pk);
#endif
  return 0;
}

int
srvVtGetPagPk (Connection * c)
{
#ifdef WITH_VT
  int tsid;
  VtPk *pk;
  uint16_t num_pk_ret;
#endif
  uint16_t pid;
  uint8_t data_id;
  uint32_t freq;
  uint8_t pol;
  uint8_t magno;
  uint8_t pgno;
  uint16_t subno;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, pid);
  ipcRcvS (c->sockfd, data_id);
  ipcRcvS (c->sockfd, magno);
  ipcRcvS (c->sockfd, pgno);
  ipcRcvS (c->sockfd, subno);
#ifndef WITH_VT
  ipcSndS (c->sockfd, SRV_ERR);
#else
  if (-1 == (tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  pk =
    vtdbGetPg (&c->p->videotext_database, pos, tsid, pid, data_id, magno,
               pgno, subno, &num_pk_ret);
  if (!pk)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_pk_ret);
  send_vt_packets (c->sockfd, pk, num_pk_ret);
  utlFAN (pk);

#endif
  return 0;
}

/*
int vt_get_idx(Connection *c)
{
	int tsid;
	VtIdxE *pk;
	uint16_t num_pk_ret;
	uint32_t freq;
	uint8_t pol;
	ipcRcvS(c->sockfd,freq);
	ipcRcvS(c->sockfd,pol);
	if(-1==(tsid=pgmdb_get_tsid(&c->p->program_database,freq,pol)))
	{
		ipcSndS(c->sockfd,SRV_ERR);
		return 0;
	}
	pk=vtdb_get_idx(&c->p->videotext_database,tsid,&num_pk_ret);
	if(!pk)
	{
		ipcSndS(c->sockfd,SRV_ERR);
		return 0;
	}
	ipcSndS(c->sockfd,SRV_NOERR);
	ipcSndS(c->sockfd,num_pk_ret);
	ioBlkWr(c->sockfd,pk,sizeof(VtIdxE)*num_pk_ret);
	utlFAN(pk);
	return 0;
}
*/
int
srvVtGetPnrs (Connection * c)
{
#ifdef WITH_VT
  int tsid;
  uint16_t num_ret;
  uint16_t *rv;
  uint32_t i;
#endif
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
#ifndef WITH_VT
  ipcSndS (c->sockfd, SRV_ERR);
#else

  if (-1 == (tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  rv = vtdbGetPnrs (&c->p->videotext_database, pos, tsid, &num_ret);
  if (!rv)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_ret);
  for (i = 0; i < num_ret; i++)
  {
    ipcSndS (c->sockfd, rv[i]);
  }
  utlFAN (rv);
#endif

  return 0;
}

int
srvVtGetPids (Connection * c)
{
#ifdef WITH_VT
  int tsid;
  uint16_t *rv;
  uint16_t num_ret;
  uint32_t i;
#endif
  uint32_t freq;
  uint16_t pnr;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, pnr);
#ifndef WITH_VT
  ipcSndS (c->sockfd, SRV_ERR);
#else
  if (-1 == (tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  rv = vtdbGetPids (&c->p->videotext_database, pos, tsid, pnr, &num_ret);
  if (!rv)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_ret);
  for (i = 0; i < num_ret; i++)
  {
    ipcSndS (c->sockfd, rv[i]);
  }
  utlFAN (rv);
#endif
  return 0;
}

int
srvVtGetSvc (Connection * c)
{
#ifdef WITH_VT
  int tsid;
  uint8_t *rv;
  uint32_t i;
  uint16_t num_ret;
#endif
  uint16_t pid;
  uint32_t freq;
  uint8_t pol;
  uint32_t pos;

  ipcRcvS (c->sockfd, pos);
  ipcRcvS (c->sockfd, freq);
  ipcRcvS (c->sockfd, pol);
  ipcRcvS (c->sockfd, pid);
#ifndef WITH_VT
  ipcSndS (c->sockfd, SRV_ERR);
#else
  if (-1 == (tsid = pgmdbGetTsid (&c->p->program_database, pos, freq, pol)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  rv = vtdbGetSvc (&c->p->videotext_database, pos, tsid, pid, &num_ret);
  if (!rv)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  ipcSndS (c->sockfd, num_ret);
  for (i = 0; i < num_ret; i++)
  {
    ipcSndS (c->sockfd, rv[i]);
  }
  utlFAN (rv);
#endif
  return 0;
}

int
cnStreaming (Connection * c)
{
  return c->pnr != -1;
}

int
srvRecPgm (Connection * c)
{
  char *evt_name;
//      char * pvdr_name;
//      char * pgm_name;
//      int num_pgmi;
  int tsid;
  uint8_t flags = 0;
//      Selector * sel;
//      DListE * port;
//      ProgramInfo * pgi;
  if ( /*(!c->active)|| */ (!dvbTuned (&c->p->dvb)) || (!cnStreaming (c)))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }

  tsid =
    pgmdbGetTsid (&c->p->program_database, c->p->dvb.pos, c->p->dvb.freq,
                  c->p->dvb.pol);

  if (tsid == -1)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  evt_name = "quick";           //epgdb_get_current_evt_name(&c->p->epg_database,tsid,c->pnr);
/*	pgi=pgmdbFindPgmPnr(&c->p->program_database,c->p->dvb.freq,c->p->dvb.pol,c->pnr,&num_pgmi);
	if(NULL==pgi)
	{
		utlFAN(evt_name);
		ipcSndS(c->sockfd,SRV_ERR);
		return 0;
	}
	pvdr_name=pgi->provider_name;
	pgm_name=pgi->service_name;
*/
  if (recorderStart
      (&c->quickrec, evt_name, &c->p->conf, &c->p->dvb,
       &c->p->program_database, c->pnr, flags))
    ipcSndS (c->sockfd, SRV_ERR);
  else
    ipcSndS (c->sockfd, SRV_NOERR);

//      clear_program_info(pgi);
//      utlFAN(pgi);
  return 0;
}

int
srvRecPgmStop (Connection * c)
{
  if (recorderActive (&c->quickrec))
  {
    recorderStop (&c->quickrec);
    ipcSndS (c->sockfd, SRV_NOERR);
  }
  else
    ipcSndS (c->sockfd, SRV_ERR);
  return 0;
}

/**
 *\brief abort any scheduled/active recordings
 */
int
srvAbortRec (Connection * c)
{
  if (!c->super)
  {
//      errMsg("a\n");
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  //no need to be active as we can't be when recorder is active
  ipcSndS (c->sockfd, SRV_NOERR);
  recTaskCancelRunning (&c->p->recorder_task);
  return 0;
}

/**
 *\brief update recording schedule

how to do this properly?
when we edit the schedule, we basically do a read-modify-write sequence.
But if we use some kind of locking and the client crashes, the server will hang.
The other option would be to async notify client of changes to schedule status.
This may not help. Too complicated.
we may also signal that we have changed at the write stage,
causing the client to reload the schedule and start over.
We'd need to allocate some memory inside
connection and store retrieved schedule there for later compare.
free it on exit, when we get new schedule for client, and when modify fails.
Or allocate on client and do the compare there.
There will be locking, but just during calls. This is done by conn_mutex already.

server's perspective:
-----------------------
|update schedule start|
-----------------------
         |
---------------
|lock recorder|
---------------
          |
-----------------------
|send current schedule|
-----------------------
          |
--------------------
|receive abort_flag|
--------------------
          |
          |                    --------  --------
<abort_flag==1>--------------->|unlock|->|return|;
        <else>                 --------  --------
          |
----------------------
|receive new schedule|
----------------------
          |
      --------
      |unlock|
      --------
          |
      --------
      |return|
      --------

still a bit brokenz..
*/

int
srvRecSched (Connection * c)
{
  uint8_t abort_flag;
  if (!c->super)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  recTaskLock (&c->p->recorder_task);
  recTaskSndSched (c->sockfd, &c->p->recorder_task);
  ipcRcvS (c->sockfd, abort_flag);
  if (abort_flag)
  {
    recTaskUnlock (&c->p->recorder_task);
    return 0;
  }
  recTaskRcvSched (c->sockfd, &c->p->recorder_task);
  recTaskUnlock (&c->p->recorder_task);
  return 0;
}

//add a single entry
int
srvAddSched (Connection * c)
{
  if (!c->super)
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }
  ipcSndS (c->sockfd, SRV_NOERR);
  recTaskLock (&c->p->recorder_task);
  recTaskRcvEntry (c->sockfd, &c->p->recorder_task);
  recTaskUnlock (&c->p->recorder_task);
  return 0;
}


/*
int srvRecSched(Connection *c)
{
	uint8_t idx;
	RsEntry tmp;
	ipcRcvS(c->sockfd,idx);
	if((!(c->active&&c->super))||(idx>=REC_SCHED_SIZE)||(rsGet(&c->p->recorder_task.sch,idx)->state==RS_RUNNING))
	{
//	errMsg("a\n");
		rsRcvEntry(&tmp,c->sockfd);
		ipcSndS(c->sockfd,SRV_ERR);
		return 0;
	}
	if(rsRcvEntry(&tmp,c->sockfd))
	{
//	errMsg("b\n");
		ipcSndS(c->sockfd,SRV_ERR);
	}
	else
	{
		ipcSndS(c->sockfd,SRV_NOERR);
		rsClearEntry(rsGet(&c->p->recorder_task.sch,idx));
		*(rsGet(&c->p->recorder_task.sch,idx))=tmp;
	}
	return 0;
}
*/
int
srvGetSched (Connection * c)
{
  ipcSndS (c->sockfd, SRV_NOERR);
  recTaskLock (&c->p->recorder_task);
  recTaskSndSched (c->sockfd, &c->p->recorder_task);
  recTaskUnlock (&c->p->recorder_task);
  return 0;
}

int
srvGetSwitch (Connection * c)
{
  uint32_t num = 0;
  SwitchPos *sp = dvbGetSwitch (&c->p->dvb, &num);
  uint32_t i;
  debugMsg ("get_switch: %p, %u\n", sp, num);
  ipcSndS (c->sockfd, SRV_NOERR);

  ipcSndS (c->sockfd, num);
  for (i = 0; i < num; i++)
  {
    spSnd (sp + i, c->sockfd);
  }
  return 0;
}

int
srvSetSwitch (Connection * c)
{
  uint32_t num = 0;
  SwitchPos *sp = dvbGetSwitch (&c->p->dvb, &num);
  uint32_t num2 = 0;
  SwitchPos *sp2;
  uint32_t i;
  debugMsg ("set_switch\n");
  if (!(c->active && c->super))
  {
    ipcSndS (c->sockfd, SRV_ERR);
    return 0;
  }

  ipcSndS (c->sockfd, SRV_NOERR);

  ipcRcvS (c->sockfd, num2);
  if (!num2)
  {
    sp2 = NULL;
  }
  else
  {
    sp2 = utlCalloc (num2, sizeof (SwitchPos));
    for (i = 0; i < num2; i++)
    {
      spRcv (sp2 + i, c->sockfd);
    }
  }
  debugMsg ("setup_switch done: %p, %u\n", sp2, num2);
  dvbSetSwitch (&c->p->dvb, sp2, num2);
  //now deallocate the old one
  for (i = 0; i < num; i++)
  {
    spClear (sp + i);
  }
  utlFAN (sp);
  return 0;
}

int
srvTpStatus (Connection * c)
{
  uint8_t valid;
  uint8_t tuned;
  uint32_t pos;
  uint32_t freq;
  uint32_t ifreq;
  uint8_t pol;
  int32_t ft;
  int32_t fc;
  ipcSndS (c->sockfd, SRV_NOERR);
  dvbGetTstat (&c->p->dvb, &valid, &tuned, &pos, &freq, &pol, &ft, &fc,
               &ifreq);
  ipcSndS (c->sockfd, valid);
  ipcSndS (c->sockfd, tuned);
  ipcSndS (c->sockfd, pos);
  ipcSndS (c->sockfd, freq);
  ipcSndS (c->sockfd, pol);
  ipcSndS (c->sockfd, ft);
  ipcSndS (c->sockfd, fc);
  ipcSndS (c->sockfd, ifreq);
  return 0;
}


int
srvRstat (Connection * c)
{
  uint8_t tag;
  DListE *e;
  PgmState *p = c->p;
  RecTask *rt = &p->recorder_task;

  ipcSndS (c->sockfd, SRV_NOERR);
  /*
     first iterate thru all connections
     and send their rec. status
     can I find the address of the socket?
     getpeername... getsockname..
   */
  e = dlistFirst (&p->conn_active);
  while (e)
  {
    Connection *c2 = (Connection *) e;
    if (recorderActive (&c2->quickrec))
    {
      RState rs;
      struct sockaddr a;
      socklen_t len;
      tag = 0xff;
      ipcSndS (c->sockfd, tag);
      memset (&a, 0, sizeof (a));
      len = sizeof (a);
      getpeername (c2->sockfd, &a, &len);
      //FIXXME: is it in net or host order?
      ipcSndS (c->sockfd, a.sa_family);
      ioBlkWr (c->sockfd, a.sa_data, sizeof (a.sa_data));       //here, 14 bytes
      ipcSndS (c->sockfd, c2->super);
      recorderGetState (&c2->quickrec, &rs);    //TODO: make sure recorder is not stopped asynch.. not properly synchronized?
      ipcSndS (c->sockfd, c2->quickrec.pnr);
      ipcSndS (c->sockfd, rs.time);
      ipcSndS (c->sockfd, rs.size);
    }
    e = dlistENext (e);
  }
  tag = 0;
  ipcSndS (c->sockfd, tag);     //start of scheduled recordings
  recTaskLock (rt);
  e = dlistFirst (&rt->active);
  while (e)
  {
    EitRec *er = dlistEPayload (e);
    taskLock (&er->t);
    tag = 0xfc;
    ipcSndS (c->sockfd, tag);
    rsSndEntry (er->e, c->sockfd);
    tag = er->eit_state;
    ipcSndS (c->sockfd, tag);
    ipcSndS (c->sockfd, er->start);
    ipcSndS (c->sockfd, er->last_eit_seen);
    ipcSndS (c->sockfd, er->last_id_seen);
    if (recorderActive (&er->rec))      //active, recording
    {
      RState rs;
      tag = 0xfd;
      ipcSndS (c->sockfd, tag); //active, running
      recorderGetState (&er->rec, &rs);
      ipcSndS (c->sockfd, er->rec.pnr);
      ipcSndS (c->sockfd, rs.time);
      ipcSndS (c->sockfd, rs.size);
    }
    else
    {
      tag = 0xfe;
      ipcSndS (c->sockfd, tag); //active, not running
    }
    taskUnlock (&er->t);
    e = dlistENext (e);
  }
  recTaskUnlock (rt);
  tag = 1;
  ipcSndS (c->sockfd, tag);     //done

  return 0;
}

int
srvUstat (Connection * c)
{
  uint8_t tag;
  DListE *e;
  PgmState *p = c->p;
  RecTask *rt = &p->recorder_task;

  ipcSndS (c->sockfd, SRV_NOERR);
  /*
     first iterate thru all connections
     and send their rec. status
     can I find the address of the socket?
     getpeername... getsockname..
   */
  e = dlistFirst (&p->conn_active);
  while (e)
  {
    Connection *c2 = (Connection *) e;
    struct sockaddr a;
    socklen_t len;
    tag = 0xff;
    ipcSndS (c->sockfd, tag);
    memset (&a, 0, sizeof (a));
    len = sizeof (a);
    getpeername (c2->sockfd, &a, &len);
    ipcSndS (c->sockfd, a.sa_family);
    ioBlkWr (c->sockfd, a.sa_data, sizeof (a.sa_data)); //here, 14 bytes
    ipcSndS (c->sockfd, c2->super);
    ipcSndS (c->sockfd, c2->active);
    ipcSndS (c->sockfd, c2->pnr);
    e = dlistENext (e);
  }
  recTaskLock (rt);
  if (dlistFirst (&rt->active))
  {
    tag = 2;
    ipcSndS (c->sockfd, tag);   //done, recorder active
  }
  else
  {
    tag = 1;
    ipcSndS (c->sockfd, tag);   //done, recorder idle
  }
  recTaskUnlock (rt);

  return 0;
}
