#ifndef __epg_ts_h
#define __epg_ts_h
#include <time.h>
#include <stdint.h>

/**
 *\file epg_ts.h
 *\brief epgdb programme schedule data, one per transport stream
 */


/**
 *\brief per ts events array
 *
 */
typedef struct
{
  union
  {
    uint64_t key;
    struct
    {
      uint32_t tsid;            //<the transport stream id
      uint32_t pos;             ///<the satellite position
    };
  };
  bool od;                      ///<if true root is not valid and root_p is
  union
  {
    FpPointer root_p;           ///<pgms on disk
    BTreeNode *root;            ///<holds EpgPgms
  };
} EpgTs;

#endif
