#ifndef __rs_entry_h
#define __rs_entry_h
#include <stdint.h>
#include <time.h>

/**
 *\file rs_entry.h
 *\brief recording schedule entry datastructures and functions
 *
 *
 */
#define REC_SCHED_SIZE 32       ///<the number of entries in a schedule. We define this here so clients and server have the same idea of this.

/**
 *\brief current state of entry
 */
typedef enum
{
  RS_IDLE,                      ///<not scheduled
  RS_WAITING,                   ///<scheduled, but not started
  RS_RUNNING,                   ///<scheduled, and started
} RSState;

/**
 *\brief type of recording
 */
typedef enum
{
  RST_ONCE,                     ///<single event
  RST_DAY,                      ///<daily event. wd flags tell which days.
  RST_WEEK,                     ///<weekly
  RST_2WEEK,                    ///<every 2weeks
  RST_3WEEK,                    ///<...3
  RST_4WEEK,                    ///<...4
  RST_MONTH                     ///<every month
} RSType;

#define RST_COUNT 7

//weekday flags
#define WD_SUN (1<<0)
#define WD_MON (1<<1)
#define WD_TUE (1<<2)
#define WD_WED (1<<3)
#define WD_THU (1<<4)
#define WD_FRI (1<<5)
#define WD_SAT (1<<6)

//flags
#define RSF_SKIP_VT    1        ///<don't dump vt
#define RSF_SKIP_SPU  (1<<1)    ///<don't dump subtitles
#define RSF_SKIP_DSM  (1<<2)    ///<don't dump dsm-cc object carousel or app streams
#define RSF_AUD_ONLY  (1<<3)    ///<skip anything but audio and PSI. Takes precedence over the upper three flags.
#define RSF_EIT       (1<<4)    ///<use EIT for starting/stopping
//#define RSF_PDC       (1<<4)///<use pdc for starting/stopping

//#define RSF_PRI       (1<<3)///<hi priority. probably use a seperate value for this or skip it altogether
typedef struct
{
  RSState state;
  RSType type;
  uint8_t wd;                   ///<bitwise or of WD_.. flags. only makes sense for RST_DAY
  uint8_t flags;                ///<controls what to store and if we use pdc
  uint8_t pos;                  ///<LNB/SWITCH positions
  uint8_t pol;                  ///<polarisation
  uint32_t freq;                ///<frequency
  uint16_t pnr;                 ///<program number
  time_t start_time;            ///<GMT, please
  time_t duration;              ///<in seconds, I guess
//      uint32_t pil;///<programme identification label
  int32_t evt_id;               ///<event identification for EIT triggered recordings
  char *name;                   ///<event name
} RsEntry;

///returs 1 on error, 0 otherwise
int rsSndEntry (RsEntry * sch, int fd);
///returs 1 on error, 0 otherwise
int rsRcvEntry (RsEntry * sch, int fd);

/**
 *\brief returns true if rec should be started (based on entries' flags, start time and current time)
 */
int rsPending (RsEntry * e);

/**
 *\brief transition from WAITING to RUNNING state
 */
int rsStart (RsEntry * e);

/**
 *\brief return true if rec should be stopped now (RUNNING and duration exceeded)
 */
int rsStopping (RsEntry * e);

/**
 *\brief return true if state==RUNNING
 */
int rsIsRunning (RsEntry * e);

/**
 *\brief reset to RS_IDLE for RST_ONCE or update timestamp for next event and set to RS_WAITING
 *
 *only works for RS_ACTIVE entries
 *\return 1 on error(unknown type or not running)
 */
int rsDone (RsEntry * e);

void rsClearEntry (RsEntry * e);

char const *rsGetStateStr (uint8_t state);
char const *rsGetTypeStr (uint8_t type);
char const *rsGetWdStr (uint8_t wd);
char const *rsGetFlagStr (uint8_t flag);

time_t rsNextTime (RSType tp, uint8_t wd, time_t t);

int rsNeq (RsEntry * a, RsEntry * b);

int rsClone (RsEntry * dest, RsEntry * src);    ///<initialize dest with data from src, event name will be duplicated
RsEntry *rsCloneSched (RsEntry * src, uint32_t src_sz); ///<duplicates entire array of src_size entries
void rsDestroy (RsEntry * sch, uint32_t sz);    ///<clear and free whole array. must be malloc'ed
int rsNeqSched (RsEntry * a, uint32_t a_sz, RsEntry * b, uint32_t b_sz);

#endif //__rs_entry
