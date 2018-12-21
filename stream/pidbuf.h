#ifndef __pidbuf_h
#define __pidbuf_h

#include <stdint.h>
/**
 *\file pidbuf.h
 *\brief operations on pid buffers
 *
 *these routines are slow so one is best advised not to use them too frequently
 */
#define pidbufMergeUniq(a_,b_,c_,d_,e_)\
dmalloc_pidbufMergeUniq(a_,b_,c_,d_,e_,__FILE__,__LINE__)
#define pidbufAddPid(a_,b_,c_)\
dmalloc_pidbufAddPid(a_,b_,c_,__FILE__,__LINE__)
#define pidbufRmvPid(a_,b_,c_)\
dmalloc_pidbufRmvPid(a_,b_,c_,__FILE__,__LINE__)
#define pidbufDifference(a_,b_,c_,d_,e_)\
dmalloc_pidbufDifference(a_,b_,c_,d_,e_,__FILE__,__LINE__)
#define pidbufDup(a_,b_)\
dmalloc_pidbufDup(a_,b_,__FILE__,__LINE__)


///duplicates a pidbuf. returns NULL for num_a==0 or error
uint16_t *dmalloc_pidbufDup (uint16_t * a, uint16_t num_a,
                             char *file, int line);


uint16_t *dmalloc_pidbufMergeUniq (uint16_t * a, uint16_t num_a, uint16_t * b,
                                   uint16_t num_b, uint16_t * num_ret,
                                   char *file, int line);
///adds a pid if it is not already contained. returns NULL on failure (out of mem).
uint16_t *dmalloc_pidbufAddPid (uint16_t pid, uint16_t ** pidbuf,
                                uint16_t * num_pids, char *file, int line);
///returns NULL if pid not found, else the reallocated buffer
uint16_t *dmalloc_pidbufRmvPid (uint16_t pid, uint16_t ** pidbuf,
                                uint16_t * num_pids, char *file, int line);
///returns pids in a but not in b (set operation A-B). Returns NULL for empty set
uint16_t *dmalloc_pidbufDifference (uint16_t * a_buf, uint16_t a_sz,
                                    uint16_t * b_buf, uint16_t b_sz,
                                    uint16_t * num_ret, char *file, int line);

int pidbufContains (uint16_t v, uint16_t * array, uint16_t array_size);
void pidbufDump (char *pfx, uint16_t * pids, uint16_t sz);
#endif
