#ifndef __iterator_h
#define __iterator_h
#include <stdint.h>
#include "btre.h"
#include "list.h"

/**
 *\file iterator.h
 *\brief defines a rather abstract base for an unspecific iterator
 *
 *will be used to iterate over lists and trees
 */

#define I_RAND 0                ///<random access possible(by index)
#define I_SEQ_REV 1             ///<only reverse sequential access permitted(lists)
#define I_SEQ_FWD 2             ///<only forward sequential access permitted(lists)
#define I_SEQ_BOTH 3            ///<sequential access permitted(lists)

///a sequential iterator interface
typedef struct
{
  void (*first_element) (void *data);   ///<set to first element
  void (*last_element) (void *data);    ///<set to last element
  int (*advance) (void *data);  ///<advance by 1
  int (*rewind) (void *data);   ///<rewind by 1
} IteratorSeq;

///a random access iterator interface
typedef struct
{
  int (*idx_element) (void *data, uint32_t idx);        ///<select current element by index
    uint32_t (*get_idx) (void *data);   ///<return current index
    uint32_t (*get_size) (void *data);  ///<return size
} IteratorRand;

///the iterator struct. don't use directly but with functions below
typedef struct
{
  void *data;                   ///<iterator data
  uint8_t kind;                 ///<what this iterator can do
  union
  {
    IteratorSeq s;              ///<sequential iterator interface. valid if kind is I_SEQ_REV I_SEQ_FWD I_SEQ_BOTH
    IteratorRand r;             ///<random acces iterator interface. valid if kind is I_RAND
  };

  void *(*get_element) (void *data);    ///<get current element

  void (*i_destroy) (void *data);       ///<free iterator data
} Iterator;

///initialize an iterator with a btree. gives random access iterator
int iterBTreeInit (Iterator * i, BTreeNode * root);
///initialize an iterator with SList. gives I_SEQ_FWD iterator
int iterSListInit (Iterator * i, SList * l);
///initialize an iterator with DList. gives I_SEQ_BOTH iterator
int iterDListInit (Iterator * i, DList * l);
///clears iterator
int iterClear (Iterator * i);

///sets iterator to first element. Works with I_SEQ_FWD and I_SEQ_BOTH iterators
int iterFirst (Iterator * i);
///sets iterator to last element. Works with I_SEQ_REV and I_SEQ_BOTH iterators
int iterLast (Iterator * i);
///advance one element element. Works with I_SEQ_FWD and I_SEQ_BOTH iterators
int iterAdvance (Iterator * i);
///go back one element. Works with I_SEQ_REV and I_SEQ_BOTH iterators
int iterRewind (Iterator * i);

///set the iterators index. Works with I_RAND iterator
int iterSetIdx (Iterator * i, uint32_t idx);
///get the iterators index. Works with I_RAND iterator
uint32_t iterGetIdx (Iterator * i);
///get the iterators size(last_index+1). Works with I_RAND iterator
uint32_t iterGetSize (Iterator * i);

///get the pointer to the current element. works with any iterator, returns payload
void *iterGet (Iterator * i);

#endif
