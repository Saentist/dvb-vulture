#include <string.h>
#include <stddef.h>             //for offsetof()
#include <inttypes.h>
#include "vt_extractor.h"
#include "utl.h"
#include "debug.h"
#include "dmalloc.h"
int DF_VTEX;
#define FILE_DBG DF_VTEX

#define ES_INIT 0
#define ES_PFX1 1
#define ES_PFX2 2
#define ES_SID0 3
#define ES_PLN0 4
#define ES_PLN1 5
#define ES_ALG0 6
#define ES_ALG1 7
#define ES_HDL  8
#define ES_STF  9
#define ES_DTID 10
#define ES_DUID 11
#define ES_DUIL 12
#define ES_BITS 13
#define ES_DUIL_SKIP 14
#define ES_BITS_SKIP 15

static const char *VT_ES_STRINGS[] = {
  "ES_INIT 0",
  "ES_PFX1 1",
  "ES_PFX2 2",
  "ES_SID0 3",
  "ES_PLN0 4",
  "ES_PLN1 5",
  "ES_ALG0 6",
  "ES_ALG1 7",
  "ES_HDL  8",
  "ES_STF  9",
  "ES_DTID 10",
  "ES_DUID 11",
  "ES_DUIL 12",
  "ES_BITS 13",
  "ES_DUIL_SKIP 14",
  "ES_BITS_SKIP 15"
};

///danger: endianness issues
#define PFX_B0 0
///danger: endianness issues
#define PFX_B1 0
///danger: endianness issues
#define PFX_B2 1
///private stream 1 stream id
#define PS1_SID 0xbd
///the bits we want to see set after packet length
#define PK_MSK 0x84

/**
 *\brief processes vt packets byte wise
 *
 * "Why so complicated ?" you might ask. The problem is that we may not get
 * the whole PES packet in one read, so I dare not assume that the 
 * vt packets are complete. Thus the state machine business.
 * As a bonus it's very selective.
 *
 * Actually we are guaranteed to get complete vt packets in a PES packet, so we could significantly simplify this
 * The PES packet however, may span multiple ts packets.
 * As we get called at ts packet intervals, we'd need to maintain some state information.
 *
 */
/*
static inline int vtExProcByte(VtExState *ex,uint8_t b,void * x,void (*process)(void *x,VtPk *buf))
{
	debugMsg("processing: %hhu, %s, %hu\n",b,struGetStr(VT_ES_STRINGS,ex->st),ex->plen);
	if(ex->st>ES_PLN1)
	{
		if(ex->plen<1)
			ex->st=ES_INIT;
		else
			ex->plen--;
	}
	switch(ex->st)
	{
		case ES_INIT:
		switch(b)
		{
			case PFX_B0:
			ex->st=ES_PFX1;
			break;
			default:
			ex->st=ES_INIT;
			break;
		}
		break;
		case ES_PFX1:
		switch(b)
		{
			case PFX_B1:
			ex->st=ES_PFX2;
			break;
			default:
			ex->st=ES_INIT;
			break;
		}
		break;
		case ES_PFX2:
		switch(b)
		{
			case PFX_B2:
			ex->st=ES_SID0;
			break;
			case PFX_B1://stay in this state as long as we only see zeroes
			ex->st=ES_PFX2;
			break;
			default:
			ex->st=ES_INIT;
			break;
		}
		break;
		case ES_SID0:
		if(b!=PS1_SID)
			ex->st=ES_INIT;
		else
			ex->st=ES_PLN0;
		break;
		case ES_PLN0:
		ex->plen=b<<8;
		ex->st=ES_PLN1;
		break;
		case ES_PLN1:
		ex->plen|=b&0xff;
//		errMsg("packet length: %hd\n",ex->plen);
		if((ex->plen+6)%184)//not integral blabla..
			ex->st=ES_INIT;
		else
		{
			ex->st=ES_ALG0;
		}
		break;
		case ES_ALG0:
		if((b&PK_MSK)==PK_MSK)
			ex->st=ES_ALG1;
		else
			ex->st=ES_INIT;
		break;
		case ES_ALG1:
		ex->st=ES_HDL;
		break;
		case ES_HDL:
		if(b==0x24)
		{
			ex->st=ES_STF;
			ex->idx=0;
		}
		else
			ex->st=ES_INIT;
		break;
		case ES_STF:
		ex->idx++;
		if(ex->idx>=0x24)
		{
			ex->idx=0;
			ex->st=ES_DTID;
		}
		break;
		case ES_DTID:
		break;
		case ES_DUID:
		if((b==0x02)|| (b==0xc0) ||(b==0x03))//non-subtitle EBU teletext or inverted teletext ... and ARTE(CSAT) has duid 3 for its teletext. Would be subtitle data. I think this should be tagged separately per packet
		{
			ex->st=ES_DUIL;
			ex->buf.data_unit_id=b;
		}
		else
		{
			ex->st=ES_DUIL_SKIP;
		}
		break;
		case ES_DUIL:
		if(b<=VT_UNIT_LEN)//some packets seem to be smaller(VPS/WSS), so we don't force exact match.
		{
			ex->buf.data_unit_len=b;
			ex->st=ES_BITS;
		}
		else
			ex->st=ES_INIT;
		break;
		case ES_BITS:
		ex->buf.data[ex->idx]=b;
		ex->idx++;
		if(ex->idx>=ex->buf.data_unit_len)
		{//complete
			process(x,&ex->buf);
			if(ex->plen>=1)
			{//another packet inside
				ex->st=ES_DUID;
				ex->idx=0;
			}
			else
			{
				ex->st=ES_INIT;
				ex->idx=0;
			}
		}
		break;
		case ES_DUIL_SKIP:
		if(b<=VT_UNIT_LEN)//some packets seem to be smaller(VPS/WSS), so we don't force exact match.
		{
			ex->buf.data_unit_len=b;
			ex->st=ES_BITS_SKIP;
		}
		else
			ex->st=ES_INIT;
		break;
		case ES_BITS_SKIP:
		ex->idx++;
		if(ex->idx>=ex->buf.data_unit_len)
		{//complete
			if(ex->plen>=1)
			{//another packet inside
				ex->st=ES_DUID;
				ex->idx=0;
			}
			else
			{
				ex->st=ES_INIT;
				ex->idx=0;
			}
		}
		break;
		default:
		ex->st=ES_INIT;
		ex->idx=0;
		break;
	}
	return 0;
}
*/
int
vtExInit (VtEx * ex, void *x, void (*process) (void *x, VtPk * buf))
{
  debugMsg ("VtExinit()\n");
  memset (ex, 0, sizeof (VtEx));
  ex->process = process;
  ex->x = x;
  return 0;
}

///we cast to VtPk later, so we need a bit more space... need to align this..
#define SCR_PFX (offsetof(VtPk,data)-3)

/**
 *process individual VT data units
 */
static int
vt_ex_data (uint8_t * b, int l, uint16_t pid, void *x,
            void (*process) (void *x, VtPk * buf))
{
  uint8_t data_id, data_unit_id, data_unit_len;
  if (l < 1)
    return 1;
  debugMsg ("data l:%d, b: %x \n", l, b);
  if ((b[0] >= 0x10) && (b[0] <= 0x1f))
  {
    data_id = b[0];
  }
  else
  {
    debugMsg ("data_id fail:%" PRIx8 "\n", b[0]);
    return 1;
  }
  b++;
  l--;
  while (l > 0)
  {
    if (l < 3)
      return 1;
    debugMsg ("%" PRIx8 " %" PRIx8 " %" PRIx8 "\n", b[0], b[1], b[2]);
    data_unit_len = b[1];
    if ((b[0] == 0x02) || (b[0] == 0xc0) || (b[0] == 0x03))     //non-subtitle EBU teletext or inverted teletext ... and ARTE(CSAT) has duid 3 for its teletext. Would be subtitle data. I think this should be tagged separately per packet
    {
      data_unit_id = b[0];
      if (data_unit_len <= VT_UNIT_LEN) //some packets seem to be smaller(VPS/WSS), so we don't force exact match.
      {
        if (data_unit_len <= (l - 2))
        {
//                                      VtPk *tmp=(VtPk*)(b+2-offsetof(VtPk,data));//needs some alignment... copying would do..
          VtPk tmpp;
          memmove (tmpp.data, b + 2, data_unit_len);
          tmpp.data_id = data_id;
          tmpp.data_unit_id = data_unit_id;
          tmpp.data_unit_len = data_unit_len;
          tmpp.pid = pid;
          process (x, &tmpp);
/*
					tmp->data_id=data_id;
					tmp->data_unit_id=data_unit_id;
					tmp->data_unit_len=data_unit_len;
					tmp->pid=pid;
					process(x,tmp);
*/
          debugMsg ("processed\n");
        }
      }
    }
    l -= data_unit_len + 2;
    b += data_unit_len + 2;
  }
  return 0;
}

/**
 *\brief processes pes packet chunk containing vt data
 *
 * gets called for each ts packet
 * collects data until pes packet is complete, then does the processing
 *
 */
static int
vtExProcPES (VtExState * ex, uint8_t * b, int l, void *x,
             void (*process) (void *x, VtPk * buf))
{
  static const uint8_t vt_ex_pfx[] = { PFX_B0, PFX_B1, PFX_B2, PS1_SID };
  debugMsg ("processing: %" PRIu16 ", %s\n", ex->pid,
            struGetStr (VT_ES_STRINGS, ex->st));
  switch (ex->st)
  {
  case ES_INIT:
    if (memcmp (b, (const void *) vt_ex_pfx, 4))
    {
      ex->st = ES_BITS_SKIP;
      debugMsg ("pfx fail\n");
      return 1;
    }
    ex->plen = (b[4] << 8) | b[5];
//              debugMsg("packet length: %hd\n", ex->plen);
    if ((ex->plen + 6) % 184)   //not integral blabla...
    {
      ex->st = ES_BITS_SKIP;
      debugMsg ("size fail\n");
      return 2;
    }
    ex->plen -= 39;             //we skip header later
    if (ex->scratch_size < ((int) (ex->plen + SCR_PFX)))
    {
      void *n;
      n = utlRealloc (ex->scratch, ex->plen + SCR_PFX);
      if (n)
      {
        ex->scratch = n;
        ex->scratch_size = ex->plen + SCR_PFX;
      }
      else
      {
        ex->st = ES_BITS_SKIP;
        debugMsg ("realloc fail\n");
        return 3;
      }
    }
    b += 45;
    l -= 45;
    ex->scratch_idx = SCR_PFX;
    ex->st = ES_BITS;
    //fallthrough
  case ES_BITS:
    if (ex->scratch_idx >= ((int) (ex->plen + SCR_PFX)))
    {
      debugMsg ("Overrun\n");
    }
    else
    {
      //           693                   690
      if ((ex->scratch_size - ex->scratch_idx) >= l)    //this overran.. not here, but below in vt_ex_data...
        memmove (ex->scratch + ex->scratch_idx, b, l);
      ex->scratch_idx += l;
      debugMsg ("idx: %d, plen: %" PRIu16 "\n", ex->scratch_idx,
                ex->plen + SCR_PFX);
      if (ex->scratch_idx >= ((int) (ex->plen + SCR_PFX)))
      {
        int sz = ex->scratch_idx;
        if (sz > ex->scratch_size)
          sz = ex->scratch_size;
        vt_ex_data (ex->scratch + SCR_PFX, sz - SCR_PFX, ex->pid, x, process);
        ex->scratch_idx = 0;
        ex->st = ES_BITS_SKIP;
      }
    }
    break;
  default:
    break;
  }
  return 0;
}

static void
vt_ex_st_destroy (void *x NOTUSED, BTreeNode * this, int which,
                  int depth NOTUSED)
{
  debugMsg ("vt_ex_st_destroy\n");
  if ((which == 2) || (which == 3))
  {
    debugMsg ("ex_st_destroy\n");
    VtExState *st = btreeNodePayload (this);
    utlFAN (st->scratch);
    debugMsg ("scratch\n");
    utlFAN (this);
    debugMsg ("this\n");
  }
}

void
vtExClear (VtEx * ex)
{
  debugMsg ("vtExClear\n");
//      debugMsg("btreeWalk\n");
  btreeWalk (ex->states, ex, vt_ex_st_destroy);
  ex->states = NULL;
  memset (ex, 0, sizeof (VtEx));
}

void
vtExFlush (VtEx * ex)
{
  debugMsg ("vtExFlush %p\n", ex->states);
//      debugMsg("btreeWalk\n");
  btreeWalk (ex->states, ex, vt_ex_st_destroy);
  ex->states = NULL;
}

int
vt_ex_state_cmp (void *x NOTUSED, const void *a, const void *b)
{
  VtExState *ap = (VtExState *) a, *bp = (VtExState *) b;
  return (bp->pid < ap->pid) ? -1 : (bp->pid != ap->pid);
}

static VtExState *
vt_ex_get_or_ins_state (BTreeNode ** root, uint16_t pid)
{
  BTreeNode *n;
  VtExState tmp;
  tmp.pid = pid;
  tmp.plen = 0;
  tmp.scratch = NULL;
  tmp.scratch_size = 0;
  tmp.st = ES_BITS_SKIP;
  n =
    btreeNodeGetOrIns (root, &tmp, NULL, vt_ex_state_cmp, sizeof (VtExState));
  if (NULL != n)
    return btreeNodePayload (n);
  return NULL;
}

void
vtExPut (VtEx * ex, uint8_t * buf, uint16_t pid, uint8_t len, uint8_t starts)
{
  VtExState *es = vt_ex_get_or_ins_state (&ex->states, pid);
  if (NULL == es)
    return;
  if (starts)
  {
//              if(es->st!=ES_INIT)
//                      errMsg("Bad Pid change\n");
//              es->idx=0;
    es->st = ES_INIT;
  }
  vtExProcPES (es, buf, len, ex->x, ex->process);

  /*
     i=0;
     while(i<len)
     {
     vtExProcByte(es,buf[i],ex->x,ex->process);
     i++;
     }
   */
}
