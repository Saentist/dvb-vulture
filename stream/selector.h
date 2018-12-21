#ifndef __selector_h
#define __selector_h
#include <stdint.h>
#include "bool2.h"
#include <pthread.h>
#include "trn_strm.h"
#include "list.h"
#include "events.h"
#include "item_list.h"
#include "custck.h"

/**
 *\file selector.h
 *\brief Selects data
 *
 *use for ts packets. selects packets based on pid.<p>
 *handy for TS demultiplexing/distributing
 *Each packet gets registered in multiple queues, one for each 
 *output port.<p>
 *output ports may be registered dynamically.
 *an output port consists of an id and a set of pids to accept.
 *Multiple ports may filter the same pids. Selector is reference 
 *counting.
 */


typedef struct Selector_S Selector;
typedef struct SelPort SelPort;
/**
 *\brief selector output port data
 */
struct SelPort
{
  DListE e;                     ///<header
  uint16_t num_pids;
  uint16_t *pids;               ///<all pids to filter
  SList q;                      ///<the queue
  Selector *backptr;            ///<points back. may need another lock to protect this?
  void (*packet_here) (SelPort * p, void *dta, void *pk);       ///<may be NULL. used to notify clients packet-sync. selector is locked here.
  void *dta;                    ///<may be NULL. passed to func above.
};

/**
 *\brief elements residing inside selector queue
 */
typedef struct
{
  SListE e;                     ///<header
  void *data;                   ///<points to ref counted data packet(read only). Multiple elements may point to same one. Element's reference count reflects their number.
} SelE;

/**
 *\brief main demux structure
 *
 */
struct Selector_S
{
  DList ports;                  ///<main list of all output ports
  ItemList payload;             ///<allocate payload blocks
  ItemList elements;            ///<allocate queue elements
  uint32_t num_buffers, limit;
  pthread_mutex_t mx;           ///<serializes access
  void (*pid_change) (uint16_t * add_set, uint16_t sz_add, uint16_t * rmv_set, uint16_t sz_rmv, void *dta);     ///< used to update frontend status. for fixed pids
  void *dta;                    ///<user ptr for callbacks.
};

/**
 *initializes distributor with empty queue
 *
 *\param s the selector to initialize
 *\param limit upper bound on allocated elements (payload data blocks, not queue elements)
 *\param pid_change function to call when pids change
 *\param dta parameter for callback
 *\return 1 on error 0 otherwise
 */
int selectorInit (Selector * s,
                  void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                      uint16_t * rmv_set, uint16_t rmv_size,
                                      void *dta), void *dta, uint32_t limit);
/**
 *clear Selector by releasing all pending elements
 */
void selectorClear (Selector * d);

int CUselectorInit (Selector * d,
                    void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                        uint16_t * rmv_set, uint16_t rmv_size,
                                        void *dta), void *dta, uint32_t limit,
                    CUStack * s);
void CUselectorClear (void *d);

///returns 1 if no elements are in queue
int selectorIsEmpty (Selector * d);

/**
 *\brief reinitialize selector
 *
 *used when tuning to remove all packets and pids. 
 *after getting a tuned event clients may need to reopen pids using ModPort. 
 *won't call pid_change. 
 */
int selectorReinit (Selector * d);

/**
 *\brief set up another output port
 *
 *\param d the selector to use
 *\param pids the pids to use
 *\param num_pids the size of the pids array
 *\return a pointer to the port to be used with other functions
 */
DListE *selectorAddPort (Selector * d);
DListE *selectorAddPortUnsafe (Selector * d);

///same as above but for synchronous delivery
DListE *selectorAddPortSync (Selector * d,
                             void (*packet_here) (SelPort * p, void *dta,
                                                  void *pk), void *dta);
DListE *selectorAddPortSyncUnsafe (Selector * d,
                                   void (*packet_here) (SelPort * p,
                                                        void *dta, void *pk),
                                   void *dta);

/**
 *\brief modify pids on output port
 *
 *Modify the port to filter only specified pids
 *
 */
int selectorModPort (DListE * port, uint16_t n_pids, uint16_t * pids);
int selectorModPortUnsafe (DListE * port, uint16_t n_pids, uint16_t * pids);

/**
 *\brief get the pids of a port
 */
uint16_t *selectorPortGetPids (DListE * port, uint16_t * num_pids);

/**
 *\brief removes(destroys) output port
 *
 *port must not be used afterwards
 */
int selectorRemovePort (DListE * port);

/**
 *\brief feed data item into buffer
 */
int selectorFeed (Selector * d, void *p);

/**
 *\brief retrieves an element off an output port
 */
void *selectorReadElement (DListE * port);
/**
 *\brief free an element
 *
 *receiver must do this exactly once for each processed element
 *
 *\see item_release()
 */
void selectorReleaseElement (Selector * d, void *p);

/**
 *\brief Queue status retrieval function
 *
 *\return 1 if data is available at port port, 0 otherwise
 *\warning not protected by mutex
 */
int selectorDataAvail (DListE * port);

/**
 *\brief get a new data item (for feed_selector)
 */
void *selectorAllocElement (Selector * d);

/**
 *\brief does a blocking wait for data
 *
 *does polling until timeout or element available.
 *\param timeout_ms the timeout in milliseconds
 *\return 0 if data is available, something else on timeout or error
 */
int selectorWaitData (DListE * port, int timeout_ms);

/**
 *\brief queries active pids
 *
 *\return array containing all filtering pids
 */
uint16_t *selectorGetPids (Selector * b, uint16_t * num_ret);

///send an event with associated data (size must not be larger than TS_LEN-2)
#define selectorSndEvtSz(_SEL,_EVT,_DTA) selectorSndEvt(_SEL,_EVT,&(_DTA),sizeof(_DTA))
///send just an event
#define selectorSndEv(_SEL,_EVT) selectorSndEvt(_SEL,_EVT,NULL,0)
///send event+data
int selectorSndEvt (Selector * b, uint8_t evt, void *d, size_t sz);

int selectorLock (Selector * s);
int selectorUnlock (Selector * s);
void *selectorAllocElementUnsafe (Selector * d);
int selectorFeedUnsafe (Selector * d, void *p); ///<doesn't synchronize and doesn't signal availability
void *selectorReadElementUnsafe (DListE * port);
void selectorReleaseElementUnsafe (Selector * d, void *p);
int selectorSignalUnsafe (Selector * d);        ///<signals availability to readers

typedef struct
{
  uint32_t type;                ///<1: event, 0: data
  union
  {
    DvbEvtS event;
    uint8_t data[TS_LEN];
  };
} SelBuf;

/**
 *\brief removes elements with wrong pid from queue
 *
 *\warning must be called with mutex already held
 */
void selPortRSU (DListE * port);
/**
 *\brief returns true if element contains event
 */
#define selectorEIsEvt(_P) ((SelBuf *)(_P))->type

/**
 *\brief returns pointer to elements' (ts)payload
 */
#define selectorEPayload(_P) (((SelBuf *)(_P))->data)
///reverse
#define selectorPElement(_P) (((uint8_t *)(_P))-offsetof(SelBuf,data))


#endif
