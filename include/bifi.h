#ifndef __bifi_h
#define __bifi_h
#include <stdint.h>

/**
 *\file bifi.h
 *\brief large bit field manipulation routines
 */

///the bitfield type is really just an array of bytes, used for declared fields, not suitable for dyn alloc
typedef uint8_t bifi[];

///allocate bitfield of specified size
uint8_t *bifiAlloc (uint32_t size_in_bits);

///declares bitfield of specified size and name
#define BIFI_DECLARE(BIFI_NAME,BIFI_SIZE_IN_BITS) uint8_t BIFI_NAME[(((BIFI_SIZE_IN_BITS)+7)>>3)]

///initializes field to all zeroes
void bifiInit (bifi field, uint32_t size_bytes);
///initializes field to zeroes using sizeof() must only be used on array type
#define BIFI_INIT(BIFI) bifiInit(BIFI,sizeof(BIFI))

///see if a bit is set
uint8_t bifiGet (bifi field, uint32_t bit_idx);
///set a bit
void bifiSet (bifi field, uint32_t bit_idx);
///clear a bit
void bifiClr (bifi field, uint32_t bit_idx);

///and two fields
#define BIFI_AND(DEST,A,B) bifiAnd(DEST,A,B,sizeof(DEST))
///or two fields
#define BIFI_OR(DEST,A,B) bifiOr(DEST,A,B,sizeof(DEST))
///xor two fields
#define BIFI_XOR(DEST,A,B) bifiXor(DEST,A,B,sizeof(DEST))
///returns zero if all bits are zero
#define BIFI_IS_NONZERO(FIELD) bifiIsNonzero(FIELD,sizeof(FIELD))

///and two fields. a,b,dest _must_ all have same size
void bifiAnd (bifi dest, bifi a, bifi b, uint32_t size_bytes);
///or two fields. a,b,dest _must_ all have same size
void bifiOr (bifi dest, bifi a, bifi b, uint32_t size_bytes);
///xor two fields. a,b,dest _must_ all have same size
void bifiXor (bifi dest, bifi a, bifi b, uint32_t size_bytes);
///returns zero if all bits are zero
uint8_t bifiIsNonzero (bifi field, uint32_t size_bytes);

#endif
