#ifndef __svt_h
#define __svt_h

#include<stdint.h>
#include"list.h"
#include"table.h"
#include"sec_ex.h"
#include"selector.h"
#include"btre.h"


/**
 *\file svt.h
 *\brief svc_tracker to track service pids(for one service)
 *
 *each out port may have several pnrs
 *different out ports may have same pnr attached
 *but with different funcs
 *
 *when pmt packet arrives, svt gets a copy
 *if pmt gets changed, it'll iterate thru the list of 
 *output ports and will update their pids.
 *
 *when an output is removed by a client, will iterate the
 *tree and remove the output pointers. if all outputs are gone for a tracker, 
 *deallocate/destroy tracker.
 *
 *when an output is added, iterate over it's pnr/func list
 *and lookup existing/create new trackers to add outputs.
 *
 */


///0x06..private data.. vt and spu are supposedly here..
//#define FUNC_PVT6   (1<<0)
///all audio ((t==3)||(t==4)||(t==0xf)||(t==0x11))
#define FUNC_AUDIO  (1<<1)
///all video ((t==1)||(t==2)||(t==0x10))
#define FUNC_VIDEO  (1<<2)
///0x15-0x7f itu reserved
//#define FUNC_ITURES (1<<3)
///0x80-0xff private
//#define FUNC_PVT    (1<<4)
///all the others 0x00,0x05,0x07(MHEG),0x08-0x0E,0x12,0x13,0x14,
//#define FUNC_OTHER  (1<<5)
///may be problematic
#define FUNC_VT     (1<<6)
///may be problematic
#define FUNC_SPU    (1<<7)
///add pat,sdt,and pmt to stream..
#define FUNC_PSI    (1<<8)

/*perhaps do this differently and allow for VT+SUB+AUD+VID and 
aswell as /all/ streams(there's more than just those 4)
like: #define FUNC_ALL (1<<4)
and:  #define FUNC_VSAV (FUNC_VT|FUNC_SUB|FUNC_AUDIO|FUNC_VIDEO)
will this work with my set operations?
I am oring and anding those things, so I might want those 
bits to map to disjoint sets of pids..
or a complete partition...
but in the past I have distinguished VT and SUBTITLE pids based on 
presence of their descriptors
...
it's 0x06 for subtitles
no Idea wotsit for vtext/vbi etc...
ah.. also 0x06...
partition as follows:

audio:
0x3,0x4,0xf,0x11,
video:
0x01,0x02,0x10,
SPU:
sub desc and 0x06
VT:
vt desc and 0x06
hrmpfh.. what if both descriptors present..?
or just forward all 0x6, choose later?
yeah.. otherwise
might not work fo vt,sub...
let's just see what happens... 

BUG: sync callback for PMT packets does not mean 
sync pid switching, because dvb modules grabs chunks
of upto 256 ts-packets. by the time it gets fed to 
selector, it may be too late already..
*/

#define FUNC_ALL 0xffff



typedef struct SvcTrk SvcTrk;
typedef struct SwDmx SwDmx;

/*
one for each pnr/function of output..
*/
typedef struct
{
  uint16_t pnr;                 ///<the pnrs
  uint16_t func;                ///<determines subset to output
  SvcTrk *trk;                  ///<points to tracker associated with this pnr.
} FuncRec;

typedef struct
{
  DListE e;                     ///<is in outputs list of SwDmx structure
  DListE *output;               ///<points to output port. pids themselves are somewhere in there..
  uint16_t n_hpids;             ///<number of hpids
  uint16_t *hpids;              ///<pids that are not associated with a service
  unsigned n_funcs;             ///<how large below array is
  FuncRec *r;                   ///<array of records holding func/pnr data and tracker references..
  SwDmx *dmx;                   ///<top level object
} SvcOut;


/**
 *\brief tracks a service
 *
 *starts out with pnr set to it's service. pmt and pmt_next are NULL.
 *there's at least one output port associated with it.
 *If pmt or pmt_next arrives and they are new versions or no tables present,
 *change set of pids.
 *remove previous set of svc pids and replace with set of pmts'.
 */
struct SvcTrk
{
  BTreeNode n;                  ///<in svt_root tree of SwDmx obect. ssorted by svc id
  uint16_t pnr;                 ///<identifier
  uint16_t pid;                 ///<pmt pid
  DListE *input;                ///<packets with pmt pid comes in here.. perhaps also pat..
  Table *pmt[2];                ///<pmts: [0]=current [1]=next
  unsigned n_out;               ///<size of array below(number of elements)
  SvcOut **output;              ///<outputs the packets on these ports. Uses it to change pids. just ptrs here, may be shared by multiple SvcTrk objects.
  SecEx ex;
};

struct SwDmx
{
  BTreeNode *svt_root;
  DList outputs;                //TODO: mutex for this... or do it up the call chain..(in dvb module)
  Selector s;
};

SvcOut *svtOutAdd (SwDmx * dmx);
void svtOutRmv (SvcOut * o);
int svtOutMod (SvcOut * o,
               uint16_t n_funcs, uint16_t * pnrs, uint16_t * funcs,
               uint16_t n_pids, uint16_t * pids);
DListE *svtOutSelPort (SvcOut * o);
Selector *svtOutSelector (SvcOut * o);

int CUswdmxInit (SwDmx * dmx,
                 void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                     uint16_t * rmv_set, uint16_t rmv_size,
                                     void *dta), void *dta, uint32_t limit,
                 CUStack * s);
void CUswdmxClear (void *d);

int swdmxInit (SwDmx * dmx,
               void (*pid_change) (uint16_t * add_set, uint16_t add_size,
                                   uint16_t * rmv_set, uint16_t rmv_size,
                                   void *dta), void *dta, uint32_t limit);
void swdmxClear (SwDmx * dmx);


#define svtOutWaitData(o_,t_)  selectorWaitData(svtOutSelPort(o_),t_)
#define svtOutReadElement(o_)  selectorReadElement(svtOutSelPort(o_))
#define svtOutReleaseElement(o_,d_)  selectorReleaseElement(svtOutSelector(o_),d_)
#define svtEPayload(d_) selectorEPayload(d_)

#endif
