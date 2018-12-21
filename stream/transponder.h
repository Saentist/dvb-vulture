#ifndef __transponder_h
#define __transponder_h
#include <stdint.h>
#include "table.h"
#include "btre.h"

/**
 *\file transponder.h
 *\brief transponder related information used in pgmdb
 */

typedef struct Transponder_s Transponder;

///enum used to index transponder and pgmdb tbl and tbl_next
typedef enum
{
  T_PAT = 0,
  T_SDT = 1
} TpTbl;

/**
 *\brief holds data for transponder
 *
 *frequency and polarization should be unique for each different transponder
 *this _has_ to have the same layout as TranspondeInfo, 'coz I am too lazy to copy it element-wise.
 */
struct Transponder_s
{
  uint32_t frequency;           ///<true frequency in 10 kHz units for dvb-s, 10 Hz units for dvb-t
  uint8_t pol;                  ///<signal polarization

  int32_t ftune;                ///<fine tune signed offset to real frequency value same units as frequency
  uint32_t srate;               ///<symbol rate
  uint8_t fec;                  ///<forward error corr
  uint16_t orbi_pos;
  uint8_t east;
  uint8_t roff;
  uint8_t msys;
  uint8_t mtype;

  uint8_t bw;
  uint8_t pri;
  uint8_t mp_fec;
  uint8_t constellation;
  uint8_t hier;
  uint8_t code_rate_h;
  uint8_t code_rate_l;
  uint8_t guard;
  uint8_t mode;
  uint8_t inv;

  uint8_t deletable;            ///<1 if this tp was generated from nit entry or add_transp() and may be removed using del_transp. 0 for tp from initial tuning file.
  uint8_t scanned;              ///<used for scanning. 1 if already scanned, 0 if not. reset to 0 after scanning

  Table *tbl[2];                ///<currently active tables SDT and PAT

//      Table * tbl_next[2];///<next tables(now in pgmdb)
};



/**
 *\brief write a single transponder to file
 */
int write_transponder (void *x, BTreeNode * v, FILE * f);

/**
 *\brief write transponders(tree) to file
 *
 */
int tp_node_write (BTreeNode * root, FILE * f);

/**
 *\brief read a single transponder from file and return as BTreeNode
 */
BTreeNode *read_transponder (void *x, FILE * f);

/**
 *\brief read transponders from file
 *
 *\return the top Node of tree
 */
BTreeNode *tp_node_read (FILE * f);

#endif
