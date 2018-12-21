#ifndef __list_h
#define __list_h
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/**
 *\file list.h
 *\brief singly and doubly linked lists
 *
 *the singly linked variant doesn't support removing elements from the end
 *whereas the doubly linked variant is symmetric
 */
typedef struct SListE_s SListE;
struct SListE_s
{
  SListE *next;                 ///<the next element in list
};

typedef struct
{
  SListE *first;                ///<for removing and inserting
  SListE *last;                 ///<for inserting only
} SList;

typedef struct DListE_s DListE;
typedef struct
{
  DListE *first;                ///<one end
  DListE *last;                 ///<other end
} DList;

struct DListE_s
{
  DListE *next;                 ///<the next element in list
  DListE *prev;                 ///<the previous element in list
};

///initializes the list
void slistInit (SList * l);
///get payload pointer
void *slistEPayload (SListE * e);
///act on every element in list. Won't access element after calling func, so it works for freeing elements.
int slistForEach (SList * l, void *v, void (*func) (void *, SListE * e));
///get but don't remove first element
SListE *slistFirst (SList * l);
///get but don't remove last element
SListE *slistLast (SList * l);
///get next element
SListE *slistENext (SListE * e);
///remove and return first element
SListE *slistRemoveFirst (SList * l);
///remove and return next element. if e is NULL gets first element
SListE *slistRemoveNext (SList * l, SListE * e);
///count nr of elements
int slistCount (SList * l);
///insert element at start of list
int slistInsertFirst (SList * l, SListE * e);
///insert element at end of list
int slistInsertLast (SList * l, SListE * e);
///insert element after another one
int slistInsertAfter (SList * l, SListE * a, SListE * b);
///returns the size to allocate for an element given payload size
size_t slistESize (size_t size_of_payload);
///write the list to file
int slistWrite (SList * l, void *x,
                int (*write) (void *x, SListE * this, FILE * f), FILE * f);
///read the list from file
int slistRead (SList * l, void *x, SListE * (*read) (void *x, FILE * f),
               FILE * f);
///return the size needed to write the list to disk
uint64_t slistWriteSize (SList * l, void *x,
                         uint64_t (*size) (void *x, SListE * this));

///initialize a doubly linked list
void dlistInit (DList * l);
///return payload pointer
void *dlistEPayload (DListE * e);
///return element pointer
#define dlistPElement(_PP) (((DListE*)(_PP))-1)
///calls func on every element. may be used for freeing elements
int dlistForEach (DList * l, void *v, void (*func) (void *, DListE * e));
///calls func on every element in reverse order
int dlistForEachRev (DList * l, void *v, void (*func) (void *, DListE * e));
///get first element
DListE *dlistFirst (DList * l);
///get last element
DListE *dlistLast (DList * l);
///get next element
DListE *dlistENext (DListE * e);
///get previous element
DListE *dlistEPrev (DListE * e);
///count nr of elements
int dlistCount (DList * l);
///remove element from list
int dlistRemove (DList * l, DListE * e);
///insert element before all others
int dlistInsertFirst (DList * l, DListE * e);
///inser element at end
int dlistInsertLast (DList * l, DListE * e);
///insert element afer another one
int dlistInsertAfter (DList * l, DListE * a, DListE * b);
///insert element before another element
int dlistInsertBefore (DList * l, DListE * a, DListE * b);
///return allocation size for an element
size_t dlistESize (size_t size_of_payload);
///return the size needed to write the list to disk
uint64_t dlistWriteSize (DList * l, void *x,
                         uint64_t (*size) (void *x, DListE * this));
///write the list to file
int dlistWrite (DList * l, void *x,
                int (*write) (void *x, DListE * this, FILE * f), FILE * f);
///read the list from file
int dlistRead (DList * l, void *x, DListE * (*read) (void *x, FILE * f),
               FILE * f);

#endif
