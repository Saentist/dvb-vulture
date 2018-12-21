#include "tp_info.h"
#include "utl.h"
#include "dmalloc_loc.h"
#include "ipc.h"
#include "debug.h"

static char *tpi_we_strings[] = { "W", "E" };
static char *tpi_pol_strings[] = { "H", "V", "L", "R" };
static char *tpi_msys_strings[] = { "DVB-S", "DVB-S2" };
static char *tpi_mtype_strings[] = { "Auto", "QPSK", "8PSK", "16-QAM" };

static char *tpi_fec_strings[] = {
  "not defined",
  "1/2 conv. code rate",
  "2/3 conv. code rate",
  "3/4 conv. code rate",
  "5/6 conv. code rate",
  "7/8 conv. code rate",
  "8/9 conv. code rate",
  "3/5 conv. code rate",
  "4/5 conv. code rate",
  "9/10 conv. code rate",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "no conv. coding"
};

char const *
tpiGetPolStr (uint8_t pol)
{
  return struGetStr (tpi_pol_strings, pol);
}

char const *
tpiGetMsysStr (uint8_t msys)
{
  return struGetStr (tpi_msys_strings, msys);
}

char const *
tpiGetMtypeStr (uint8_t mtype)
{
  return struGetStr (tpi_mtype_strings, mtype);
}

char const *
tpiGetFecStr (uint8_t fec)
{
  return struGetStr (tpi_fec_strings, fec);
}

char const *
tpiGetWeStr (uint8_t e)
{
  return struGetStr (tpi_we_strings, e);
}

#define DO_TPI(f_,tp_) {\
	f_(sock,tp_->frequency);\
	f_(sock,tp_->pol);\
	f_(sock,tp_->ftune);\
	f_(sock,tp_->srate);\
	f_(sock,tp_->fec);\
	f_(sock,tp_->orbi_pos);\
	f_(sock,tp_->east);\
	f_(sock,tp_->roff);\
	f_(sock,tp_->msys);\
	f_(sock,tp_->mtype);\
\
	f_(sock,tp_->bw);\
	f_(sock,tp_->pri);\
	f_(sock,tp_->mp_fec);\
	f_(sock,tp_->constellation);\
	f_(sock,tp_->hier);\
	f_(sock,tp_->code_rate_h);\
	f_(sock,tp_->code_rate_l);\
	f_(sock,tp_->guard);\
	f_(sock,tp_->mode);\
	f_(sock,tp_->inv);\
\
	f_(sock,tp_->deletable);\
}

void
tpiSnd (int sock, TransponderInfo * tp)
{
DO_TPI (ipcSndS, tp)}


void
tpiRcv (int sock, TransponderInfo * tp)
{
DO_TPI (ipcRcvS, tp)}
