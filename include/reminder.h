#ifndef __reminder_h
#define __reminder_h
#include <time.h>
#include "bool2.h"
#include <stdint.h>
#include "list.h"
#include "dvb_string.h"
#include "rs_entry.h"           //for RSType

/**
 *\file reminder.h
 *\brief Reminders for automatic tuning
 *
 *
 */

typedef struct
{
  RSType type;
  uint8_t wd;
  time_t start;
  time_t duration;
  uint32_t pos;                 ///<satellite position
  uint32_t freq;                ///<transponder frequency
  uint8_t pol;                  ///<polarisation
  uint16_t pnr;                 ///<program number
  DvbString service_name;
  char *event_name;
} Reminder;

DListE *reminderNew (RSType type,
                     uint8_t wd,
                     time_t start,
                     time_t duration,
                     uint32_t pos,
                     uint32_t freq,
                     uint8_t pol, uint16_t pnr, DvbString * svc, char *evt);
int reminderDel (DListE * rem);
int reminderAdd (DList * l, DListE * rem);
bool reminderIsReady (Reminder * rem);
//DListE *reminderIsReady(DList *l);
///remove and del or just relink reminder depending on type. returns 1 if deleted
int reminderDone (DList * l, DListE * rem);
///remove/relink past events
int reminderListUpdate (DList * l, time_t t);
int reminderListWrite (FILE * f, DList * l);
int reminderListRead (FILE * f, DList * l);
#endif
