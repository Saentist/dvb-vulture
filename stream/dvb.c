#include <stdio.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <poll.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h>

#include "dvb.h"
#include "utl.h"
#include "config.h"
#include "debug.h"
//#include "pgmdb.h"
#include "dmalloc.h"
int DF_DVB;
#define FILE_DBG DF_DVB

///conditionally close file desc and invalidate
#define CCAIFD(fd_) ((fd_ != -1)?(close( fd_ ), fd_ =-1 ):0)

char *festatus_strings[] = {
  "FESTATUS_HAS_SIGNAL = 0x01\n",
  "FESTATUS_HAS_CARRIER= 0x02\n",
  "FESTATUS_HAS_VITERBI= 0x04\n",
  "FESTATUS_HAS_SYNC   = 0x08\n",
  "FESTATUS_HAS_LOCK   = 0x10\n",
  "FESTATUS_TIMEDOUT   = 0x20\n",
  "FESTATUS_REINIT     = 0x40\n"
};

char *fecaps_strings[] = {
  "FE_CAN_INVERSION_AUTO         = 0x1\n",
  "FE_CAN_FEC_1_2                = 0x2\n",
  "FE_CAN_FEC_2_3                = 0x4\n",
  "FE_CAN_FEC_3_4                = 0x8\n",
  "FE_CAN_FEC_4_5                = 0x10\n",
  "FE_CAN_FEC_5_6                = 0x20\n",
  "FE_CAN_FEC_6_7                = 0x40\n",
  "FE_CAN_FEC_7_8                = 0x80\n",
  "FE_CAN_FEC_8_9                = 0x100\n",
  "FE_CAN_FEC_AUTO               = 0x200\n",
  "FE_CAN_QPSK                   = 0x400\n",
  "FE_CAN_QAM_16                 = 0x800\n",
  "FE_CAN_QAM_32                 = 0x1000\n",
  "FE_CAN_QAM_64                 = 0x2000\n",
  "FE_CAN_QAM_128                = 0x4000\n",
  "FE_CAN_QAM_256                = 0x8000\n",
  "FE_CAN_QAM_AUTO               = 0x10000\n",
  "FE_CAN_TRANSMISSION_MODE_AUTO = 0x20000\n",
  "FE_CAN_BANDWIDTH_AUTO         = 0x40000\n",
  "FE_CAN_GUARD_INTERVAL_AUTO	   = 0x80000\n",
  "FE_CAN_HIERARCHY_AUTO         = 0x100000\n",
  "FE_CAN_8VSB                   = 0x200000\n",
  "FE_CAN_16VSB                  = 0x400000\n",
  "FE_NEEDS_BENDING              = 0x20000000\n",
  "FE_CAN_RECOVER                = 0x40000000\n",
  "FE_CAN_MUTE_TS                = 0x80000000\n"
};

char *fetype_strings[] = {
  [FE_QPSK] = "FE_QPSK",
  [FE_QAM] = "FE_QAM",
  [FE_OFDM] = "FE_OFDM",
  [FE_ATSC] = "FE_ATSC"
};


static void *dvb_transfer (void *p);
static void dvb_restart (uint16_t * add_set, uint16_t add_size,
                         uint16_t * rmv_set, uint16_t rmv_size, void *dta);

static int
dvbLock (dvb_s * adapter)
{
  return taskLock (&adapter->t);
}

static int
dvbUnlock (dvb_s * adapter)
{
  return taskUnlock (&adapter->t);
}

static void
dvb_bits_to_string (uint32_t bits, char **strtbl, uint8_t sz)
{
  int i;
  for (i = 0; i < sz; i++)
  {
    if ((1 << i) & bits)
    {
      debugPri (1, strtbl[i]);
    }
  }
}

void
dvbFeStatusBits (fe_status_t s)
{
  dvb_bits_to_string (s, festatus_strings,
                      sizeof (festatus_strings) / sizeof (char *));
}

static int
dvb_read_switch (dvb_s * d, char *switchconf)
{
  FILE *f;
  uint32_t num_sw, i;
  SwitchPos *sp;
  f = fopen (switchconf, "rb");
  if (!f)
  {
    return 1;
  }

  if (1 != fread (&d->f_corr, sizeof (d->f_corr), 1, f))
  {
    fclose (f);
    return 1;
  }

  if (1 != fread (&num_sw, sizeof (num_sw), 1, f))
  {
    fclose (f);
    return 1;
  }
  if (!num_sw)
  {
    sp = NULL;
  }
  else
  {
    sp = utlCalloc (num_sw, sizeof (SwitchPos));
    for (i = 0; i < num_sw; i++)
    {
      if (spRead (sp + i, f))
      {
        while (i--)
        {
          spClear (sp + i);
          utlFAN (sp);
          fclose (f);
          return 1;
        }
      }
    }
  }
  fclose (f);
  d->positions = sp;
  d->num_positions = num_sw;
  return 0;
}

int
dvbInit (dvb_s * d, RcFile * cfg,
         void (*tuning_success) (uint32_t pos, uint32_t freq, uint32_t srate,
                                 uint8_t pol, void *user), void *user)
{
  Section *s;
//      int hwRestricted=0;
  char *tmpp;
  long tmp;
  char frontend_path[32];
//      char dvr_path[32];
  d->adapter = 0;
  d->fe = 0;
  d->dmx = 0;
  d->dvr = 0;
  d->pid_fds = NULL;
//      d->maxFilters=256;
  d->f_corr = 0;
  d->num_buffers = 8192;
  d->tuning_success = tuning_success;
  d->user = user;
  d->niceness = 0;
  d->switchconf = DEFAULT_SWTCHDB;
  s = rcfileFindSec (cfg, "DVB");
  d->tune_wait_s = 2;
  d->dmx_bufsz = 1540096;
//  d->pgmdb=pgmdb;
  if (!s)
  {
    debugPri (1, "no DVB Section in config file\n");
  }
  else
  {
    if (!rcfileFindSecValInt (s, "adapter", &tmp))      //adapter index, default:0
      d->adapter = tmp;
    if (!rcfileFindSecValInt (s, "frontend", &tmp))     //frontend index, default:0
      d->fe = tmp;
    if (!rcfileFindSecValInt (s, "demux", &tmp))        //demux index, default:0
      d->dmx = tmp;
    if (!rcfileFindSecValInt (s, "dvr", &tmp))  //dvr index, default:0
      d->dvr = tmp;
    if (!rcfileFindSecValInt (s, "tune_wait_s", &tmp))  //number of seconds to wait for a LOCK. default:2
    {
      d->tune_wait_s = tmp;
      if (d->tune_wait_s < 1)
        d->tune_wait_s = 1;
    }
//              if(!rcfileFindSecValInt(s,"restricted",&tmp))
//                      hwRestricted=tmp;
    if (!rcfileFindSecValInt (s, "num_buffers", &tmp))  //max number of ts buffers in the demuxer default:8192
    {
      d->num_buffers = tmp;
      if (d->num_buffers < 8192)
        d->num_buffers = 8192;
    }
    if (!rcfileFindSecValInt (s, "dmx_bufsz_bytes", &tmp))      //frontend(dvr) ringbuffer size. kernel default: DVR_BUFFER_SIZE=10*188*1024. minimum: 512*188. smaller values are set to minimum.
    {
      d->dmx_bufsz = tmp;
      if (d->dmx_bufsz < 96256)
        d->dmx_bufsz = 96256;
    }
    if (!rcfileFindSecValInt (s, "niceness", &tmp))     //niceness for dvb thread. default: 0
      d->niceness = tmp;
    if (!rcfileFindSecVal (s, "database", &tmpp))       //switch configuration file. default:/var/lib/dvbvulture/swtch.db
      d->switchconf = tmpp;
  }


  if (dvb_read_switch (d, d->switchconf))
  {
    errMsg
      ("couldn't read switch conf at %s \nPerhaps you need to configure ? \n",
       d->switchconf);
    d->num_positions = 0;
    d->positions = NULL;
  }

  d->frontend_fd = -1;
  d->dvr_fd = -1;
  if ((32 <=
       snprintf (frontend_path, 32, "/dev/dvb/adapter%d/frontend%d",
                 d->adapter, d->fe))
      || (32 <=
          snprintf (d->demux_path, 32, "/dev/dvb/adapter%d/demux%d",
                    d->adapter, d->dmx))
      || (32 <=
          snprintf (d->dvr_path, 32, "/dev/dvb/adapter%d/dvr%d", d->adapter,
                    d->dvr)))
  {
    errMsg ("snprintf failed\n");
    return 1;
  }

  if (!cuStackInit (&d->destroy))
    return 1;

  d->frontend_fd = CUopen (frontend_path, O_RDWR, 0, &d->destroy);
  if (d->frontend_fd == -1)
  {
    errMsg ("Failed to open %s : %s\n", frontend_path, strerror (errno));
    return 1;
  }

  if (ioctl (d->frontend_fd, FE_GET_INFO, &d->info) < 0)
  {
    errMsg ("Failed to get front end info: %s\n", strerror (errno));
    return 1;
  }

  debugPri (1, "----------------Frontend Information----------------\n");
  debugPri (1, "Name: %s\n", d->info.name);
  debugPri (1, "Type: %s\n", struGetStr (fetype_strings, d->info.type));
  debugPri (1, "Freq Min: %u, Max: %u, Stepsize: %u, Tolerance: %u\n",
            d->info.frequency_min, d->info.frequency_max,
            d->info.frequency_stepsize, d->info.frequency_tolerance);
  debugPri (1, "Symbol Rate Min: %u, Max: %u, Tolerance: %u\n",
            d->info.symbol_rate_min, d->info.symbol_rate_max,
            d->info.symbol_rate_tolerance);
  debugPri (1, "??notifier_delay??%u\n", d->info.notifier_delay);
  debugPri (1, "Caps 0x%08x\n", d->info.caps);
  dvb_bits_to_string (d->info.caps, fecaps_strings,
                      sizeof (fecaps_strings) / sizeof (char *));
  debugPri (1, "-------------End Frontend Information---------------\n");
  if (CUswdmxInit (&d->ts_dmx, dvb_restart, d, d->num_buffers, &d->destroy))
    return 1;
  if (CUtaskInit (&d->t, d, dvb_transfer, &d->destroy))
    return 1;
  d->tuned = 0;
  d->valid = 0;
  pthread_mutex_init (&d->fd_mx, NULL);
  return 0;
}

/* return current time (in microseconds) */
static uint64_t
current_time (void)
{
  struct timeval tv;
#ifdef __VMS
  (void) gettimeofday (&tv, NULL);
#else
  struct timezone tz;
  (void) gettimeofday (&tv, &tz);
#endif
  return (uint64_t) tv.tv_sec * 1000000.0 + tv.tv_usec;
}

int
dvbIsSat (dvb_s * d)
{
  switch (d->info.type)
  {
  case FE_QPSK:
  case FE_ATSC:
  case FE_QAM:
    return 1;
  default:
    return 0;
  }
  return 0;
}

/*
int dvbIsSat2(dvb_s * d)
{
  switch (d->info.type)
  {
    case FE_QPSK:
    case FE_QAM:
    if(d->info.caps&FE_CAN_2G_MODULATION)
    {
      return 1;
    }
    default:
    return 0;
	}
	return 0;
}
*/
int
dvbIsTerr (dvb_s * d)
{
  switch (d->info.type)
  {
  case FE_OFDM:
    return 1;
  default:
    return 0;
  }
  return 0;
}

//02eu4
int
dvbUnTune (dvb_s * adapter)
{
  struct dtv_property prop = {.cmd = DTV_CLEAR };
  struct dtv_properties props = { 1, &prop };
  int rv;
  taskStop (&adapter->t);
  debugPri (1, "UnTune\n");
  dvbLock (adapter);
  rv = ioctl (adapter->frontend_fd, FE_SET_PROPERTY, &props);
  CCAIFD (adapter->dvr_fd);
  adapter->tuned = 0;
  selectorSndEv (&adapter->ts_dmx.s, E_IDLE);
  dvbUnlock (adapter);
  return rv;
}

static void
dvb_close_pid (void *x NOTUSED, BTreeNode * n, int which, int depth NOTUSED)
{
  if ((which == 2) || (which == 3))
  {
    PidFd *pfd = btreeNodePayload (n);
    if (pfd->fd >= 0)
    {
      debugPri (1, "Closing Pid: %hu\n", pfd->pid);
      if (ioctl (pfd->fd, DMX_STOP))
      {
        errMsg ("DMX_STOP on %d failed: %s\n", pfd->fd, strerror (errno));
      }
      if (close (pfd->fd) < 0)
      {
        errMsg ("close on %d failed: %s\n", pfd->fd, strerror (errno));
      }
    }
    utlFAN (n);
  }
}

void
dvb_close_all_pids (dvb_s * d)
{
  debugPri (1, "deallocating pids\n");
  pthread_mutex_lock (&d->fd_mx);
  btreeWalk (d->pid_fds, d, dvb_close_pid);
  d->pid_fds = NULL;
  pthread_mutex_unlock (&d->fd_mx);
}


static void
toggle_inv (struct dvb_frontend_parameters *f_p)
{
  if (f_p->inversion == INVERSION_OFF)
    f_p->inversion = INVERSION_ON;
  else
    f_p->inversion = INVERSION_OFF;
}

int
dvbTuneT (dvb_s * adapter, __u32 pos, __u32 freq, fe_spectral_inversion_t inv,
          fe_bandwidth_t bw,
          fe_code_rate_t crh,
          fe_code_rate_t crl,
          fe_modulation_t mod,
          fe_transmit_mode_t mode,
          fe_guard_interval_t guard, fe_hierarchy_t hier, __s32 ft)
{
  TuneDta td;
  uint32_t ifreq;
//      uint8_t crapbuf[4096*2];//to empty the kernel buffer. 
  fe_status_t status = 0;
  uint64_t then, now;

  ifreq = (freq + ft) * 10 + adapter->f_corr;
  if ((adapter->tuned) && (adapter->ifreq == ifreq) && (adapter->pos == pos))
    return 0;

  td.pos = pos;
  td.freq = freq;
  td.srate = 0;
  td.pol = 0;

  taskStop (&adapter->t);
  dvbLock (adapter);
//  dvb_close_all_pids (adapter);
  assert (NULL == adapter->pid_fds);
  selectorReinit (&adapter->ts_dmx.s);

  CCAIFD (adapter->dvr_fd);

//      read(adapter->dvr_fd,crapbuf,sizeof(crapbuf));
  //despite closing all pids I get old packets afterwards. This is bad for service information (PAT=pid0 _always_). perhaps we should instead close and reopen dvr_fd. Ok
  selectorSndEvtSz (&adapter->ts_dmx.s, E_TUNING, td);
  adapter->frontend_parm.frequency = ifreq;
  if ((adapter->info.frequency_min > adapter->frontend_parm.frequency) ||
      (adapter->info.frequency_max < adapter->frontend_parm.frequency))
  {
    errMsg ("frequency %u out of range (%u to %u)\n",
            adapter->frontend_parm.frequency, adapter->info.frequency_min,
            adapter->info.frequency_max);
    dvbUnlock (adapter);
    return 1;
  }

/*	
	if((srate<adapter->info.symbol_rate_min)||(srate>adapter->info.symbol_rate_max))
	{
		errMsg("symbol rate %u out of range (%u to %u)\n",srate,adapter->info.symbol_rate_min,adapter->info.symbol_rate_max);
		dvbUnlock(adapter);
		return 1;
	}
	*/

  adapter->frontend_parm.u.ofdm.bandwidth = bw;
  adapter->frontend_parm.u.ofdm.code_rate_HP = crh;
  adapter->frontend_parm.u.ofdm.code_rate_LP = crl;
  adapter->frontend_parm.u.ofdm.constellation = mod;
  adapter->frontend_parm.u.ofdm.transmission_mode = mode;
  adapter->frontend_parm.u.ofdm.guard_interval = guard;
  adapter->frontend_parm.u.ofdm.hierarchy_information = hier;
  adapter->frontend_parm.inversion = inv;


//discard stale events ... this hangs!!!
//while(ioctl(adapter->frontend_fd,FE_GET_EVENT,&event)!=-1);
  if (ioctl (adapter->frontend_fd, FE_SET_FRONTEND, &adapter->frontend_parm) <
      0)
  {
    errMsg ("FE_SET_FRONTEND: %s\n", strerror (errno));
  }

  then = current_time ();
  do
  {
    if (ioctl (adapter->frontend_fd, FE_READ_STATUS, &status) < 0)
    {
      errMsg ("FE_READ_STATUS: %s\n", strerror (errno));
      dvbUnlock (adapter);
      return 1;
    }
    else
    {
      dvbFeStatusBits (status);
      if (status & FE_HAS_LOCK)
        break;
    }
    usleep (300000);
    now = current_time ();
  }
  while (now < (then + adapter->tune_wait_s * 1000000));
  adapter->valid = 1;
  adapter->ifreq = ifreq;
  adapter->freq = freq;
  adapter->pol = 0;
  adapter->pos = pos;
  adapter->srate = 0;
  adapter->ft = ft;
  if (status & FE_HAS_LOCK)
  {
    adapter->tuned = 1;
    debugPri (1, "tuning_success\n");
    adapter->dvr_fd = open (adapter->dvr_path, O_RDONLY | O_NONBLOCK, 0);
    if (adapter->dvr_fd == -1)
    {
      errMsg ("Failed to open %s : %s\n", adapter->dvr_path,
              strerror (errno));
      dvbUnlock (adapter);
      return 1;
    }
    if (ioctl (adapter->dvr_fd, DMX_SET_BUFFER_SIZE, adapter->dmx_bufsz))       //not sure if I have to set this for every dmx
    {
      errMsg ("DMX_SET_BUFFER_SIZE on %d failed: %s\n", adapter->dvr_fd,
              strerror (errno));
    }
    selectorSndEvtSz (&adapter->ts_dmx.s, E_TUNED, td);
    dvbUnlock (adapter);
//    adapter->tuning_success (td.pos, td.freq, td.srate, td.pol,
//                             adapter->user);
    taskStart (&adapter->t);
    return 0;
  }
  adapter->tuned = 0;
  selectorSndEv (&adapter->ts_dmx.s, E_IDLE);
  dvbUnlock (adapter);
  return 1;
}

/*
tuning should be done differently

set up the frontend here and monitor status inside dvb_transfer
call tuned callback from there.
It would be necessary to handle stream setup differently
as tuned/idle events could happen anytime.
any stream setup should thus be done via callbacks.
pgmdb has to be tuned for stream setup to work, so we provide
a func pgmdbAddCb(void (*cb)(..),data,evt)
cb(pgmdb,data,evt) gets called when pgmdb got specified event
at that point stream listing(PMT) will work and output can be set up

cb has to be called from separate thread, so pgmdb_parse_loop won't block

possibly use seperate module event notifier evtn.h
if we see event, call
evtnNotify(n,evt);
to add a callback
p=evtnAdd(n,cb,d,evt_id)
to remove callback
d=evtnRmv(n,p)


evtnInit
evtnClear
evtnStart
evtnStop

with pgmdb behind packet selector, pgmdb can listen for tuned event...
will run from selectors' callback, sync to dvb.
don't need a pointer here, either.. no need to know of each other..
*/

int
dvbTuneS (dvb_s * adapter, __u32 pos, __u32 freq, __u32 srate, __u8 pol,
          __s32 ft, fe_rolloff_t roff, fe_modulation_t mod)
{
  struct dvb_frontend_parameters f_p = { 0 };
  TuneDta td;
  uint32_t ifreq;
  fe_status_t status = 0;
  uint64_t then, now;
  if ((!adapter->num_positions) || (NULL == adapter->positions))
  {
    errMsg
      ("DVB-S but switch/LNB not configured... sorry, I cannot tune, please configure first\n");
    return 1;
  }

  if (spGetIf
      (adapter->positions, adapter->num_positions, pos, (freq + ft) * 10,
       &ifreq))
  {
    errMsg ("Switch error\n");
    return 1;
  }

  ifreq += adapter->f_corr;
  if ((adapter->tuned) && (adapter->ifreq == ifreq) && (adapter->pol == pol)
      && (adapter->pos == pos))
    return 0;

  td.pos = pos;
  td.freq = freq;
  td.srate = srate;
  td.pol = pol;

  srate *= 100;
  debugPri (1, "Tuning to %u%c,srate: %u\n", freq / 100, pol ? 'V' : 'H',
            srate);
  taskStop (&adapter->t);
  dvbLock (adapter);
//  dvb_close_all_pids (adapter);
  assert (NULL == adapter->pid_fds);
  selectorReinit (&adapter->ts_dmx.s);

  CCAIFD (adapter->dvr_fd);

  selectorSndEvtSz (&adapter->ts_dmx.s, E_TUNING, td);
  if (spDoSwitch
      (adapter->positions, adapter->num_positions, pos, (freq + ft) * 10, pol,
       adapter->frontend_fd))
  {
    errMsg ("Switch error\n");
    dvbUnlock (adapter);
    return 1;
  }

  f_p.frequency = ifreq;
  if ((adapter->info.frequency_min > f_p.frequency) ||
      (adapter->info.frequency_max < f_p.frequency))
  {
    errMsg ("frequency %u out of range (%u to %u)\n",
            f_p.frequency, adapter->info.frequency_min,
            adapter->info.frequency_max);
    dvbUnlock (adapter);
    return 1;
  }

  if ((srate < adapter->info.symbol_rate_min)
      || (srate > adapter->info.symbol_rate_max))
  {
    errMsg ("symbol rate %u out of range (%u to %u)\n", srate,
            adapter->info.symbol_rate_min, adapter->info.symbol_rate_max);
    dvbUnlock (adapter);
    return 1;
  }

  f_p.u.qpsk.symbol_rate = srate;
  f_p.u.qpsk.fec_inner = FEC_AUTO;

  f_p.inversion = INVERSION_OFF;

  if (adapter->info.type == FE_QAM)
  {
    struct dtv_properties prp;
    struct dtv_property p;

    f_p.u.qam.modulation = mod;

    p.cmd = DTV_ROLLOFF;
    p.u.data = roff;
    p.result = 0;
    prp.num = 1;
    prp.props = &p;
    if (ioctl (adapter->frontend_fd, FE_SET_PROPERTY, &prp) < 0)
    {
      errMsg ("FE_SET_PROPERTY: %s\n", strerror (errno));
    }
    if (p.result)
    {
      errMsg ("DTV_ROLLOFF: %d\n", p.result);
    }
  }
  if (ioctl
      (adapter->frontend_fd, FE_SET_FRONTEND_TUNE_MODE,
       FE_TUNE_MODE_ONESHOT) < 0)
  {
    errMsg ("FE_SET_FRONTEND_TUNE_MODE(FE_TUNE_MODE_ONESHOT): %s\n",
            strerror (errno));
  }

  adapter->frontend_parm = f_p;
  if (ioctl (adapter->frontend_fd, FE_SET_FRONTEND, &adapter->frontend_parm) <
      0)
  {
    errMsg ("FE_SET_FRONTEND: %s\n", strerror (errno));
  }

  usleep (30000);
  then = current_time ();
  do
  {
    if (ioctl (adapter->frontend_fd, FE_READ_STATUS, &status) < 0)
    {
      errMsg ("FE_READ_STATUS: %s\n", strerror (errno));
      dvbUnlock (adapter);
      return 1;
    }
    else
    {
      dvbFeStatusBits (status);
      if (status & FE_HAS_LOCK)
        break;
    }
    toggle_inv (&f_p);
    adapter->frontend_parm = f_p;
    if (ioctl (adapter->frontend_fd, FE_SET_FRONTEND, &adapter->frontend_parm)
        < 0)
    {
      errMsg ("FE_SET_FRONTEND: %s\n", strerror (errno));
    }
    usleep (30000);
    now = current_time ();
  }
  while (now < (then + adapter->tune_wait_s * 1000000));

  adapter->valid = 1;
  adapter->freq = freq;
  adapter->ifreq = ifreq;
  adapter->pol = pol;
  adapter->pos = pos;
  adapter->srate = srate;
  adapter->ft = ft;
  if (status & FE_HAS_LOCK)
  {
    adapter->tuned = 1;
    debugPri (1, "tuning_success\n");
    adapter->dvr_fd = open (adapter->dvr_path, O_RDONLY | O_NONBLOCK, 0);
    if (adapter->dvr_fd == -1)
    {
      errMsg ("Failed to open %s : %s\n", adapter->dvr_path,
              strerror (errno));
      dvbUnlock (adapter);
      return 1;
    }
    if (ioctl (adapter->dvr_fd, DMX_SET_BUFFER_SIZE, adapter->dmx_bufsz))       //not sure if I have to set this for every dmx
    {                           //guess only this is really necessary as it calls dvb_ringbuffer_reset
      errMsg ("DMX_SET_BUFFER_SIZE on %d failed: %s\n", adapter->dvr_fd,
              strerror (errno));
    }
    selectorSndEvtSz (&adapter->ts_dmx.s, E_TUNED, td);
    dvbUnlock (adapter);
//    adapter->tuning_success (td.pos, td.freq, td.srate, td.pol,
    //                           adapter->user);
    taskStart (&adapter->t);
    return 0;
  }
  adapter->tuned = 0;
  selectorSndEv (&adapter->ts_dmx.s, E_IDLE);
  dvbUnlock (adapter);
  return 1;
}

int
dvbFeStatus (dvb_s * adapter, fe_status_t * status,
             unsigned int *ber, unsigned int *strength,
             unsigned int *snr, unsigned int *ucblock)
{
  uint32_t tempU32;
  uint16_t tempU16;

  if (status)
  {
    if (ioctl (adapter->frontend_fd, FE_READ_STATUS, status) < 0)
    {
      errMsg ("FE_READ_STATUS: %s\n", strerror (errno));
      return -1;
    }
  }

  if (ber)
  {
    if (ioctl (adapter->frontend_fd, FE_READ_BER, &tempU32) < 0)
    {
      errMsg ("FE_READ_BER: %s\n", strerror (errno));
      *ber = 0xffffffff;
    }
    else
    {
      *ber = tempU32;
    }
  }
  if (strength)
  {
    if (ioctl (adapter->frontend_fd, FE_READ_SIGNAL_STRENGTH, &tempU16) < 0)
    {
      errMsg ("FE_READ_SIGNAL_STRENGTH: %s\n", strerror (errno));
      *strength = 0xffff;
    }
    else
    {
      *strength = tempU16;
    }
  }

  if (snr)
  {
    if (ioctl (adapter->frontend_fd, FE_READ_SNR, &tempU16) < 0)
    {
      errMsg ("FE_READ_SNR: %s\n", strerror (errno));
      *snr = 0xffff;
    }
    else
    {
      *snr = tempU16;
    }
  }
  if (ucblock)
  {
    if (ioctl (adapter->frontend_fd, FE_READ_UNCORRECTED_BLOCKS, &tempU32) <
        0)
    {
      errMsg ("FE_READ_UNCORRECTED_BLOCKS: %s\n", strerror (errno));
      *ucblock = 0xffffffff;
    }
    else
    {
      *ucblock = tempU32;
    }
  }
  return 0;
}

/*
void dvb_lnb_set(dvb_s *adapter, LNBInfo_t *lnbInfo)
{
	adapter->lnbInfo = *lnbInfo;
}
*/

void
dvbClear (dvb_s * d)
{
  uint32_t i;
  FILE *f;
  debugPri (1, "dvbClear\n");
  taskStop (&d->t);
  assert (NULL == d->pid_fds);
  CCAIFD (d->dvr_fd);
  f = fopen (d->switchconf, "wb");
  if (f)
  {
    fwrite (&d->f_corr, sizeof (d->f_corr), 1, f);
    fwrite (&d->num_positions, sizeof (d->num_positions), 1, f);
    for (i = 0; i < d->num_positions; i++)
      spWrite (d->positions + i, f);
    fclose (f);
  }
  for (i = 0; i < d->num_positions; i++)
  {
    spClear (d->positions + i);
  }
  utlFAN (d->positions);
  cuStackCleanup (&d->destroy);
  pthread_mutex_destroy (&d->fd_mx);
}

void
CUdvbClear (void *d)
{
  dvbClear (d);
}

int
CUdvbInit (dvb_s * d, RcFile * cfg,
           void (*tuning_success) (uint32_t pos, uint32_t freq,
                                   uint32_t srate, uint8_t pol, void *user),
           void *user, CUStack * s)
{
  int rv = dvbInit (d, cfg, tuning_success, user);
  cuStackFail (rv, d, CUdvbClear, s);
  return rv;
}

#if 0
/**
 *\brief find the sync byte in buffer of size l and move to start of buffer,
 *
 * currently unused. remove it.
 *\param offs holds the final length
 *\return one if sync byte was found, or zero if not
 */
static int
dvb_find_ts_start (uint8_t * d, int l, int *offs)
{
  int i;
  debugPri (2, "looking for ts sync\n");
  for (i = 0; i < l; i++)
  {
    if (TS_SYNC_BYTE == d[i])
    {
      memmove (d, d + i, l - i);
      *offs = l - i;
      debugPri (2, "found at %d\n", i);
      return 1;
    }
  }
  return 0;
}
#endif

#define DVBS_SYNC 0
#define DVBS_READ 1
#define NUM_BUF (256)
#define BUF_SZ (NUM_BUF*TS_LEN)
typedef struct
{
  struct iovec iov[NUM_BUF];
  SelBuf *baseptr[NUM_BUF];
  int used;                     ///<byte index. starts at iov[0] first byte
  int num_avail;                ///<buffers allocated(at the start of the iov array)
} DvbVec;

///transfers filled buffers
static void
dvb_proc_iov (dvb_s * w, DvbVec * v, ssize_t done_sz)
{
  int i;
  int num_buf;
  int part;
  int avail_buf;

  v->used += done_sz;           //so we can keep track of total size(including partial buffers)
  if (v->used > (NUM_BUF * TS_LEN))
  {                             //the number of used bytes exceeds the total number allocated!!
    errMsg ("Something's very wrong\n");
  }
  num_buf = v->used / TS_LEN;   //the number of buffers totally filled
  part = v->used % TS_LEN;      //the number of bytes in the last buffer
  avail_buf = v->num_avail - num_buf;   //the number of buffers at least partially empty

  debugPri (3, "processing iovec\n");
  for (i = 0; i < num_buf; i++)
  {
    if (v->baseptr[i]->data[0] == TS_SYNC_BYTE)
    {
      selectorFeedUnsafe (&w->ts_dmx.s, v->baseptr[i]);
      v->used -= TS_LEN;
    }
    else                        //not aligned
    {                           //release all buffers and start over
      int j;                    //should I try to align to packet boundaries manually..?
      debugPri (2, "not aligned\n");    //I _do_ see those occasionally
      for (j = i; j < v->num_avail; j++)
        selectorReleaseElementUnsafe (&w->ts_dmx.s, v->baseptr[j]);
      v->num_avail = 0;
      v->used = 0;              //also removes possibly partially filled buf
      return;
    }
  }
  selectorSignalUnsafe (&w->ts_dmx.s);

  if (part != 0)
  {                             //one buffer filled partially, adjust ptr and length. (this is rather unlikely, I wonder if it really works)
    debugPri (2, "part\n");     //have not seen this yet..
    uint8_t *ptr = v->baseptr[num_buf]->data;
    ptr += part;
    v->iov[num_buf].iov_base = ptr;
    v->iov[num_buf].iov_len = TS_LEN - part;
  }

  if (avail_buf)
  {                             //move partially empty buffers to start of vector
    memmove (v->iov, v->iov + num_buf, sizeof (struct iovec) * avail_buf);
    memmove (v->baseptr, v->baseptr + num_buf, sizeof (SelBuf *) * avail_buf);
  }
  v->num_avail = avail_buf;
}

///tries to allocate buffers. Maximum of NUM_BUF.
static void
dvb_init_iov (dvb_s * w, DvbVec * v)
{
  int i;
  SelBuf *d = NULL;

  debugPri (3, "init iovec\n");
  for (i = v->num_avail; i < NUM_BUF; i++)
  {
    d = selectorAllocElementUnsafe (&w->ts_dmx.s);
    if (NULL != d)
    {
      v->baseptr[i] = d;
      v->iov[i].iov_base = d->data;
      v->iov[i].iov_len = TS_LEN;
      d->type = 0;              //not an event
    }
    else
    {
      v->num_avail = i;
      return;
    }
  }
  v->num_avail = NUM_BUF;
}

///release available buffers
static void
dvb_clear_iov (dvb_s * w, DvbVec * v)
{
  int i;
  uint8_t *d = NULL;
  debugPri (3, "clearing iovec\n");
  selectorLock (&w->ts_dmx.s);
  for (i = 0; i < v->num_avail; i++)
  {
    d = (uint8_t *) v->baseptr[i];
    selectorReleaseElementUnsafe (&w->ts_dmx.s, d);
  }
  v->num_avail = 0;
  v->used = 0;
  selectorUnlock (&w->ts_dmx.s);
}

static void *
dvb_transfer (void *p)
{
  dvb_s *w = p;
  int rv;
  struct pollfd pfd[1];
  DvbVec v;                     //FIXXME is rather large, perhaps use malloc
  ssize_t done_sz;
  nice (w->niceness);
  pfd[0].fd = w->dvr_fd;
  pfd[0].events = POLLIN | POLLPRI;
  taskLock (&w->t);
  v.num_avail = 0;
  v.used = 0;
  selectorLock (&w->ts_dmx.s);
  dvb_init_iov (w, &v);
  selectorUnlock (&w->ts_dmx.s);
  while (taskRunning (&w->t))
  {
    taskUnlock (&w->t);
    rv = (poll (pfd, 1, 300) > 0);
    taskLock (&w->t);
    if ((rv) && (pfd[0].revents & POLLIN))
    {
      done_sz = readv (pfd[0].fd, v.iov, v.num_avail);
      selectorLock (&w->ts_dmx.s);
      if (done_sz > 0)
      {
        dvb_proc_iov (w, &v, done_sz);
      }
      else if (done_sz == -1)
      {                         //got EOVERFLOW here.. wonder if it is actual error or frontend ring buffer overflow
        /*from the dvb API docs:
           EOVERFLOW
           The filtered data was not read from the buffer in due
           time, resulting in non-read data being lost. The buffer
           is flushed.
         */
        errMsg ("readv error: %s\n", strerror (errno)); //or is 256 buffers too much?
      }

      dvb_init_iov (w, &v);
      selectorUnlock (&w->ts_dmx.s);
      if (done_sz <= (NUM_BUF / 3 * TS_LEN))
      {
        taskUnlock (&w->t);
        usleep (10000);         //does this reduce lock contention?
        taskLock (&w->t);
      }

    }
  }
  dvb_clear_iov (w, &v);
  taskUnlock (&w->t);
  return NULL;
}

static int
dvb_cmp_pid (void *x NOTUSED, const void *a, const void *b)
{
  PidFd const *afd = a, *bfd = b;
  return (bfd->pid < afd->pid) ? -1 : (bfd->pid != afd->pid);
}

static void
dvb_rmv_pid_fd (dvb_s * w, uint16_t pid)
{
  PidFd *pfd;
  PidFd cv;
  BTreeNode *n;
  cv.pid = pid;
  n = btreeNodeFind (w->pid_fds, &cv, w, dvb_cmp_pid);
  if (!n)
  {
    errMsg ("node with pid: %" PRIu16 " not found\n", pid);
    return;
  }
  pfd = btreeNodePayload (n);
  debugPri (1, "Closing Pid: %hu\n", pid);
  if (ioctl (pfd->fd, DMX_STOP))
  {
    errMsg ("DMX_STOP on %d failed: %s\n", pfd->fd, strerror (errno));
  }
  if (close (pfd->fd) < 0)
  {
    errMsg ("close on %d failed: %s\n", pfd->fd, strerror (errno));
  }
  btreeNodeRemove (&w->pid_fds, n);
  utlFAN (n);
}

static void
dvb_ins_pid_fd (dvb_s * w, uint16_t pid)
{
  PidFd pfd;
  struct dmx_pes_filter_params filt;
  BTreeNode *n;
  pfd.pid = pid;
  pfd.fd = -1;
  n = btreeNodeFind (w->pid_fds, &pfd, w, dvb_cmp_pid);
  if (n)
  {
    errMsg ("node with pid: %" PRIu16 " already exists\n", pid);
    return;
  }
  debugPri (1, "Opening Pid: %hu\n", pid);
  pfd.fd = open (w->demux_path, O_RDWR, 0);
  if (pfd.fd < 0)
  {
    errMsg ("open on %s failed: %s\n", w->demux_path, strerror (errno));
    return;
  }
  else
  {
    filt.pid = pid;
    filt.input = DMX_IN_FRONTEND;
    filt.output = DMX_OUT_TS_TAP;
    filt.pes_type = DMX_PES_OTHER;
    filt.flags = DMX_IMMEDIATE_START;
    if (ioctl (pfd.fd, DMX_SET_PES_FILTER, &filt))
    {
      errMsg ("DMX_SET_PES_FILTER on %d failed: %s\n", pfd.fd,
              strerror (errno));
      return;
    }
  }
  btreeNodeGetOrIns (&w->pid_fds, &pfd, w, dvb_cmp_pid, sizeof (PidFd));
}

static void
dvb_restart (uint16_t * add_set, uint16_t add_size, uint16_t * rmv_set,
             uint16_t rmv_size, void *dta)
{
  dvb_s *w = dta;
  int i;
  //we get called with selector locked. this is a problem as dvb_transfer_loop locks the other way around
  debugPri (2, "locking\n");
  pthread_mutex_lock (&w->fd_mx);       //one more
  debugPri (2, "locking done\n");
  for (i = 0; i < rmv_size; i++)
  {
    dvb_rmv_pid_fd (w, rmv_set[i]);
  }

  for (i = 0; i < add_size; i++)
  {
    dvb_ins_pid_fd (w, add_set[i]);
  }
  pthread_mutex_unlock (&w->fd_mx);
  return;
}

SvcOut *
dvbDmxOutAddU (dvb_s * a)
{
  return svtOutAdd (&a->ts_dmx);
}

void
dvbDmxOutRmvU (SvcOut * o)
{
  svtOutRmv (o);
}

int
dvbDmxOutModU (SvcOut * o,
               uint16_t n_funcs, uint16_t * pnrs, uint16_t * funcs,
               uint16_t n_pids, uint16_t * pids)
{
  return svtOutMod (o, n_funcs, pnrs, funcs, n_pids, pids);
}

SvcOut *
dvbDmxOutAdd (dvb_s * a)
{
  SvcOut *rv;
  dvbLock (a);
  rv = svtOutAdd (&a->ts_dmx);
  dvbUnlock (a);
  return rv;
}

#define dmx_to_dvb(dmx_) ((dvb_s*)(((uint8_t *) dmx_)-offsetof(dvb_s,ts_dmx)))

void
dvbDmxOutRmv (SvcOut * o)
{
  dvb_s *a = NULL;
  dvbLock (a = dmx_to_dvb (o->dmx));
  svtOutRmv (o);
  dvbUnlock (a);
}

int
dvbDmxOutMod (SvcOut * o,
              uint16_t n_funcs, uint16_t * pnrs, uint16_t * funcs,
              uint16_t n_pids, uint16_t * pids)
{
  int rv;
  dvb_s *a = NULL;
  dvbLock (a = dmx_to_dvb (o->dmx));
  rv = svtOutMod (o, n_funcs, pnrs, funcs, n_pids, pids);
  dvbUnlock (a);
  return rv;
}

int
dvbTuned (dvb_s * a)
{
  return a->tuned;
}

int
dvbWouldTune (dvb_s * a, __u32 pos, __u32 freq, __u32 srate NOTUSED, __u8 pol,
              __s32 ft)
{
  uint32_t ifreq;
  if (dvbIsSat (a))
  {
    if (spGetIf
        (a->positions, a->num_positions, pos, (freq + ft) * 10, &ifreq))
    {
      errMsg ("Switch error\n");
      return 1;
    }
  }
  else if (dvbIsTerr (a))
  {
    ifreq = (freq + ft) * 10;
  }
  else
  {
    errMsg ("What is this?\n");
    return 1;
  }
  ifreq += a->f_corr;
  return !((a->pos == pos) && (a->ifreq == ifreq) && (a->pol == pol)
           && (a->tuned));
}

int
dvbPnrAddEvt (dvb_s * a, uint16_t pnr)
{
  debugPri (2, "dvbPnrAddEvt()\n");
  return selectorSndEvtSz (&a->ts_dmx.s, E_PNR_ADD, pnr);
}

int
dvbPnrRmvEvt (dvb_s * a, uint16_t pnr)
{
  debugPri (2, "dvbPnrRmvEvt()\n");
  return selectorSndEvtSz (&a->ts_dmx.s, E_PNR_RMV, pnr);
}

int
dvbSetSwitch (dvb_s * a, SwitchPos * sp, uint32_t num)
{
  a->num_positions = num;
  a->positions = sp;
  debugPri (1, "SetSwitch %u positions: %p\n", a->num_positions,
            a->positions);
  selectorSndEvtSz (&a->ts_dmx.s, E_CONFIG, num);
  return 0;
}

SwitchPos *
dvbGetSwitch (dvb_s * a, uint32_t * num_ret)
{
  debugPri (1, "GetSwitch %u positions: %p\n", a->num_positions,
            a->positions);
  *num_ret = a->num_positions;
  return a->positions;
}

int32_t
dvbGetFCorr (dvb_s * a)
{
  debugPri (1, "dvbGetFCorr: %d\n", a->f_corr);
  return a->f_corr;
}

int
dvbSetFCorr (dvb_s * a, int32_t corr)
{
  debugPri (1, "dvbSetFCorr: %d\n", corr);
  if (NULL == a)
    return 1;
  a->f_corr = corr;
  return 0;
}

int
dvbGetTstat (dvb_s * a, uint8_t * valid, uint8_t * tuned, uint32_t * pos,
             uint32_t * freq, uint8_t * pol, int32_t * ft, int32_t * fc,
             uint32_t * ifreq)
{
  *valid = a->valid;
  *tuned = a->tuned;
  *pos = a->pos;
  *freq = a->freq;
  *pol = a->pol;
  *ft = a->ft;
  *fc = a->f_corr;
  *ifreq = a->ifreq;
  return 0;
}
