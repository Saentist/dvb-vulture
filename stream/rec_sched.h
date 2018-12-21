#ifndef __rec_sched_h
#define __rec_sched_h
#include <stdint.h>
#include <time.h>
#include "rcfile.h"
#include "rs_entry.h"
#include "list.h"
/**
 *\file rec_sched.h
 *\brief recording schedule datastructures and functions
 *
 *
 */

//typedef RsEntry RecSched[REC_SCHED_SIZE];
typedef DList RecSched;

typedef struct
{
  RecSched sched;
  char *file_name;
} RecSchedFile;

/**
 *\brief read schedule from disk or reinit
 */
int rsInit (RecSchedFile * sch, RcFile * conf);

/**
 *\brief write entries to disk and clear
 */
void rsClear (RecSchedFile * sch);

/**
 *\brief write all entries to disk
 */
int rsWrite (RecSchedFile * sch);

//RsEntry *rsGet(RecSchedFile *sch,int idx);

int rsInsertEntry (DList * sch, DListE * e);

#endif
