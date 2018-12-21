#include <stdlib.h>
#include "pgm_info.h"
#include "utl.h"
#include "in_out.h"
#include "ipc.h"
#include "dmalloc_loc.h"

static char *program_info_running_status_strings[] = {
  "undefined",
  "not running",
  "starts in a few seconds",
  "pausing",
  "running",
  "service off-air"
};

static char *program_info_svc_type_strings[] = {
  "reserved for future use",
  "digital television service",
  "digital radio sound service",
  "Teletext service",
  "NVOD reference service",
  "NVOD time-shifted service",
  "mosaic service",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "advanced codec digital radio sound service",
  "advanced codec mosaic service",
  "data broadcast service",
  "reserved for Common Interface Usage (EN 50221 [37])",
  "RCS Map (see EN 301 790 [7])",
  "RCS FLS (see EN 301 790 [7])",
  "DVB MHP service",
  "MPEG-2 HD digital television service",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "reserved for future use",
  "advanced codec SD digital television service",
  "advanced codec SD NVOD time-shifted service",
  "advanced codec SD NVOD reference service",
  "advanced codec HD digital television service",
  "advanced codec HD NVOD time-shifted service",
  "advanced codec HD NVOD reference service"
};

char const *
programInfoGetSvcTypeStr (uint8_t status)
{
  return struGetStr (program_info_svc_type_strings, status);
}

char const *
programInfoGetRunningStatusStr (uint8_t status)
{
  return struGetStr (program_info_running_status_strings, status);
}

void
programInfoClear (ProgramInfo * pg)
{
  dvbStringClear (&pg->provider_name);
  dvbStringClear (&pg->service_name);
  dvbStringClear (&pg->bouquet_name);
}

void
programInfoSnd (int sock, ProgramInfo * pg)
{
  ipcSndS (sock, pg->pnr);
  ipcSndS (sock, pg->pid);
  ipcSndS (sock, pg->svc_type);
  ipcSndS (sock, pg->schedule);
  ipcSndS (sock, pg->present_follow);
  ipcSndS (sock, pg->status);
  ipcSndS (sock, pg->ca_mode);
  dvbStringSnd (sock, &pg->provider_name);
  dvbStringSnd (sock, &pg->service_name);
  dvbStringSnd (sock, &pg->bouquet_name);
  ipcSndS (sock, pg->num_es);
}

void
programInfoRcv (int sock, ProgramInfo * pg)
{
  ipcRcvS (sock, pg->pnr);
  ipcRcvS (sock, pg->pid);
  ipcRcvS (sock, pg->svc_type);
  ipcRcvS (sock, pg->schedule);
  ipcRcvS (sock, pg->present_follow);
  ipcRcvS (sock, pg->status);
  ipcRcvS (sock, pg->ca_mode);
  dvbStringRcv (sock, &pg->provider_name);
  dvbStringRcv (sock, &pg->service_name);
  dvbStringRcv (sock, &pg->bouquet_name);
  ipcRcvS (sock, pg->num_es);
}
