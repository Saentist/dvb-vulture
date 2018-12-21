#ifndef __trn_strm_h
#define __trn_strm_h
#include <stdint.h>
/**
 *\file trn_strm.h
 *\brief some low-level transport stream routines
 */

#define SDT_PID 0x0011
#define NIT_PID 0x0010
#define PAT_PID 0x0000
#define SIT_PID 0x001f
#define DIT_PID 0x001e
#define TS_LEN 188
#define TS_SYNC_BYTE 0x47
#define TS_PTR(_P,_I) ((_P)+(TS_LEN*(_I)))

uint16_t tsGetPid (uint8_t * ts);
int tsIsStuffing (uint8_t * d);
uint8_t tsGetAFC (uint8_t * ts);
uint8_t tsGetPUSI (uint8_t * ts);
#endif
