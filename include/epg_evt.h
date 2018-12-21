#ifndef __epg_evt_h
#define __epg_evt_h
#include <stdint.h>
#include <wchar.h>
#include "list.h"
/**
 *\file epg_evt.h
 *\brief EPG event handling
 *
 *Used for interpreting raw events.
 *
 *The PIL stuff never really worked...
 *unsure why..
 */

///not really used anymorem
typedef struct
{
  uint16_t evt_id;
  time_t start;
  time_t duration;
  uint8_t running_status;
  uint8_t ca_mode;
  uint8_t content1;             ///<the first content descriptors nibble1
  uint8_t content2;             ///<the first content descriptors nibble2
  uint8_t rating;               ///<first parental rating descriptor
  char *name;                   ///<what's the deal with the item description thingy?
  char *description;
} EVT;

typedef struct
{
  uint32_t num_events;          ///<how many events are there?
  uint8_t **events;             ///<each one points at an unparsed event
} EpgSchedule;

typedef struct
{
  uint32_t num_pgm;             ///<how many lists are here
  uint16_t *pgm_nrs;            ///<array of num_pgm integers telling program number
  EpgSchedule *sched;           ///<array of schedules. One for each pgm
} EpgArray;

void epgArrayClear (EpgArray * a);
void epgArrayDestroy (EpgArray * a);    ///<also free()s a itself
void evtClear (EVT * e);
uint16_t evtGetSize (uint8_t * ev_p);
char *evtGetName (uint8_t * ev_p);
char *evtGetDesc (uint8_t * ev_p);
wchar_t *evtGetNameW (uint8_t * ev_p);
wchar_t *evtGetDescW (uint8_t * ev_p);
/**
 *\brief get the events' start
 *
 *FIXME not a real time_t unix timestamp. just a hack to roll mjd and starting time into one value.
 */
time_t evtGetStart (uint8_t * ev_start);
time_t evtGetDuration (uint8_t * ev_start);
uint16_t evtGetId (uint8_t * ev_start); ///<get event id
uint16_t *evtGetCd (uint8_t * ev_start, unsigned *sz_ret);      ///<get content descriptors. sz_ret is number of shorts returned. returns NULL on error.
///not running
#define EIT_RST_NOT 1
///starts in a few seconds (e.g. for video recording)
#define EIT_RST_SOON 2
///pausing
#define EIT_RST_PAUSED 3
///running
#define EIT_RST_RUNNING 4
///service off-air
#define EIT_RST_OFF 5
uint8_t evtGetRst (uint8_t * ev_start); ///<get running status

/**
 *\brief get the programme Identification Label for the event
 *
 *\return the label, or PIL_NONE if no label is available for the event
 */
uint32_t evtGetPIL (uint8_t * ev_start);

uint8_t evtPILDay (uint32_t pil);
uint8_t evtPILMonth (uint32_t pil);
uint8_t evtPILHour (uint32_t pil);
uint8_t evtPILMinute (uint32_t pil);
#define PIL_TC   0x00007fff     ///<TIMER CONTROL: ignore PIL, use timer
#define PIL_RIT  0x00007fbf     ///<RECORDING INHIBIT/TERMINATE: inhibit or end recording
#define PIL_INT  0x00007f7f     ///<INT: interrupt recording for a moment (what's the difference to the previous one?)
#define PIL_CONT 0x00007f3f     ///<Continuation code
#define PIL_NONE 0x000fffff     ///<No specific PIL Value

#endif
