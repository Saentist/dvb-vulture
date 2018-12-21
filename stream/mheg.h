#ifndef __mheg_h
#define __mheg_h

#include "rcfile.h"
#include "dvb.h"
#include "pgmdb.h"
#include "task.h"
#include "assoc.h"



/*
	obj carousels:
	obj carousel consists of multiple _data_ carousels
	each logical data carousel gets it's own directory..
	one log data carousel may span multiple pids(ouch)...
	although modules seem to be located in one pid only..(is this true?)
	
	module version in DII
	
	check all arrays in BIOP download... to avoid overruns
	
	structure carousels like follows:
	Carousel
	owns several DataCarousel
	owns root dir
	
	dataCarousel
	owns several CarouselPids
	and directories
	
	carPids hold the individual downloaders... unfinished modules
	
	
	use two versions for persistence:
	one directory with current version(completely downloaded)
	
	one temp dir with possibly new version.
	so I can swap atomically
	
	MhegSvc contains pointers to one or more Carousels
	
	how to structure the directories?
	tsid/carousel_id(naah carousel id unique within program..)
	
	umm...
	
	B.2.10
	Delivery of carousels within multiple services
	Carousels shall be considered identical if, in the PMTs of the services, all the following hold:
	Either:
	a)
	Both services are delivered within the same transport stream; and
	- Both services list the boot component of the carousel on the same PID.
	- The carousel_identifier_descriptor for the carousel are identical in both services (so the carousels have
	 the same carousel_Id and boot parameters).
	 - All association tags used in the carousel map to the same PIDs in both services.
	 In this case, the carousel is transmitted over a single path, but the services are allowed to reference the carousel via a
	 number of routes, including deferral to a second PMT via deferred association tags.
	 Or:
	 b)
	 Both services are delivered over multiple transport streams; and
	 - The carousel_id in the carousel_identifier_descriptor is in the range of 0x100 - 0xffffffff (containing the
	  broadcaster's organisation_id in the most significant 24 msbs of carousel_id).
	  - The carousel_identifier_descriptor for the carousel are identical in both services (so the carousels have
	   the same carousel Id and boot parameters).
	   
	*shudder*
	probably best to have tsid/svc_id and use redundant copies...
	might be able to download data cars seperately and symlink into svc dirs
	(I think this is what kilvington is doing..)
	
	but he assumes it is always rooted at nid which it is not..
	in the second case, the association tags are problematic...
	perhaps better avoid it
*/

typedef struct MhegSvc MhegSvc;

/* data about each module */
struct module
{
  uint16_t module_id;
  uint32_t download_id;
  uint8_t version;
  uint16_t block_size;
  uint16_t nblocks;
  uint32_t blocks_left;         /* number of blocks left to download */
  bool *got_block;              /* which blocks we have downloaded so far */
  uint32_t size;                /* size of the file */
  unsigned char *data;          /* the actual file data */
};

/*
one for each distinct cmp_tag in svc
owned by a service as the component tags are service specific..
and carousels DSI/DII contain references to those
Carousels(per service) creates links
from service gateway(DSI&DII).. i think there may be more than one of those, too..
*/
typedef struct
{
  Task t;
  //do I *need* the carousel_id or is pid sufficient?.. Oh, the carousel_id is used in obj references.. so, yes
  //they are sorted by pid though, and duplicate entries are not permitted...
  /*have to do different.. references are bound to service via comp tags, so carousel may look different when 
     dl'ded from different services */
  uint32_t carousel_id;
  uint16_t boot_pid;
  uint16_t *pids;               ///<dsmcc pids
  uint16_t npids;
  MhegSvc *backptr;
  uint32_t nmodules;
  bool got_dsi;
  uint32_t dsi_tid;             //transaction id, redownload on change
  uint32_t dii_tid;             //transaction id, for dii.. may need multiple..
  struct module *modules;
} Carousel;

typedef struct Mheg Mheg;


///one object per network Id
typedef struct
{
  uint16_t id;
  BTreeNode *svc;               ///<holds mheg service downloaders
  Mheg *backptr;
} MhegNid;




/**
 *\brief for one service
 *
 *might have several applications in the AIT
 *
 */
struct MhegSvc
{
  Task t;                       ///tracks pat...
  Task t2;                      ///tracks pmt(if pid known)...
  int32_t pmt_pid;
  int pmt_ver;                  ///<remember current version to track updates
  uint16_t pnr;
  MhegNid *backptr;
  int32_t video_pid;
  int32_t video_type;
  int32_t audio_pid;
  int32_t audio_type;
  struct assoc assoc;           ///<cmp_tags to pids..
  BTreeNode *ocars;             ///<object carousels (boot) sorted by pid
};

struct Mheg
{
  Task t;
  int niceness;
  int tuned;
  BTreeNode *root;
  MhegNid *current_onid;

  dvb_s *dvb;                   ///<for requesting input
  PgmDb *pgmdb;
  SvcOut *o;
};

int mhegInit (Mheg * mheg, RcFile * c, dvb_s * dvb, PgmDb * pgmdb);
int CUmhegInit (Mheg * mheg, RcFile * c, dvb_s * dvb, PgmDb * pgmdb,
                CUStack * s);
void mhegClear (Mheg * mheg);
int mhegStart (Mheg * mheg);
int mhegStop (Mheg * mheg);
int mhegGet (Mheg * mheg, uint16_t onid, uint16_t pnr,
             char **ret, uint16_t * num_ret);

int mhegIdle (Mheg * mheg);
void add_dsmcc_pid (Carousel * s, uint16_t pid);

#endif
