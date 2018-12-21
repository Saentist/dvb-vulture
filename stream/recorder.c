#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include "utl.h"
#include "config.h"
#include "recorder.h"
#include "rec_sched.h"
#include "pex.h"
#include "dvb.h"
#include "debug.h"
#include "pidbuf.h"
#include "dmalloc.h"
//#include "tsreader.h"

int DF_REC;
#define FILE_DBG DF_REC

int
recorderPause (Recorder * r)
{
  if (!recorderActive (r))
    return 1;
  taskLock (&r->recording_task);
  r->paused = 1;
  taskUnlock (&r->recording_task);
  return 0;
}

int
recorderResume (Recorder * r)
{
  if (!recorderActive (r))
    return 1;
  taskLock (&r->recording_task);
  r->paused = 0;
  taskUnlock (&r->recording_task);
  return 0;
}


static void *
do_record (void *p)
{
  Recorder *r = p;
  int rv, fadv_ctr = 0;

  taskLock (&r->recording_task);
  r->start_time = time (NULL);
  debugPri (1, "do_record\n");
  while (taskRunning (&r->recording_task))
  {
    taskUnlock (&r->recording_task);
    rv = selectorWaitData (r->port, 300);
    taskLock (&r->recording_task);
    if (rv)
    {
      debugPri (10, "pthread_cond_timedwait: %s\n", strerror (rv));
    }
    else
    {
      uint8_t *d[256];
      int num = 0;
      int i;

      selectorLock (r->sel);
      while ((num < 256) && (d[num] = selectorReadElementUnsafe (r->port)))
        num++;
      selectorUnlock (r->sel);

      for (i = 0; i < num; i++)
      {
        if (selectorEIsEvt (d[i]))
        {
          int j;
          switch (selectorEvtId (d[i]))
          {
          case E_TUNING:
          case E_CONFIG:
            //we're changing frequency, abort recording
            debugPri (1, "Freq change, Stopping recorder\n");
            selectorLock (r->sel);
            for (j = 0; j < num; j++)
              selectorReleaseElementUnsafe (r->sel, d[j]);
            selectorUnlock (r->sel);
            dvbDmxOutRmv (r->o);
//            dvbTsOutputClear (r->dvb, r->port);
            fclose (r->output);
            r->o = NULL;
            r->port = NULL;
            r->sel = NULL;
            r->output = NULL;
            taskUnlock (&r->recording_task);
            return NULL;
            break;
          default:
            break;
          }
        }
        else
        {
          if (!tsIsStuffing (selectorEPayload (d[i])))
          {
            if (!r->paused)
            {
              if (1 != fwrite (selectorEPayload (d[i]), TS_LEN, 1, r->output))
              {
                r->err = 1;
                errMsg ("fwrite error: %s\n", strerror (errno));
              }
              if (fadv_ctr > FADV_INTERVAL)
              {                 //this seems to work. but it is ugly (FADV_INTERVAL is defined in config.h)
                posix_fadvise (fileno (r->output), 0, 0, POSIX_FADV_DONTNEED);
                fadv_ctr = 0;
              }
              fadv_ctr++;
            }
          }
        }
      }
      selectorLock (r->sel);
      for (i = 0; i < num; i++)
      {
        selectorReleaseElementUnsafe (r->sel, d[i]);
      }
      selectorUnlock (r->sel);
    }
  }
  debugPri (1, "Stopping recorder\n");
  dvbDmxOutRmv (r->o);
//  dvbTsOutputClear (r->dvb, r->port);
  fclose (r->output);
  r->o = NULL;
  r->port = NULL;
  r->sel = NULL;
  r->output = NULL;
  taskUnlock (&r->recording_task);
  return NULL;
}

int
recorderGetState (Recorder * r, RState * rs)
{
  if ((!r->output) || (!r->port))
    return 1;
  rs->size = ftello (r->output);
  rs->time = time (NULL) - r->start_time;
  rs->err = r->err;
  r->err = 0;
  return 0;
}


/**
 *\brief POSIX Portable Filename Character Set
 *
 *     The set of 65 characters, which can be used in naming<br>
 *     files on a POSIX-compliant host, that are correctly<br>
 *     processed in all locales.	The set is:<br>
 * <br>
 *     a..z A..Z 0..9 ._-
 */
static void
recorder_clean_filename (char *name)
{
  int i = 0;
  while (name[i])
  {
    if (!(((name[i] >= 'a') && (name[i] <= 'z')) ||
          ((name[i] >= 'A') && (name[i] <= 'Z')) ||
          ((name[i] >= '0') && (name[i] <= '9')) ||
          (name[i] == '_') || (name[i] == '-')))
      name[i] = '_';
    i++;
  }
}

static FILE *
create_rec_file (char *pvdr_name, char *pgm_name, char *evt_name, char *path,
                 char *template)
{
  FILE *rv = NULL;
  char *buf = NULL;
//      char *buf2=NULL;
  char tmstmp[20];
  int i = 0;
  struct stat statbuf;

//      int buf2_sz=256;
  uint32_t idx = 2;
  int st_ok;
  char *p = NULL;
  PexHandler rpl[4] =
    { {'n', pgm_name}, {'e', evt_name}, {'p', pvdr_name}, {'t', tmstmp} };
  uint32_t num_rpl = 4;
  debugMsg ("create_rec_file\n");
  if ((template == NULL) || (strlen (template) < 1) || (path == NULL)
      || (strlen (path) < 1))
    return NULL;

//      buf2=utlCalloc(1,buf2_sz);
  snprintf (tmstmp, 20, "%u", (unsigned int) time (NULL));
//      rpl[3].replacement=buf2;
  buf = pexParse (rpl, num_rpl, template);      //BUG?
  if (NULL == buf)
  {
//              utlFAN(buf2);
    return NULL;
  }
  recorder_clean_filename (buf);        //BUG?
  p = utlCalloc (1, strlen (buf) + strlen (path) + 40); //trailing NUL, slash, .ts and possibly one more number
  if (!p)
  {
    utlFAN (buf);
//              utlFAN(buf2);
    return NULL;
  }
  debugMsg ("prepending path %s to %s\n", path, buf);
  memmove (p, path, strlen (path));
  i = strlen (path);
  if (p[i - 1] != '/')
  {
    p[i] = '/';
    p[i + 1] = '\0';
  }
  debugMsg ("strcat\n");
  strcat (p, buf);
  utlFAN (buf);
  buf = p;
  debugMsg ("prepended path %s\n", buf);
  strcat (p, ".ts");
  debugMsg ("appended ext %s\n", buf);

//      buf="/var/lib/dvbvulture/rec/test.ts";
  debugMsg ("Trying to stat %s\n", buf);
  st_ok = !stat (buf, &statbuf);
  if (st_ok)
  {
    i = strlen (buf) - 3;       // ->.ts
//              p=utlRealloc(buf,strlen(buf)+1+20);//make room for number
//              if(!p)
//              {//it failed
//                      utlFAN(buf);
//                      utlFAN(buf2);
//                      return NULL;
//              }
//              buf=p;
    buf[strlen (buf) - 3] = '\0';       //cut off ext
    while ((st_ok) && (idx < 20))
    {
      debugMsg ("..failed\n");
//                      snprintf(buf2,buf2_sz,"-%u",idx);
//                      strcpy(buf+i,buf2);//append number
      snprintf (tmstmp, 20, "-%" PRIu32, idx);
      strcpy (buf + i, tmstmp); //append number
      strcat (buf, ".ts");      //append suffix
      debugMsg ("Trying to stat %s\n", buf);
      st_ok = !stat (buf, &statbuf);
      idx++;
    }
  }

  if (!st_ok)
  {
    debugMsg ("Trying to open %s\n", buf);
    rv = fopen (buf, "w");
    if (!rv)
    {
      errMsg ("fopen error %s\n", strerror (errno));
      utlFAN (buf);
      return NULL;
    }
  }
  utlFAN (buf);
  return rv;
}

static int
recorder_is_dsm (uint8_t type)
{
  return ((type == 0x08) ||
          (type == 0x0A) ||
          (type == 0x0B) || (type == 0x0C) || (type == 0x0D));
}

//int recorderStart(Recorder * r,char * fmt,char * evt_name,char * pgm_name,char * pvdr_name,RcFile * cfg,Selector *input,DListE *port)
int
recorderStart (Recorder * r, char *evt_name, RcFile * cfg, dvb_s * dvb,
               PgmDb * pgmdb, uint16_t pnr, uint8_t flags)
{
  char *path = DEFAULT_REC_PATH;
  char *tmp;
  char *fmt = DEFAULT_REC_TEMPLATE;
  char *pgm_name;
  char *pvdr_name;
//  int i, j;
  Section *s;
  ProgramInfo *pgi;
  int num_pgmi;
  SvcOut *port;
  uint16_t func;
//  DListE *port;
//  uint16_t num_pids;
//  uint16_t *pids;
//  uint16_t pcr_pid = 0x1fff;
//      Selector * input;
//  ES *stream_infos;
//  int num_es;

  if (recorderActive (r))
    return 1;

  memset (r, 0, sizeof (Recorder));
  s = rcfileFindSec (cfg, "REC");
  if (s && (!rcfileFindSecVal (s, "path", &tmp)))
  {
    path = tmp;
  }
  if (s && (!rcfileFindSecVal (s, "format", &tmp)))
  {
    fmt = tmp;
  }
  debugMsg ("pgmdbFindPgmPnr\n");

  pgi =
    pgmdbFindPgmPnr (pgmdb, dvb->pos, dvb->freq, dvb->pol, pnr, &num_pgmi);
  if (NULL == pgi)
  {
    errMsg ("Failed to get Program information\n");
    return 1;
  }                             /*need to get only pgm/provider name..  perhaps verify that pnr actually exists? */

  if (taskInit (&r->recording_task, r, do_record))
  {
    programInfoClear (pgi);
    utlFAN (pgi);
    return 1;
  }

  pvdr_name = dvbStringToAscii (&pgi->provider_name);
  pgm_name = dvbStringToAscii (&pgi->service_name);

  r->output = create_rec_file (pvdr_name, pgm_name, evt_name, path, fmt);

  utlFAN (pvdr_name);
  utlFAN (pgm_name);

  if (!r->output)
  {
    errMsg ("opening output file failed\n");
    programInfoClear (pgi);
    utlFAN (pgi);
    return 1;
  }

  /*
     see if it's txt and we don't skip it or
     its sub and we don't skip it or
     it's neither subtitle nor vt pid
   */

  func = FUNC_PSI;
  if (flags & RSF_AUD_ONLY)
  {
    func |= FUNC_AUDIO;
  }
  else
  {
    if ((flags & RSF_SKIP_VT) || (flags & RSF_SKIP_SPU))
    {
      if (!(flags & RSF_SKIP_VT))
        func |= FUNC_VT;
      if (!(flags & RSF_SKIP_SPU))
        func |= FUNC_SPU;
      func |= FUNC_AUDIO;
      func |= FUNC_VIDEO;       //NOT SURE IF WORKS.. would want to stream all but exclude VT/SPU... this is different..
    }
    else
      func = FUNC_ALL;
  }
  debugMsg ("getting Ts output\n");
  port = dvbDmxOutAdd (dvb);

  if (port)
  {
    dvbDmxOutMod (port, 1, &pnr, &func, 0, NULL);
    r->dvb = dvb;
    r->sel = svtOutSelector (port);
    r->o = port;
    r->port = svtOutSelPort (port);
    debugMsg ("starting task\n");
    taskStart (&r->recording_task);
  }
  else
  {
    programInfoClear (pgi);
    utlFAN (pgi);
    fclose (r->output);
    return 1;
  }
  programInfoClear (pgi);
  utlFAN (pgi);
  r->pnr = pnr;
  return 0;
}

void
recorderClearErr (Recorder * r)
{
  r->err = 0;
}

int
recorderErr (Recorder * r)
{
  return r->err;
}

int
recorderStop (Recorder * r)
{
  if (!recorderActive (r))
    return 1;
  taskClear (&r->recording_task);
  return 0;
}
