#ifndef __config_h
#define __config_h
/**
 *\file config.h
 *\brief contains some sane defaults for various parts of the program
 *
 */
#define REMOTEINTERFACE_PORT 2241
#define BCAST_PORT 5004
///a local multicast group
#define BCAST_ADDR4 "224.0.1.2"
///link local
#define BCAST_ADDR6 "ff02:::1:2"
#define BCAST_ADDR(af_) (((af_)==AF_INET)?BCAST_ADDR4:BCAST_ADDR6)
///default to one hop
#define BCAST_TTL 1
#define GLOB_CFG "/etc/dvbvulture/sa.conf"
//#define DEFAULT_PIDFILE "/var/run/sa.pid"
#define DEFAULT_VTDB "/var/lib/dvbvulture/vt.db"
#define DEFAULT_PGMDB "/var/lib/dvbvulture/pgm.db"
#define DEFAULT_EPGDB "/var/lib/dvbvulture/epg.db"
#define DEFAULT_SCHDB "/var/lib/dvbvulture/sch.db"
#define DEFAULT_SWTCHDB "/var/lib/dvbvulture/swtch.db"
///default MHEG directory
#define DEFAULT_MHEG "/var/lib/dvbvulture/mheg/"
//#define DEFAULT_EPG_BUFSZ 168///<1week hours
#define DEFAULT_REC_TEMPLATE "%n-%t"
#define DEFAULT_REC_PATH "/var/lib/dvbvulture/rec"

#define DEFAULT_SUPER_ADDR4 "192.168.0.2"
//link-local addr(is this valid?)... wil never match...
#define DEFAULT_SUPER_ADDR6 "fe80::2"
#define DEFAULT_SUPER_ADDR(af_) (((af_)==AF_INET)?DEFAULT_SUPER_ADDR4:DEFAULT_SUPER_ADDR6)

///defaults to global scope SAP
#define DEFAULT_SAP_ADDR4 "224.2.127.254"
///link local SAP multicast
#define DEFAULT_SAP_ADDR6 "ff02:::2:7FFE"
#define DEFAULT_SAP_ADDR(af_) (((af_)==AF_INET)?DEFAULT_SAP_ADDR4:DEFAULT_SAP_ADDR6)

#define DEFAULT_SAP_SRC4 "192.168.0.3"
#define DEFAULT_SAP_SRC6 "fe80::3"
#define DEFAULT_SAP_SRC(af_) (((af_)==AF_INET)?DEFAULT_SAP_SRC4:DEFAULT_SAP_SRC6)

#define FADV_INTERVAL 8192
#define DEFAULT_AF AF_INET

/**
because they had to change the listing format for initial tuning files
in 2014...
if this is defined the initial tuning files have to be in .ini format
[CHANNEL]
FREQUENCY=...

If this is not defined the initial tuning files have to be in the old format 
one line per transponder

*/
#define NEW_SCAN_TABLES

#endif
