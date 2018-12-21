#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <malloc.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <inttypes.h>
#include "debug.h"
#include "dvb.h"
#include "utl.h"
#include "rcfile.h"
#include "dmalloc.h"
#include "bits.h"
#include "vtdb.h"
#include "pgmdb.h"
#include "in_out.h"
#include "ipc.h"
#include "opt.h"
#include "help.h"
//#include "buffer.h"
//#include "distributor.h"
#include "config.h"
#include "pgmstate.h"
#include "server.h"
#ifdef _REENTRANT
#include <pthread.h>
static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif


/**
 *\file main.c
 *\brief The stream agent is responsible for distributing the 
 * data stream from the tv card or file.
 * 
 * handles background tasks such as epg, videotext buffering/refresh and recording schedules.
 * Runs as a daemon and doesn't need access to X server.
 * The remote connects to it for displaying content and setting parameters.
 * multiple remotes may connect (and display) but only one may set parameters.
 * video data is sent as standard rtp stream preferrably on a broadcast address.
 * control information, teletext and EPG are transferred over different per-client TCP connections.
 * If there are no clients connected, the stream agent shall stop broadcasting to conserve bandwidth.
 * in no broadcast mode it shall do maintenance tasks such as keeping videotext buffer and program guides up to date.
 *
 *
 */
int DF_MAIN;
#define FILE_DBG DF_MAIN

void clean_exit (int signum);


void
detach (char *pidfile)
{
  pid_t pid, mypid, sid;
  FILE *f;
  pid = fork ();
  if (pid < 0)
  {
    errMsg ("fork failed\n");
    exit (1);
  }
  if (pid > 0)
  {
    exit (0);
  }
  sid = setsid ();              //become a session group leader
  if (sid < 0)
  {
    errMsg ("setsid failed\n");
    exit (1);
  }

  mypid = fork ();              //supposedly this has to be done so new process is not session group leader... can never acquire a terminal and is reparented to init
  if (mypid < 0)
  {
    errMsg ("2nd fork failed\n");
    exit (1);
  }

  if (mypid > 0)
  {                             //parent
    debugMsg ("opening pidfile %s\n", pidfile);
    f = fopen (pidfile, "w");
    if (!f)
    {
      errMsg ("Couldn't open pidfile %s\n", pidfile);
      exit (1);
    }
    fprintf (f, "%u\n", mypid);
    fclose (f);
    exit (0);
  }

  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
  if ((-1 == open ("/dev/null", O_RDONLY)) ||
      (-1 == open ("/dev/null", O_WRONLY)) ||
      (-1 == open ("/dev/null", O_WRONLY)))
    errMsg ("something failed to open\n");
  chdir ("/");
}

void
finish (void *pidfile)
{
  unlink ((char *) pidfile);
}

int
CUdetach (char *pidfile, CUStack * s)
{
  int rv = 0;
  detach (pidfile);
  cuStackFail ((int) rv, (void *) pidfile, finish, s);
  return rv;
}

void
CUrcfileClear (void *data)
{
  RcFile *f = (RcFile *) data;
  rcfileClear (f);
}

int
CUrcfileParse (char *conf_file, RcFile * data, CUStack * s)
{
  int rv = rcfileParse (conf_file, data);
  cuStackFail ((int) rv, (void *) data, CUrcfileClear, s);
  return rv;
}

volatile sig_atomic_t TERMINATING = 0;
int SRV_SOCKFD = -1;
#define shdnAndInvOnc(fd__) (((fd__)!=-1)?(shutdown(fd__,SHUT_RDWR),(fd__)= -1):0)

///one thread for every connected client. They all sync on the conn_mutex.
void *
server (void *v)
{
  DListE *e;
  PgmState *p;
  int rv;
  Connection *c;
  struct pollfd pfd[1];
  uint8_t cmd;
  e = v;
  debugMsg ("server ptr:%p\n", e);
  c = (Connection *) e;
  debugMsg ("server payload ptr:%p\n", c);
  nice (c->niceness);
  p = c->p;
  debugMsg ("server descriptor:%d\n", c->sockfd);

  pthread_mutex_lock (&p->conn_mutex);

  pfd[0].fd = c->sockfd;
  pfd[0].events = POLLIN | POLLPRI;
  while (!c->dead)
  {
    pthread_mutex_unlock (&p->conn_mutex);
    rv = (poll (pfd, 1, 600) > 0);
    /*
       when one box(client) crashed, stale connection stayed active and locked out others...(no FIN?)
       would I get notified of this by kernel via some POLL flag, or do I have to send bytes continously?...
       killing with KILL or normal(TERM) does not show this behaviour...
       enabled SO_KEEPALIVE...
       will probably still crash the server(SIGPIPE)...
     */
    if ((rv) && (pfd[0].revents & POLLHUP))
    {
      debugMsg ("POLLHUP. Ending connection\n");
      pthread_mutex_lock (&p->conn_mutex);
      c->dead = 1;
    }
    else if ((rv) && (pfd[0].revents & POLLIN))
    {
      errno = 0;
      //I get reads of length zero with no error for an ending connection
      if (read (c->sockfd, &cmd, sizeof (cmd)) != sizeof (cmd))
      {
        errMsg ("Connection read() returned: %s. Ending connection.\n",
                strerror (errno));
        pthread_mutex_lock (&p->conn_mutex);
        c->dead = 1;
      }
      else
      {
//                                      uint8_t rtn_eauth=SRV_E_AUTH;
        pthread_mutex_lock (&p->conn_mutex);
        switch (cmd)
        {
        case LIST_TP:
          srvListTp (c);
          break;
        case SCAN_TP:
          srvScanTp (c);
          break;
        case LIST_PG:
          srvListPg (c);
          break;
        case TUNE_PG:
          srvTunePg (c);
          break;
        case SET_SWITCH:
          srvSetSwitch (c);
          break;
        case GET_SWITCH:
          srvGetSwitch (c);
          break;
        case TP_FT:
          srvTpFt (c);
          break;
        case GET_STATS:
          srvGetDvbStats (c);
          break;
        case NUM_POS:
          srvNumPos (c);
          break;
        case ADD_TP:
          srvAddTp (c);
          break;
        case DEL_TP:
          srvDelTp (c);
          break;
        case EPG_GET_DATA:     //get data for specified pnr and time interval
          srvEpgGetData (c);
          break;
        case GO_INACTIVE:      //release permissions but stay attached
          srvGoInactive (c);
          break;
        case GO_ACTIVE:        //reacquire permissions if possible
          srvGoActive (c);
          break;
        case VT_GET_SVC_PK:    //get svc specific packets
          srvVtGetSvcPk (c);
          break;
        case VT_GET_MAG_PK:    //get magazine specific packets
          srvVtGetMagPk (c);
          break;
        case VT_GET_PAG_PK:    //get a page
          srvVtGetPagPk (c);
          break;
        case VT_GET_PNRS:      //list vt pids and datastreams for transponder
          srvVtGetPnrs (c);
          break;
        case VT_GET_PIDS:      //list vt pids and datastreams for program number
          srvVtGetPids (c);
          break;
        case VT_GET_SVC:       //list vt pids and datastreams for program number
          srvVtGetSvc (c);
          break;
        case REC_PGM:
          srvRecPgm (c);
          break;
        case REC_PGM_STOP:
          srvRecPgmStop (c);
          break;
        case REC_SCHED:
          srvRecSched (c);
          break;
        case GET_SCHED:
          srvGetSched (c);
          break;
        case SET_FCORR:
          srvSetFcorr (c);
          break;
        case GET_FCORR:
          srvGetFcorr (c);
          break;
        case ABORT_REC:
          srvAbortRec (c);
          break;
        case ADD_SCHED:
          srvAddSched (c);
          break;
        case SRV_TSTAT:
          srvTpStatus (c);
          break;
        case SRV_RSTAT:
          srvRstat (c);
          break;
        case SRV_USTAT:
          srvUstat (c);
          break;
          /*                      case SRV_TST:
             shdnAndInvOnc(SRV_SOCKFD);
             TERMINATING=1;
             break; */
        default:
          debugMsg ("unknown command %" PRIu8 "\n", cmd);
          c->dead = 1;
          break;
        }
      }
    }
    else
      pthread_mutex_lock (&p->conn_mutex);
  }
  errMsg ("connection ended\n");
  connectionClear (c);
  errMsg ("connection cleared\n");
  dlistRemove (&p->conn_active, &c->e);
  if (c->sockfd != -1)
  {
    shutdown (c->sockfd, SHUT_RDWR);
    close (c->sockfd);
    c->sockfd = -1;
  }
  dlistInsertLast (&p->conn_dead, &c->e);

  taskLock (&p->recorder_task.t);       //can recorder_task go active here... need to lock
  if ((NULL == dlistFirst (&p->conn_active)) && (!recTaskActive (&p->recorder_task)) && (!TERMINATING)) //no one else is using frontend
  {
    //clear frontend
    dvbUnTune (&p->dvb);
    while (!pgmdbUntuned (&p->program_database))
    {                           //wait for pgmdb..
      debugPri (1, "pgmdbUntuned\n");
      usleep (500000);
    }
    //start our offline task
    offlineTaskStart (&p->ot);
  }
  taskUnlock (&p->recorder_task.t);
  pthread_mutex_unlock (&p->conn_mutex);        //order of those is important
  return NULL;
}

void
create_connection (int fd, PgmState * p, struct sockaddr_storage *peer_addr,
                   socklen_t addrlen NOTUSED, int niceness)
{
  DListE *e;
  Connection *c;
  uint32_t id = CONN_ID;
  size_t s = sizeof (Connection);

  debugMsg ("trying to allocate connection with size:%u\n", s);
  e = utlCalloc (1, s);
  if (NULL == e)
  {
    errMsg ("error allocating connection\n");
    return;
  }
  if (ipcSndS (fd, id))
  {
    errMsg ("write: %s\n", strerror (errno));
    utlFAN (e);
    return;
  }

  debugMsg ("ptr:%p\n", e);

  debugMsg ("trying to get payload\n");
  c = (Connection *) e;
  debugMsg ("connection ptr:%p\n", c);
  connectionInit (c, fd, p, peer_addr, niceness);       //should use addrlen
  debugMsg ("descriptor:%d\n", fd);
  pthread_mutex_lock (&p->conn_mutex);
  debugMsg ("stopping offline task\n");
  offlineTaskStop (&p->ot);
  debugMsg ("trying to queue connection data\n");
  dlistInsertLast (&p->conn_active, &c->e);
  debugMsg ("done queueing connection data\n");
  debugMsg ("trying to create thread\n");
  pthread_create (&c->thread, NULL, server, c);
  debugMsg ("done creating thread\n");
  pthread_mutex_unlock (&p->conn_mutex);
}

void
cleanup_connections (PgmState * p)      //do join on finalized connection threads
{
  DListE *e;
  Connection *c;
  pthread_mutex_lock (&p->conn_mutex);
  while ((e = dlistFirst (&p->conn_dead)))
  {
    debugMsg ("Cleaning Connection\n");
    dlistRemove (&p->conn_dead, e);
    c = (Connection *) e;
    pthread_mutex_unlock (&p->conn_mutex);
    pthread_join (c->thread, NULL);
    utlFAN (e);
    pthread_mutex_lock (&p->conn_mutex);
  }
  pthread_mutex_unlock (&p->conn_mutex);
}

void
kill_conn (void *v NOTUSED, DListE * e)
{
  Connection *c;
  c = (Connection *) e;
  c->dead = 1;                  //set them all dead
}

int
abort_connections (PgmState * p)
{
  int rv = 0, i;
  pthread_mutex_lock (&p->conn_mutex);
  dlistForEach (&p->conn_active, p, kill_conn);
  i = 0;
  while (dlistFirst (&p->conn_active) && (i < p->abort_poll_count))
  {
    pthread_mutex_unlock (&p->conn_mutex);
    usleep (p->abort_timeout_us);
    i++;
    pthread_mutex_lock (&p->conn_mutex);
  }
  pthread_mutex_unlock (&p->conn_mutex);
  if (i == p->abort_poll_count)
    rv = 1;
  cleanup_connections (p);
  return rv;
}

static int
control_run (PgmState * p)
{
  uint16_t port;
  long v;
  int backlog, niceness = 0;
  int af;
  sigset_t b;
  dlistInit (&p->conn_active);
  dlistInit (&p->conn_dead);
  p->master = NULL;
  port = REMOTEINTERFACE_PORT;
  if (!rcfileFindValInt (&p->conf, "SERVER", "port", &v))
  {
    port = v;
  }
  debugMsg ("using server port: %" PRIu16 "\n", port);

  niceness = 0;
  if (!rcfileFindValInt (&p->conf, "SERVER", "niceness", &v))
  {
    niceness = v;
  }
  debugMsg ("using server niceness: %hu\n", niceness);

  backlog = 20;
  if (!rcfileFindValInt (&p->conf, "SERVER", "backlog", &v))
  {
    backlog = v;
  }
  debugMsg ("using backlog: %d\n", backlog);

  af = DEFAULT_AF;
  if (!rcfileFindValInt (&p->conf, "SERVER", "af", &v))
  {
    if (v == 6)
      af = AF_INET6;
    if (v == 4)
      af = AF_INET;
  }
  debugMsg ("using af: %s\n", (af == AF_INET) ? "AF_INET" : "AF_INET6");

  p->super_addr = DEFAULT_SUPER_ADDR (af);
  rcfileFindVal (&p->conf, "SERVER", "super_addr", &p->super_addr);

  sigemptyset (&b);
  sigaddset (&b, SIGTERM);
  sigaddset (&b, SIGINT);
  pthread_sigmask (SIG_BLOCK, &b, NULL);        //block signal handler while working on file descriptor?

  SRV_SOCKFD = ioTcpSocket (af, port);
  if (SRV_SOCKFD < 0)
  {
    errMsg ("Failed to create server socket!\n");
    pthread_sigmask (SIG_UNBLOCK, &b, NULL);
    return 1;
  }

  debugMsg ("listening...\n");
  if (listen (SRV_SOCKFD, backlog))
  {
    errMsg ("listen: %s\n", strerror (errno));
    shdnAndInvOnc (SRV_SOCKFD);
    pthread_sigmask (SIG_UNBLOCK, &b, NULL);
    return 1;
  }
  pthread_sigmask (SIG_UNBLOCK, &b, NULL);
  pthread_mutex_init (&p->conn_mutex, NULL);
  debugMsg ("starting server\n");
  taskLock (&p->recorder_task.t);       //can recorder_task go active here... need to lock
  if (!recTaskActive (&p->recorder_task))       //no one else is using frontend
  {
    offlineTaskStart (&p->ot);
  }
  taskUnlock (&p->recorder_task.t);
  do
  {
    int afd;
    struct sockaddr_storage peer_addr;
    socklen_t addrlen =
      ((af ==
        AF_INET) ? sizeof (struct sockaddr_in) : sizeof (struct
                                                         sockaddr_in6));
//this may block, so dead connections linger too long.. TODO: use a nonblocking socket+usleep...
    afd = accept (SRV_SOCKFD, (struct sockaddr *) &peer_addr, &addrlen);
    if (afd != -1)
    {
      int optval = 1;
      socklen_t optlen = sizeof (optval);
      debugMsg ("creating connection\n");
      /* Set the keepalive option active */
      if (setsockopt (afd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
      {
        errMsg ("setsockopt(): %s", strerror (errno));
      }
      /* 2 minutes timeout */
      optval = 120;
      if (setsockopt (afd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen) < 0)
      {
        errMsg ("setsockopt(): %s", strerror (errno));
      }
      create_connection (afd, p, &peer_addr, addrlen, niceness);
    }
    else
    {
      errMsg ("accept: %s\n", strerror (errno));
    }
    cleanup_connections (p);    //do join on finalized connection threads
  }
  while (!TERMINATING);

  abort_connections (p);        //do finalize and join connection threads
  pthread_mutex_destroy (&p->conn_mutex);
  return 0;
}

int
xprintf (void *d NOTUSED, const char *f, int l, int pri, const char *template,
         ...)
{
#ifdef DMALLOC_DISABLE
  int n, size = 100;
  char *p, *np;
#endif
  va_list ap;
#ifndef DMALLOC_DISABLE
  char buf[256];
//  extern char *program_invocation_short_name;
#endif

#ifdef _REENTRANT
  pthread_mutex_lock (&debug_mutex);
#endif

#ifdef DMALLOC_DISABLE

  if ((p = utlMalloc (size)) == NULL)
    return 0;

  while (1)
  {
    /* Try to print in the allocated space. */
    va_start (ap, template);
    n = vsnprintf (p, size, template, ap);
    va_end (ap);
    /* If that worked, return the string. */
    if (n > -1 && n < size)
      break;
    /* Else try again with more space. */
    if (n > -1)                 /* glibc 2.1 */
      size = n + 1;             /* precisely what is needed */
    else                        /* glibc 2.0 */
      size *= 2;                /* twice the old size */
    if ((np = realloc (p, size)) == NULL)
    {
      utlFAN (p);
      return 0;
    }
    else
    {
      p = np;
    }
  }

  syslog (pri, "File %s, Line %d: %s", f, l, p);
  utlFAN (p);
#else

  snprintf (buf, 256, "Thread %p, File %s, Line %d:",
            (void *) pthread_self (), f, l);
  buf[255] = '\0';
//  dmalloc_message ("%s, ", program_invocation_short_name);
//  dmalloc_message ("File %s, ", f);
//  dmalloc_message ("Line %d: ", l);
  dmalloc_message (buf);
  va_start (ap, template);
  dmalloc_vmessage (template, ap);
  va_end (ap);

#endif

#ifdef _REENTRANT
  pthread_mutex_unlock (&debug_mutex);
#endif
  return 0;
}


int
xprintfnd (void *d NOTUSED, const char *f, int l, int pri NOTUSED,
           const char *template, ...)
{
  va_list ap;

#ifdef _REENTRANT
  pthread_mutex_lock (&debug_mutex);
#endif
  printf ("File %s, ", f);
  printf ("Line %d: ", l);
  va_start (ap, template);
  vprintf (template, ap);
  va_end (ap);
#ifdef _REENTRANT
  pthread_mutex_unlock (&debug_mutex);
#endif
  return 0;
}

void
tuning_success (uint32_t pos, uint32_t freq, uint32_t srate, uint8_t pol,
                void *user)
{
  PgmState *p = user;
  TuneDta td;
  /*
     called when tuning successful
     dvb mutex is locked and dvb task stopped
     here we wait for pgmdb to be up to date

     lock net_writer,vtdb as they depend on pgmdb being up to date
     send tuned event
     wait for pgmdb to tune
     unlock tasks
   */

  netWriterLock (&p->net_writer);
#ifdef WITH_VT
  vtdbLock (&p->videotext_database);
#endif
  td.pos = pos;
  td.freq = freq;
  td.srate = srate;
  td.pol = pol;
//perhaps we should check for p->program_database.tuned==0 first?
  selectorSndEvtSz (&p->dvb.ts_dmx.s, E_TUNED, td);
  while (!pgmdbTuned (&p->program_database, pos, freq, pol))
  {
    debugPri (1, "!pgmdbTuned\n");
    usleep (500000);
  }
#ifdef WITH_VT
  vtdbUnlock (&p->videotext_database);
#endif
  netWriterUnlock (&p->net_writer);
}

//DF_MAIN is further up in this file
extern int DF_DVB;
extern int DF_EIS;
extern int DF_EPGDB;
extern int DF_SAP;
extern int DF_NETW;
extern int DF_NIS;
extern int DF_PAS;
extern int DF_PES_EX;
extern int DF_PGMDB;
extern int DF_PGM_STATE;
extern int DF_PMS;
extern int DF_PMS_CACHE;
extern int DF_REC_SCHED;
extern int DF_REC_TSK;
extern int DF_REC;
extern int DF_SDS;
extern int DF_SEC_EX;
extern int DF_SERVER;
extern int DF_TABLE;
extern int DF_TASK;
extern int DF_VTEX;
extern int DF_VTDB;
extern int DF_SELECTOR;
extern int DF_MAIN;
extern int DF_OFF_TSK;
extern int DF_TDT;
extern int DF_SEC_COMMON;
extern int DF_SW_SET;
extern int DF_EIT_REC;
extern int DF_SVT;
extern int DF_PIDBUF;
extern int DF_MHEG;

/*
 *the main function
 *cmdline arguments to process
 *-cfg <configfile>
 *make the daemon use the specified configfile instead /etc/dvbvulture/sa.conf
 *-p <pidfile>
 *use different pidfile instead of "/var/run/dvbv-stream.pid"
 *-d <modulename> or "all" 
 *enables debug flags for specified module. multiple -d possible and cumulative.
 */
int
main (int argc, char *argv[])
{
  char *value;
  long tmp;
  int i;
  char *cfgfile = GLOB_CFG;
  char pidf[257];
  PgmState p;
  int (*msg_out) (void *d NOTUSED, const char *f, int l, int pri,
                  const char *template, ...) = NULL;
  struct sigaction new_action;  //, old_action;
//      char *tz;
//this is for the timestamps to be UTC(they really have to)
//      tz = getenv("TZ");
  setenv ("TZ", "", 1);
  tzset ();                     //better set this globally, or use mutex, else there's race conditions

  strncpy (pidf, "/var/run/dvbv-stream.pid", 256);

  p.pidfile = pidf;
  msg_out = xprintf;

  for (i = 1; i < argc; i++)
  {
    if (!strcmp (argv[i], "-nd"))
    {
      msg_out = xprintfnd;
    }
  }

  debugSetFunc (NULL, msg_out, 69);
  DF_DVB = debugAddModule ("dvb");      //The dvb input. Verbosities 1-3.
  DF_EIS = debugAddModule ("eis");      //Event information sections. Verbosities 1 and 2.
  DF_EPGDB = debugAddModule ("epgdb");  //EPG database. Verbosities 1-7.
  DF_OFF_TSK = debugAddModule ("off_tsk");      //offline datafile maintenance task. Verbosity 1.
  DF_SAP = debugAddModule ("sap");      //Service Announcement Protocol. Verbosity 1.
  DF_NETW = debugAddModule ("net_writer");      //RTP Net writer. Verbosity 1.
  DF_NIS = debugAddModule ("nis");      //Network Information Sections. Verbosity 1.
  DF_PAS = debugAddModule ("pas");      //Programme Association Sections. Verbosity 1.
  DF_PES_EX = debugAddModule ("pes_ex");        //Program Elementary Stream extractor. Verbosity 1. Just one message here.
  DF_PGMDB = debugAddModule ("pgmdb");  //Services database. Verbosity 1.
  DF_PGM_STATE = debugAddModule ("pgmstate");   //Misc application state. Verbosity 1.
  DF_PMS = debugAddModule ("pms");      //Programme Map Sections. Verbosity 10,11,20. Very noisy.
  DF_PMS_CACHE = debugAddModule ("pms_cache");  //PMS cache. Verbosity 1.
  DF_REC_SCHED = debugAddModule ("rec_sched");  //Recording schedule(and file). Verbosity 1.
  DF_REC_TSK = debugAddModule ("rec_tsk");      //Recorder task(timed). Verbosity 1-2.
  DF_REC = debugAddModule ("recorder"); //Recorder core. Verbosity 1-10.
  DF_SDS = debugAddModule ("sds");      //Service Description Section. Verbosity 1.
  DF_SEC_EX = debugAddModule ("sec_ex");        //PSI section extractor. Verbosity 1.
  DF_SERVER = debugAddModule ("server");        //interface to dvbv-nr. Verbosity 1.
  DF_TABLE = debugAddModule ("table");
  DF_TASK = debugAddModule ("task");    //Task management. Verbosity 1.
  DF_VTEX = debugAddModule ("vt_extractor");    //Videotext extraction. Verbosity 1.
  DF_VTDB = debugAddModule ("vtdb");    //Videotext storage. Verbosity 1-3.
  DF_SELECTOR = debugAddModule ("selector");    //Software demuxer/router/event queue. Verbosity 1.
  DF_TDT = debugAddModule ("tdt");      //Time and date table. Verbosity 1.
  DF_SEC_COMMON = debugAddModule ("sec_common");        //section CRC check... Verbosity 1.
  DF_SW_SET = debugAddModule ("sw_set");        //Antenna/Switch setup. Verbosity 1.
  DF_EIT_REC = debugAddModule ("eit_rec");      //Recording management threads. Verbosity 1.
  DF_SVT = debugAddModule ("svt");      //service tracking software demuxer. Verbosity 1 ,10
  DF_PIDBUF = debugAddModule ("pidbuf");        //pid buffer dumps. Verbosity 1.
  DF_MHEG = debugAddModule ("mheg");    //mheg carousel downloader.
  DF_MAIN = debugAddModule ("main");
  debugSetFlags (argc, argv, "-d");


  debugMsg ("argc %d\n", argc);
  if (find_opt (argc, argv, "-sbt"))
  {
    debugMsg ("Stopping on backtrace\n");
    DebugSBT = true;
  }

  if (find_opt (argc, argv, "-h") || find_opt (argc, argv, "--help"))
  {
    fprintf (stderr, "%s\n", help_txt);
    fprintf (stderr,
             "For more information try the texinfo manual: info dvbv-stream\n");
    return 0;
  }

  if ((value = find_opt_arg (argc, argv, "-cfg")))
  {
    cfgfile = value;
  }

  if ((value = find_opt_arg (argc, argv, "-p")))
  {
    p.pidfile = value;
  }

  if (!cuStackInit (&p.destroy))
    return 1;

  if (!find_opt (argc, argv, "-nd"))
    if (CUdetach (p.pidfile, &p.destroy))
      return 1;

  //mallopt(M_MMAP_MAX,0);//disable use of mmap();


  if ((!rcfileExists (cfgfile)) && (0 != rcfileCreate (cfgfile)))
  {
    errMsg ("rc file %s does not exist and can not be created\n", cfgfile);
    cuStackCleanup (&p.destroy);
    return 1;
  }

  if (CUrcfileParse (cfgfile, &p.conf, &p.destroy))
  {
    errMsg ("cfg file parse error\n");
    return 1;
  }

  p.abort_timeout_us = 70000;
  if (!rcfileFindValInt (&p.conf, "SERVER", "abort_timeout_us", &tmp))
  {
    p.abort_timeout_us = tmp;
  }

  p.abort_poll_count = 10;
  if (!rcfileFindValInt (&p.conf, "SERVER", "abort_poll_count", &tmp))
  {
    p.abort_poll_count = tmp;
  }

  if (CUdvbInit (&p.dvb, &p.conf, tuning_success, &p, &p.destroy))
    return 1;

  if (CUpgmdbInit (&p.program_database, &p.conf, &p.dvb, &p.destroy))
    return 1;

  if (CUepgdbInit
      (&p.epg_database, &p.conf, &p.dvb, &p.program_database, &p.destroy))
    return 1;

  if (CUsapInit
      (&p.sap_announcer, &p.conf, &p.dvb, &p.program_database,
       &p.epg_database, &p.destroy))
    return 1;

#ifdef WITH_VT
  if (CUvtdbInit
      (&p.videotext_database, &p.conf, &p.dvb, &p.program_database,
       &p.destroy))
    return 1;
#endif
#ifdef WITH_MHEG
  if (CUmhegInit (&p.mheg, &p.conf, &p.dvb, &p.program_database, &p.destroy))
    return 1;
#endif

  if (CUnetWriterInit
      (&p.net_writer, &p.conf, &p.dvb, &p.program_database, &p.destroy))
    return 1;

  if (CUrecTaskInit (&p.recorder_task, &p.conf, &p, &p.destroy))
    return 1;

  if (CUofflineTaskInit (&p.ot, &p.conf, &p, &p.destroy))
    return 1;

  new_action.sa_handler = clean_exit;
  sigemptyset (&new_action.sa_mask);
  new_action.sa_flags = 0;

  sigaction (SIGTERM, &new_action, NULL);
  sigaction (SIGINT, &new_action, NULL);

  epgdbStart (&p.epg_database);
#ifdef WITH_VT
  vtdbStart (&p.videotext_database);
#endif
#ifdef WITH_MHEG
  mhegStart (&p.mheg);
#endif
  pgmdbStart (&p.program_database);
  sapStart (&p.sap_announcer);
  netWriterStart (&p.net_writer);
  recTaskStart (&p.recorder_task);
  if (control_run (&p))
  {
    cuStackCleanup (&p.destroy);
    return 1;
  }
  cuStackCleanup (&p.destroy);
  return 0;
}

/**
 *\brief make the program exit cleanly on sigterm
 */
void
clean_exit (int signum NOTUSED)
{
  TERMINATING = 1;
  shdnAndInvOnc (SRV_SOCKFD);   //shutdown supposedly works in here
}
