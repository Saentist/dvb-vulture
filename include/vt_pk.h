#ifndef __vt_pk_h
#define __vt_pk_h
#include <stdint.h>
/**
 *\file vt_pk.h
 *\brief vt packet utility routines
 */
#define VT_UNIT_LEN 0x2c
///get index of first byte of triplet. _N can be used with macro below to get a pointer. One based, so triplet 1 starts right after designation code byte.
#define VT_PK_TRIP_IDX(_N) ((_N)*3+4)
///get ptr to byte at idx. One based. Compensates for missing clock run-in and added line offset etc. needs pointer
#define VT_PK_BYTE(_PK_PTR,_IDX) (((uint8_t*)(_PK_PTR))+(_IDX)-2)
///get trip ptr. byte addressable.
#define VT_PK_TRIP(_PK_PTR,_N) VT_PK_BYTE(_PK_PTR,VT_PK_TRIP_IDX(_N))
/**
 *\brief structure holding vt packet
 */
typedef struct
{
//      uint16_t tsid;///<the originating ts
//      uint16_t pnr;///<program number
  uint16_t pid;                 ///<elementary pid
  uint8_t data_id;              ///<vt data id
  uint8_t data_unit_id;         ///<identifies vt/vps/wss/stuffing etc.
  uint8_t data_unit_len;        ///<size of payload
  uint8_t data[VT_UNIT_LEN];    ///<payload
} VtPk;

#define EX_BUFSZ sizeof(VtPk)
uint8_t bitrev (uint8_t d);
uint8_t get_pk_x (uint8_t * data);
uint8_t get_pk_y (uint8_t * data);
uint8_t get_pk_ext (uint8_t * data);
uint8_t get_magno (uint8_t * data);
uint8_t get_pgno (uint8_t * data);
uint16_t get_subno (uint8_t * data);
uint8_t is_svc_glob (uint8_t * data);
uint8_t is_mag_glob (uint8_t * data);
uint8_t is_pg_hdr (uint8_t * data);
int has_c4 (uint8_t * data);
///returns the bit-reversed and hamming8/4 corrected value or 0xff on error
uint8_t hamm84_r (uint8_t v);
uint8_t parity8 (uint8_t v);
//int hamm16(uint8_t *p, int *err);
int hamm24 (uint8_t * p, int *err);
int chk_parity (uint8_t * p, int n);
int vtpkRcv (int fd, VtPk * p);
int vtpkSnd (int fd, VtPk * p);
#endif
