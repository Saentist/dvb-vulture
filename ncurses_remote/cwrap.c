#include<signal.h>
#include"cwrap.h"

#ifdef __WIN32__
//does nothing
#define siginterrupt(a_,b_)
#endif

//FIXME MINGW
static void
winch_off (void)
{
  siginterrupt (SIGWINCH, 0);   //will cause trouble with multithreading
}

static void
winch_on (void)
{
  siginterrupt (SIGWINCH, 1);
}

int
cwDoTune (int sock, uint32_t pos, uint32_t freq, uint8_t pol, uint32_t pnr)
{
  int rv;
  winch_off ();
  rv = clientDoTune (sock, pos, freq, pol, pnr);
  winch_on ();
  return rv;
}

int
cwRecSchedAdd (int sock, RsEntry * e)
{
  int rv;
  winch_off ();
  rv = clientRecSchedAdd (sock, e);
  winch_on ();
  return rv;
}

EpgArray *
cwGetEpg (int sock, uint32_t pos,
          uint32_t frequency, uint8_t pol,
          uint16_t * pgms, uint16_t num_pgm,
          uint32_t start, uint32_t end, uint32_t num_entries)
{
  EpgArray *rv;
  winch_off ();
  rv =
    clientGetEpg (sock, pos, frequency, pol, pgms, num_pgm, start, end,
                  num_entries);
  winch_on ();
  return rv;
}

VtPk *
cwVtGetSvcPk (int sock, uint32_t pos, uint32_t frequency,
              uint8_t pol, uint16_t pid, uint8_t data_id, uint16_t * num_pk)
{
  VtPk *rv;
  winch_off ();
  rv = clientVtGetSvcPk (sock, pos, frequency, pol, pid, data_id, num_pk);
  winch_on ();
  return rv;
}

VtPk *
cwVtGetMagPk (int sock, uint32_t pos, uint32_t frequency,
              uint8_t pol, uint16_t pid, uint8_t data_id,
              uint8_t magno, uint16_t * num_pk)
{
  VtPk *rv;
  winch_off ();
  rv =
    clientVtGetMagPk (sock, pos, frequency, pol, pid, data_id, magno, num_pk);
  winch_on ();
  return rv;
}

VtPk *
cwVtGetPagPk (int sock, uint32_t pos, uint32_t frequency,
              uint8_t pol, uint16_t pid, uint8_t data_id,
              uint8_t magno, uint8_t pgno, uint16_t subno, uint16_t * num_pk)
{
  VtPk *rv;
  winch_off ();
  rv =
    clientVtGetPagPk (sock, pos, frequency, pol, pid, data_id, magno, pgno,
                      subno, num_pk);
  winch_on ();
  return rv;
}

uint8_t *
cwVtGetSvc (int sock, uint32_t pos, uint32_t frequency,
            uint8_t pol, uint16_t pid, uint16_t * num_svc)
{
  uint8_t *rv;
  winch_off ();
  rv = clientVtGetSvc (sock, pos, frequency, pol, pid, num_svc);
  winch_on ();
  return rv;
}

TransponderInfo *
cwGetTransponders (int sock, uint32_t pos, uint32_t * sz_ret)
{
  TransponderInfo *i;
  winch_off ();
  i = clientGetTransponders (sock, pos, sz_ret);
  winch_on ();
  return i;
}

ProgramInfo *
cwGetPgms (int sock, uint32_t pos, uint32_t frequency,
           uint8_t pol, uint32_t * sz_ret)
{
  ProgramInfo *rv;
  winch_off ();
  rv = clientGetPgms (sock, pos, frequency, pol, sz_ret);
  winch_on ();
  return rv;
}

int
cwScanTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol)
{
  int rv;
  winch_off ();
  rv = clientScanTp (sock, pos, freq, pol);
  winch_on ();
  return rv;
}

uint16_t *
cwVtGetPids (int sock, uint32_t pos, uint32_t frequency,
             uint8_t pol, uint16_t pnr, uint16_t * num_pids)
{
  uint16_t *rv;
  winch_off ();
  rv = clientVtGetPids (sock, pos, frequency, pol, pnr, num_pids);
  winch_on ();
  return rv;
}

int
cwRecordStart (int sock)
{
  int rv;
  winch_off ();
  rv = clientRecordStart (sock);
  winch_on ();
  return rv;
}

int
cwRecordStop (int sock)
{
  int rv;
  winch_off ();
  rv = clientRecordStop (sock);
  winch_on ();
  return rv;
}

uint32_t
cwListPos (int sock)
{
  uint32_t rv;
  winch_off ();
  rv = clientListPos (sock);
  winch_on ();
  return rv;
}

int
cwGetSched (int sock, RsEntry ** rs_ret, uint32_t * sz_ret)
{
  int rv;
  winch_off ();
  rv = clientGetSched (sock, rs_ret, sz_ret);
  winch_on ();
  return rv;
}

int
cwRecSched (int sock, RsEntry * new_sch, uint32_t new_sz,
            RsEntry * prev_sch, uint32_t prev_sz)
{
  int rv;
  winch_off ();
  rv = clientRecSched (sock, new_sch, new_sz, prev_sch, prev_sz);
  winch_on ();
  return rv;
}

int
cwRecSAbort (int sock)
{
  int rv;
  winch_off ();
  rv = clientRecSAbort (sock);
  winch_on ();
  return rv;
}

int
cwGetSwitch (int sock, SwitchPos ** sp, uint32_t * sz_ret)
{
  int rv;
  winch_off ();
  rv = clientGetSwitch (sock, sp, sz_ret);
  winch_on ();
  return rv;
}

int
cwSetSwitch (int sock, SwitchPos * sp, uint32_t num)
{
  int rv;
  winch_off ();
  rv = clientSetSwitch (sock, sp, num);
  winch_on ();
  return rv;
}

int
cwGetTstat (int sock, uint8_t * valid, uint8_t * tuned, uint32_t * pos,
            uint32_t * freq, uint8_t * pol, int32_t * ft, int32_t * fc,
            uint32_t * ifreq)
{
  int rv;
  winch_off ();
  rv = clientGetTstat (sock, valid, tuned, pos, freq, pol, ft, fc, ifreq);
  winch_on ();
  return rv;
}

int
cwGetStatus (int sock, uint8_t * status_ret,
             uint32_t * ber_ret, uint32_t * strength_ret,
             uint32_t * snr_ret, uint32_t * ucblk_ret)
{
  int rv;
  winch_off ();
  rv =
    clientGetStatus (sock, status_ret, ber_ret, strength_ret, snr_ret,
                     ucblk_ret);
  winch_on ();
  return rv;
}

int
cwFtTp (int sock, uint32_t pos, uint32_t freq, uint8_t pol, int32_t ft)
{
  int rv;
  winch_off ();
  rv = clientFtTp (sock, pos, freq, pol, ft);
  winch_on ();
  return rv;
}

int
cwSetFcorr (int sock, int32_t fc)
{
  int rv;
  winch_off ();
  rv = clientSetFcorr (sock, fc);
  winch_on ();
  return rv;
}

uint8_t
cwGoActive (int sock)
{
  uint8_t rv;
  winch_off ();
  rv = clientGoActive (sock);
  winch_on ();
  return rv;
}

uint8_t
cwGoInactive (int sock)
{
  uint8_t rv;
  winch_off ();
  rv = clientGoInactive (sock);
  winch_on ();
  return rv;
}
